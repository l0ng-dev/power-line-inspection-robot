/** @file telemetry.h @brief 传感器采样和周期遥测调度接口。 */
#ifndef PATROL_ROBOT_TELEMETRY_H
#define PATROL_ROBOT_TELEMETRY_H

#include "mqtt_client.h"
#include "stm32g4xx_hal.h"

/** @brief 初始化传感器 BSP 及各周期任务的首次执行时间。 */
void Telemetry_Init(void);

/**
 * @brief 推进采样、蓝牙状态和云端遥测任务。
 * @param mqtt MQTT 客户端句柄。
 * @param mqtt_publish_allowed 非零时允许提交周期遥测，供告警消息优先发送。
 */
void Telemetry_Process(uint32_t now_ms,
                       BSP_MQTT_HandleTypeDef *mqtt,
                       uint8_t mqtt_publish_allowed);

/** @brief 将 HAL 输入捕获回调转交给超声波 BSP。 */
void Telemetry_NotifyTimInputCapture(TIM_HandleTypeDef *timer);

#endif /* PATROL_ROBOT_TELEMETRY_H */
