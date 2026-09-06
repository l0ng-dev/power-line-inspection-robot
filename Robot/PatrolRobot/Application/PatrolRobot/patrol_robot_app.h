/**
 ******************************************************************************
 * @file    patrol_robot_app.h
 * @brief   巡检机器人应用层接口
 *
 * @details
 * 主要功能：
 * 1. 初始化巡检机器人应用层。
 * 2. 提供周期调度入口和 HAL 回调转发接口。
 *
 * 模块关系：
 * main.c 通过本接口启动并周期调用应用层。
 *
 * 主要接口：
 * PatrolRobotApp_Init()、PatrolRobotApp_Process()。
 ******************************************************************************
 */

#ifndef PATROL_ROBOT_APP_H
#define PATROL_ROBOT_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

/** @brief 初始化应用层及各 BSP 模块。 */
void PatrolRobotApp_Init(void);
/** @brief 执行一次应用层调度循环。 */
void PatrolRobotApp_Process(void);
/** @brief 转发 HAL 串口接收完成事件。 */
void PatrolRobotApp_NotifyUartRxComplete(UART_HandleTypeDef *uart);
/** @brief 转发 HAL 串口空闲接收事件。 */
void PatrolRobotApp_NotifyUartRxEvent(UART_HandleTypeDef *uart,
                                     uint16_t size);
/** @brief 转发 HAL 串口错误事件。 */
void PatrolRobotApp_NotifyUartError(UART_HandleTypeDef *uart);
/** @brief 转发 HAL 定时器输入捕获事件。 */
void PatrolRobotApp_NotifyTimInputCapture(TIM_HandleTypeDef *timer);

#ifdef __cplusplus
}
#endif

#endif /* PATROL_ROBOT_APP_H */
