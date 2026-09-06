/** @file telemetry.c @brief 传感器采样、快照与周期遥测。 */
#include "telemetry.h"
#include "patrol_robot_config.h"
#include "network_config.h"
#include "bluetooth.h"
#include "gy906.h"
#include "ultrasonic.h"
#include "i2c.h"
#include "tim.h"
#include <stdio.h>
#include <string.h>

static BSP_GY906_HandleTypeDef gy906;
static BSP_Ultrasonic_HandleTypeDef ultrasonic;
static uint32_t gy906_next_sample_tick;
static uint32_t ultrasonic_next_measurement_tick;
static uint32_t bluetooth_next_status_tick;
static uint32_t bluetooth_next_line_tick;
static uint32_t mqtt_next_telemetry_tick;
static uint32_t telemetry_distance_mm;
static uint16_t telemetry_raw_ambient;
static uint16_t telemetry_raw_object;
static BSP_Ultrasonic_StatusTypeDef ultrasonic_last_result_status =
    BSP_ULTRASONIC_STATUS_NOT_INITIALIZED;
static BSP_Ultrasonic_StatusTypeDef telemetry_ultrasonic_status =
    BSP_ULTRASONIC_STATUS_NOT_INITIALIZED;
static uint8_t telemetry_distance_valid;
static uint8_t telemetry_temperature_valid;
static uint8_t bluetooth_telemetry_line;
static BSP_GY906_StatusTypeDef gy906_status = BSP_GY906_STATUS_NOT_READY;
static uint16_t gy906_raw_ambient;
static uint16_t gy906_raw_object;
static uint8_t gy906_temperature_valid;
static uint8_t gy906_recovery_count;
static uint32_t ultrasonic_distance_mm;
static uint8_t ultrasonic_sample_valid;

static void Telemetry_UpdateUltrasonicData(void);
static void Telemetry_ProcessMqtt(uint32_t now, BSP_MQTT_HandleTypeDef *mqtt);
static void Telemetry_ProcessGy906Sample(void);
static void Telemetry_RequestTelemetry(uint32_t now);
static void Telemetry_ProcessTelemetry(uint32_t now);
static void Telemetry_TransmitTemperature(const char *full_label,
                                          const char *short_label,
                                          uint16_t raw_temperature);
static const char *Telemetry_GetUltrasonicChineseStatus(
    BSP_Ultrasonic_StatusTypeDef status);

void Telemetry_Init(void)
{
  uint32_t now;
  (void)BSP_Ultrasonic_Init(&ultrasonic, &htim2);
  Telemetry_UpdateUltrasonicData();
  HAL_Delay(GY906_POWER_ON_DELAY_MS);
  gy906_status = BSP_GY906_Init(&gy906, &hi2c1);
  now = HAL_GetTick();
  gy906_next_sample_tick = now;
  ultrasonic_next_measurement_tick = now;
  bluetooth_next_status_tick = now + BLUETOOTH_STATUS_INTERVAL_MS;
  mqtt_next_telemetry_tick = now + MQTT_TELEMETRY_INTERVAL_MS;
}

void Telemetry_Process(uint32_t now,
                       BSP_MQTT_HandleTypeDef *mqtt,
                       uint8_t mqtt_publish_allowed)
{
  BSP_Ultrasonic_Process(&ultrasonic, now);
  Telemetry_UpdateUltrasonicData();
  if ((int32_t)(now - ultrasonic_next_measurement_tick) >= 0)
  {
    (void)BSP_Ultrasonic_StartMeasurement(&ultrasonic);
    ultrasonic_next_measurement_tick = now + ULTRASONIC_MEASUREMENT_INTERVAL_MS;
  }
  if ((int32_t)(now - gy906_next_sample_tick) >= 0)
  {
    Telemetry_ProcessGy906Sample();
    gy906_next_sample_tick = now + GY906_SAMPLE_INTERVAL_MS;
  }
  if ((int32_t)(now - bluetooth_next_status_tick) >= 0)
  {
    Telemetry_RequestTelemetry(now);
    bluetooth_next_status_tick = now + BLUETOOTH_STATUS_INTERVAL_MS;
  }
  Telemetry_ProcessTelemetry(now);
  if (mqtt_publish_allowed != 0U)
  {
    Telemetry_ProcessMqtt(now, mqtt);
  }
}

void Telemetry_NotifyTimInputCapture(TIM_HandleTypeDef *timer)
{
  BSP_Ultrasonic_IC_CaptureCallback(&ultrasonic, timer);
}
static void Telemetry_UpdateUltrasonicData(void)
{
  BSP_Ultrasonic_DataTypeDef data;

  if (BSP_Ultrasonic_GetData(&ultrasonic, &data) != HAL_OK)
  {
    return;
  }

  ultrasonic_distance_mm = data.distance_mm;
  ultrasonic_sample_valid = data.sample_valid;

  /* 等待边沿是中间状态，不应覆盖最近一次已经完成的测量结果。 */
  if ((data.status == BSP_ULTRASONIC_STATUS_VALID) ||
      (data.status == BSP_ULTRASONIC_STATUS_TOO_CLOSE) ||
      (data.status == BSP_ULTRASONIC_STATUS_TIMEOUT) ||
      (data.status == BSP_ULTRASONIC_STATUS_ERROR) ||
      (data.status == BSP_ULTRASONIC_STATUS_NOT_INITIALIZED))
  {
    ultrasonic_last_result_status = data.status;
  }
}


static void Telemetry_ProcessMqtt(uint32_t now, BSP_MQTT_HandleTypeDef *mqtt)
{
  char message[BSP_MQTT_MESSAGE_SIZE];
  const char *ambient_sign;
  const char *object_sign;
  uint32_t ambient_magnitude;
  uint32_t distance_centimeter;
  uint32_t object_magnitude;
  int written;
  int32_t ambient_cdeg = ((int32_t)gy906_raw_ambient * 2L) - 27315L;
  int32_t object_cdeg = ((int32_t)gy906_raw_object * 2L) - 27315L;

  /* 消费一条下行消息；控制语义尚未定义，因此不执行电机动作。 */
  (void)BSP_MQTT_TakeMessage(mqtt, message, sizeof(message));

  if ((mqtt->state != BSP_MQTT_STATE_ONLINE) ||
      (mqtt->publish_pending != 0U) ||
      ((int32_t)(now - mqtt_next_telemetry_tick) < 0))
  {
    return;
  }

  /* 距离换算为米并固定保留两位小数，温度固定保留两位小数。 */
  distance_centimeter = ultrasonic_distance_mm / 10U;
  if ((ultrasonic_distance_mm % 10U) >= 5U)
  {
    distance_centimeter++;
  }
  ambient_sign = (ambient_cdeg < 0L) ? "-" : "";
  object_sign = (object_cdeg < 0L) ? "-" : "";
  ambient_magnitude = (ambient_cdeg < 0L)
                          ? (uint32_t)(-ambient_cdeg)
                          : (uint32_t)ambient_cdeg;
  object_magnitude = (object_cdeg < 0L)
                         ? (uint32_t)(-object_cdeg)
                         : (uint32_t)object_cdeg;
  written = snprintf(message,
                     sizeof(message),
                     "{\"距离\":\"%lu.%02lum\",\"物体温度\":\"%s%lu.%02lu度\","
                     "\"环境温度\":\"%s%lu.%02lu度\",\"距离状态\":\"%s\"}",
                     (unsigned long)(distance_centimeter / 100U),
                     (unsigned long)(distance_centimeter % 100U),
                     object_sign,
                     (unsigned long)(object_magnitude / 100U),
                     (unsigned long)(object_magnitude % 100U),
                     ambient_sign,
                     (unsigned long)(ambient_magnitude / 100U),
                     (unsigned long)(ambient_magnitude % 100U),
                     Telemetry_GetUltrasonicChineseStatus(
                         ultrasonic_last_result_status));
  if ((written > 0) && ((size_t)written < sizeof(message)))
  {
    (void)BSP_MQTT_RequestPublish(mqtt, message);
  }
  mqtt_next_telemetry_tick = now + MQTT_TELEMETRY_INTERVAL_MS;
}


static void Telemetry_ProcessGy906Sample(void)
{
  BSP_GY906_DataTypeDef temperature;

  if (gy906_status != BSP_GY906_STATUS_OK)
  {
    /* 总线恢复最多执行三次，避免硬件持续异常时反复阻塞主循环。 */
    if ((gy906.last_hal_status == HAL_BUSY) &&
        (gy906_recovery_count < GY906_MAX_BUS_RECOVERIES))
    {
      (void)BSP_GY906_RecoverBus(&gy906);
      gy906_recovery_count++;
    }

    gy906_status = BSP_GY906_Init(&gy906, &hi2c1);
  }
  else
  {
    gy906_status = BSP_GY906_ReadTemperatures(&gy906, &temperature);
    if (gy906_status == BSP_GY906_STATUS_OK)
    {
      gy906_raw_ambient = temperature.raw_ambient;
      gy906_raw_object = temperature.raw_object;
      gy906_temperature_valid = 1U;
    }
  }

}


static void Telemetry_RequestTelemetry(uint32_t now)
{
  telemetry_distance_mm = ultrasonic_distance_mm;
  telemetry_raw_ambient = gy906_raw_ambient;
  telemetry_raw_object = gy906_raw_object;
  telemetry_ultrasonic_status = ultrasonic_last_result_status;
  telemetry_distance_valid =
      ((ultrasonic_sample_valid != 0U) &&
       (telemetry_ultrasonic_status == BSP_ULTRASONIC_STATUS_VALID))
          ? 1U
          : 0U;
  telemetry_temperature_valid = gy906_temperature_valid;
  bluetooth_telemetry_line = 1U;
  bluetooth_next_line_tick = now;
}


static void Telemetry_TransmitTemperature(const char *full_label,
                                               const char *short_label,
                                               uint16_t raw_temperature)
{
  char frame[BLUETOOTH_TX_BUFFER_SIZE];
  int32_t celsius_centi;
  int32_t celsius_tenth;
  uint32_t magnitude;
  const char *sign;
  int written;

  /* 原始值单位为 0.02 K，先换算为摄氏百分度，再四舍五入到十分度。 */
  celsius_centi = ((int32_t)raw_temperature * 2L) - 27315L;
  celsius_tenth = (celsius_centi >= 0L)
                      ? ((celsius_centi + 5L) / 10L)
                      : ((celsius_centi - 5L) / 10L);
  sign = (celsius_tenth < 0L) ? "-" : "";
  magnitude = (celsius_tenth < 0L)
                  ? (uint32_t)(-celsius_tenth)
                  : (uint32_t)celsius_tenth;

  written = snprintf(frame,
                     sizeof(frame),
                     "%s:%s%lu.%lu度",
                     full_label,
                     sign,
                     (unsigned long)(magnitude / 10U),
                     (unsigned long)(magnitude % 10U));
  if (written > (int)BLUETOOTH_NOTIFY_PAYLOAD_SIZE)
  {
    written = snprintf(frame,
                       sizeof(frame),
                       "%s:%s%lu.%lu度",
                       short_label,
                       sign,
                       (unsigned long)(magnitude / 10U),
                       (unsigned long)(magnitude % 10U));
  }

  if ((written > 0) &&
      (written <= (int)BLUETOOTH_NOTIFY_PAYLOAD_SIZE) &&
      ((size_t)written < sizeof(frame)))
  {
    (void)BSP_Bluetooth_Transmit(frame);
  }
}


static void Telemetry_ProcessTelemetry(uint32_t now)
{
  char frame[BLUETOOTH_TX_BUFFER_SIZE];
  int written;

  if ((bluetooth_telemetry_line == 0U) ||
      ((int32_t)(now - bluetooth_next_line_tick) < 0))
  {
    return;
  }

  if (bluetooth_telemetry_line == 1U)
  {
    if (telemetry_ultrasonic_status == BSP_ULTRASONIC_STATUS_TIMEOUT)
    {
      (void)BSP_Bluetooth_Transmit("距离:无回波");
    }
    else if (telemetry_ultrasonic_status ==
             BSP_ULTRASONIC_STATUS_TOO_CLOSE)
    {
      (void)BSP_Bluetooth_Transmit("距离:过近");
    }
    else if ((telemetry_ultrasonic_status == BSP_ULTRASONIC_STATUS_ERROR) ||
             (telemetry_ultrasonic_status ==
              BSP_ULTRASONIC_STATUS_NOT_INITIALIZED))
    {
      (void)BSP_Bluetooth_Transmit("距离:异常");
    }
    else if (telemetry_distance_valid == 0U)
    {
      (void)BSP_Bluetooth_Transmit("距离:无");
    }
    else
    {
      /* 距离按米输出最多两位小数，并去除无意义的末尾零。 */
      uint32_t distance_centimeter =
          (telemetry_distance_mm + 5U) / 10U;
      uint32_t whole_meter = distance_centimeter / 100U;
      uint32_t decimal_centimeter = distance_centimeter % 100U;

      if (decimal_centimeter == 0U)
      {
        written = snprintf(frame,
                           sizeof(frame),
                           "距离:%lum",
                           (unsigned long)whole_meter);
      }
      else if ((decimal_centimeter % 10U) == 0U)
      {
        written = snprintf(frame,
                           sizeof(frame),
                           "距离:%lu.%lum",
                           (unsigned long)whole_meter,
                           (unsigned long)(decimal_centimeter / 10U));
      }
      else
      {
        written = snprintf(frame,
                           sizeof(frame),
                           "距离:%lu.%02lum",
                           (unsigned long)whole_meter,
                           (unsigned long)decimal_centimeter);
      }
      if ((written > 0) &&
          (written <= (int)BLUETOOTH_NOTIFY_PAYLOAD_SIZE) &&
          ((size_t)written < sizeof(frame)))
      {
        (void)BSP_Bluetooth_Transmit(frame);
      }
      else
      {
        (void)BSP_Bluetooth_Transmit("距离:过远");
      }
    }
    bluetooth_telemetry_line = 2U;
  }
  else if (bluetooth_telemetry_line == 2U)
  {
    if (telemetry_temperature_valid == 0U)
    {
      (void)BSP_Bluetooth_Transmit("物体温度:无");
    }
    else
    {
      Telemetry_TransmitTemperature("物体温度",
                                         "物温",
                                         telemetry_raw_object);
    }
    bluetooth_telemetry_line = 3U;
  }
  else
  {
    if (telemetry_temperature_valid == 0U)
    {
      (void)BSP_Bluetooth_Transmit("环境温度:无");
    }
    else
    {
      Telemetry_TransmitTemperature("环境温度",
                                         "环温",
                                         telemetry_raw_ambient);
    }
    bluetooth_telemetry_line = 0U;
  }

  bluetooth_next_line_tick = now + BLUETOOTH_LINE_INTERVAL_MS;
}


static const char *Telemetry_GetUltrasonicChineseStatus(
    BSP_Ultrasonic_StatusTypeDef status)
{
  switch (status)
  {
    case BSP_ULTRASONIC_STATUS_TOO_CLOSE:
      return "过近";

    case BSP_ULTRASONIC_STATUS_NOT_INITIALIZED:
      return "未初始化";

    case BSP_ULTRASONIC_STATUS_ERROR:
      return "异常";

    case BSP_ULTRASONIC_STATUS_TIMEOUT:
      return "无回波";

    case BSP_ULTRASONIC_STATUS_IDLE:
      return "空闲";

    case BSP_ULTRASONIC_STATUS_WAIT_RISING:
    case BSP_ULTRASONIC_STATUS_WAIT_FALLING:
      return "测量中";

    case BSP_ULTRASONIC_STATUS_VALID:
      return "正常";

    default:
      return "未知";
  }
}

