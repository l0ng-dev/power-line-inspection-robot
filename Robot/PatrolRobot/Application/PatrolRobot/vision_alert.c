/** @file vision_alert.c @brief K230 视觉告警的蓝牙与云端闭环。 */
#include "vision_alert.h"
#include "patrol_robot_config.h"
#include "network_config.h"
#include "bluetooth.h"
#include "k230.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

static BSP_K230_AlertTypeDef k230_mqtt_event_queue[K230_MQTT_EVENT_QUEUE_DEPTH];
static uint8_t k230_mqtt_queue_head;
static uint8_t k230_mqtt_queue_tail;
static uint8_t k230_mqtt_queue_count;

static uint8_t VisionAlert_QueueK230MqttEvent(const BSP_K230_AlertTypeDef *alert);
static void VisionAlert_PopK230MqttEvent(void);
static const char *VisionAlert_GetK230ChineseType(const char *type);

void VisionAlert_Init(void)
{
  (void)BSP_K230_Init(&huart4);
}

uint8_t VisionAlert_HasPendingMqtt(void)
{
  return (k230_mqtt_queue_count != 0U) ? 1U : 0U;
}

void VisionAlert_NotifyUartRxComplete(UART_HandleTypeDef *uart)
{
  BSP_K230_RxCpltCallback(uart);
}

void VisionAlert_NotifyUartError(UART_HandleTypeDef *uart)
{
  BSP_K230_ErrorCallback(uart);
}
void VisionAlert_ProcessInput(void)
{
  BSP_K230_AlertTypeDef alert;
  char message[BLUETOOTH_TX_BUFFER_SIZE];
  int written;

  if (BSP_K230_TakeAlert(&alert) == 0U)
  {
    return;
  }

  written = snprintf(message, sizeof(message), "K230:%s\r\n", alert.type);
  if ((written > 0) &&
      (written <= (int)BLUETOOTH_NOTIFY_PAYLOAD_SIZE) &&
      ((size_t)written < sizeof(message)))
  {
    (void)BSP_Bluetooth_Transmit(message);
  }
  else
  {
    (void)BSP_Bluetooth_Transmit("K230:告警\r\n");
  }

  (void)VisionAlert_QueueK230MqttEvent(&alert);
}


static uint8_t VisionAlert_QueueK230MqttEvent(
    const BSP_K230_AlertTypeDef *alert)
{
  if (alert == NULL)
  {
    return 0U;
  }

  if (k230_mqtt_queue_count >= K230_MQTT_EVENT_QUEUE_DEPTH)
  {
    /* 队列满时保留先到告警，丢弃最新告警，避免覆盖尚未上报的数据。 */
    return 0U;
  }

  k230_mqtt_event_queue[k230_mqtt_queue_tail] = *alert;
  k230_mqtt_queue_tail =
      (uint8_t)((k230_mqtt_queue_tail + 1U) % K230_MQTT_EVENT_QUEUE_DEPTH);
  k230_mqtt_queue_count++;
  return 1U;
}


static void VisionAlert_PopK230MqttEvent(void)
{
  k230_mqtt_queue_head =
      (uint8_t)((k230_mqtt_queue_head + 1U) % K230_MQTT_EVENT_QUEUE_DEPTH);
  k230_mqtt_queue_count--;
}


static const char *VisionAlert_GetK230ChineseType(const char *type)
{
  if (strcmp(type, "break") == 0)
  {
    return "断裂";
  }
  if (strcmp(type, "heat") == 0)
  {
    return "热损伤";
  }
  if (strcmp(type, "wear") == 0)
  {
    return "磨损";
  }
  return "未知";
}


void VisionAlert_ProcessMqtt(BSP_MQTT_HandleTypeDef *mqtt)
{
  BSP_K230_AlertTypeDef *alert;
  char message[BSP_MQTT_MESSAGE_SIZE];
  HAL_StatusTypeDef status;
  int written;

  if ((k230_mqtt_queue_count == 0U) ||
      (mqtt->state != BSP_MQTT_STATE_ONLINE) ||
      (mqtt->publish_pending != 0U))
  {
    return;
  }

  alert = &k230_mqtt_event_queue[k230_mqtt_queue_head];
  written = snprintf(message,
                     sizeof(message),
                     "{\"事件\":\"视觉告警\",\"设备\":\"K230\",\"类型\":\"%s\"}",
                     VisionAlert_GetK230ChineseType(alert->type));
  if ((written <= 0) || ((size_t)written >= sizeof(message)))
  {
    VisionAlert_PopK230MqttEvent();
    return;
  }

  status = BSP_MQTT_RequestPublishToTopic(mqtt,
                                          MQTT_K230_EVENT_TOPIC,
                                          message);
  /* HAL_OK 表示 MQTT 已接管消息，实际 TCP 发送结果由客户端状态机继续跟踪。 */
  if (status == HAL_OK)
  {
    VisionAlert_PopK230MqttEvent();
  }
  else if (status != HAL_BUSY)
  {
    VisionAlert_PopK230MqttEvent();
  }
}

