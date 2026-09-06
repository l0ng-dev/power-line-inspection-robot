/**
 ******************************************************************************
 * @file    patrol_robot_app.c
 * @brief   巡检机器人应用总调度入口
 ******************************************************************************
 */
#include "patrol_robot_app.h"
#include "bluetooth_command.h"
#include "telemetry.h"
#include "vision_alert.h"
#include "esp01s.h"
#include "mqtt_client.h"
#include "network_config.h"
#include "mqtt_secret.h"
#include "usart.h"

static BSP_WiFi_HandleTypeDef esp01s;
static BSP_MQTT_HandleTypeDef mqtt;

void PatrolRobotApp_Init(void)
{
  Telemetry_Init();
  if (BSP_WiFi_Init(&esp01s, &huart5, HAL_GetTick()) == HAL_OK)
  {
#if WIFI_AUTO_CONNECT
    if ((BSP_WiFi_SetSSID(&esp01s, WIFI_DEFAULT_SSID) == HAL_OK) &&
        (BSP_WiFi_SetPassword(&esp01s, WIFI_DEFAULT_PASSWORD) == HAL_OK))
    {
      (void)BSP_WiFi_Connect(&esp01s, HAL_GetTick());
    }
#endif
  }
  if (BSP_MQTT_Init(&mqtt, &esp01s, MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                    MQTT_BASE_TOPIC, HAL_GetTick()) == HAL_OK)
  {
#if MQTT_AUTO_CONNECT
    if (BSP_MQTT_SetKey(&mqtt, MQTT_BEMFA_PRIVATE_KEY) == HAL_OK)
    {
      (void)BSP_MQTT_Connect(&mqtt, HAL_GetTick());
    }
#endif
  }
  VisionAlert_Init();
  BluetoothCommand_Init(&esp01s);
}

void PatrolRobotApp_Process(void)
{
  uint32_t now = HAL_GetTick();
  BluetoothCommand_Process();
  VisionAlert_ProcessInput();
  BSP_WiFi_Process(&esp01s, now);
  BSP_MQTT_Process(&mqtt, now);
  VisionAlert_ProcessMqtt(&mqtt);
  Telemetry_Process(now, &mqtt,
                    (uint8_t)(VisionAlert_HasPendingMqtt() == 0U));
  HAL_Delay(1U);
}

void PatrolRobotApp_NotifyUartRxComplete(UART_HandleTypeDef *uart)
{
  BluetoothCommand_NotifyUartRxComplete(uart);
  VisionAlert_NotifyUartRxComplete(uart);
}

void PatrolRobotApp_NotifyUartRxEvent(UART_HandleTypeDef *uart, uint16_t size)
{
  BSP_WiFi_RxEventCallback(&esp01s, uart, size);
}

void PatrolRobotApp_NotifyUartError(UART_HandleTypeDef *uart)
{
  BluetoothCommand_NotifyUartError(uart);
  VisionAlert_NotifyUartError(uart);
  BSP_WiFi_ErrorCallback(&esp01s, uart);
}

void PatrolRobotApp_NotifyTimInputCapture(TIM_HandleTypeDef *timer)
{
  Telemetry_NotifyTimInputCapture(timer);
}
