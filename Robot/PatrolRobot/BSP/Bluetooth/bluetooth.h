/**
 ******************************************************************************
 * @file    bluetooth.h
 * @brief   蓝牙文本通信驱动接口
 *
 * @details
 * 主要功能：
 * 1. 定义蓝牙命令缓冲区。
 * 2. 提供文本收发、命令取出和 HAL 回调处理接口。
 *
 * 模块关系：
 * PatrolRobot 应用层通过本接口访问蓝牙通信功能。
 *
 * 主要接口：
 * BSP_Bluetooth_Init()、BSP_Bluetooth_Transmit()、BSP_Bluetooth_TakeLine()。
 ******************************************************************************
 */

#ifndef BSP_BLUETOOTH_H
#define BSP_BLUETOOTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

#define BSP_BLUETOOTH_RX_LINE_SIZE  96U

/**
 * @brief  初始化蓝牙串口并启动中断接收
 * @param  uart 蓝牙模块使用的 UART 句柄
 * @retval HAL 状态
 */
HAL_StatusTypeDef BSP_Bluetooth_Init(UART_HandleTypeDef *uart);
/**
 * @brief  发送一条文本消息
 * @param  message 以空字符结束的待发送文本
 * @retval HAL 状态
 */
HAL_StatusTypeDef BSP_Bluetooth_Transmit(const char *message);
/**
 * @brief  取出一条完整命令
 * @param  line     命令输出缓冲区
 * @param  capacity 输出缓冲区容量
 * @retval 取到命令返回 1，否则返回 0
 */
uint8_t BSP_Bluetooth_TakeLine(char *line, uint16_t capacity);
/**
 * @brief  读取并清除一次接收溢出事件
 * @retval 存在溢出事件返回 1，否则返回 0
 */
uint8_t BSP_Bluetooth_TakeOverflowEvent(void);
/**
 * @brief  处理 HAL 串口接收完成事件
 * @param  uart 触发回调的 UART 句柄
 */
void BSP_Bluetooth_RxCpltCallback(UART_HandleTypeDef *uart);
/**
 * @brief  处理 HAL 串口错误事件
 * @param  uart 触发回调的 UART 句柄
 */
void BSP_Bluetooth_ErrorCallback(UART_HandleTypeDef *uart);

#ifdef __cplusplus
}
#endif

#endif /* BSP_BLUETOOTH_H */
