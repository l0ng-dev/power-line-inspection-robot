/** @file bluetooth_command.h @brief 蓝牙现场配置命令接口。 */
#ifndef PATROL_ROBOT_BLUETOOTH_COMMAND_H
#define PATROL_ROBOT_BLUETOOTH_COMMAND_H

#include "esp01s.h"

/** @brief 绑定待配置的 ESP-01S 句柄。 */
void BluetoothCommand_Init(BSP_WiFi_HandleTypeDef *esp01s_handle);
/** @brief 处理一条已接收命令；该函数不等待网络连接结果。 */
void BluetoothCommand_Process(void);
void BluetoothCommand_NotifyUartRxComplete(UART_HandleTypeDef *uart);
void BluetoothCommand_NotifyUartError(UART_HandleTypeDef *uart);

#endif /* PATROL_ROBOT_BLUETOOTH_COMMAND_H */
