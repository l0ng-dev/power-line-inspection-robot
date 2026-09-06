/** @file vision_alert.h @brief K230 告警接收、缓存和上报接口。 */
#ifndef PATROL_ROBOT_VISION_ALERT_H
#define PATROL_ROBOT_VISION_ALERT_H

#include "mqtt_client.h"

/** @brief 初始化 K230 串口接收。 */
void VisionAlert_Init(void);
/** @brief 取出一条 K230 告警，并提交蓝牙提示和云端缓存。 */
void VisionAlert_ProcessInput(void);
/** @brief MQTT 空闲时提交队首告警。 */
void VisionAlert_ProcessMqtt(BSP_MQTT_HandleTypeDef *mqtt);
/** @brief 查询是否有等待提交的告警，用于确定遥测发送优先级。 */
uint8_t VisionAlert_HasPendingMqtt(void);
void VisionAlert_NotifyUartRxComplete(UART_HandleTypeDef *uart);
void VisionAlert_NotifyUartError(UART_HandleTypeDef *uart);

#endif /* PATROL_ROBOT_VISION_ALERT_H */
