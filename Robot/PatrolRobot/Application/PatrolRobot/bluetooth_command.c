/** @file bluetooth_command.c @brief 必要的现场 Wi-Fi 配置命令。 */
#include "bluetooth_command.h"
#include "bluetooth.h"
#include "usart.h"

static BSP_WiFi_HandleTypeDef *esp01s;

static uint8_t BluetoothCommand_MatchIgnoreCase(const char *text,
                                              const char *expected,
                                              uint8_t require_end)
{
  while (*expected != '\0')
  {
    char text_char = *text;
    char expected_char = *expected;

    if (text_char == '\0')
    {
      return 0U;
    }
    if ((text_char >= 'a') && (text_char <= 'z'))
    {
      text_char = (char)(text_char - ('a' - 'A'));
    }
    if ((expected_char >= 'a') && (expected_char <= 'z'))
    {
      expected_char = (char)(expected_char - ('a' - 'A'));
    }
    if (text_char != expected_char)
    {
      return 0U;
    }
    text++;
    expected++;
  }

  return ((require_end == 0U) || (*text == '\0')) ? 1U : 0U;
}


static void BluetoothCommand_ProcessBluetoothCommand(const char *command)
{
  static const char wifi_ssid_prefix[] = "WIFI SSID ";
  static const char wifi_password_prefix[] = "WIFI PASS ";

  if (BluetoothCommand_MatchIgnoreCase(command, wifi_ssid_prefix, 0U) != 0U)
  {
    if (BSP_WiFi_SetSSID(esp01s,
                         command + sizeof(wifi_ssid_prefix) - 1U) == HAL_OK)
    {
      (void)BSP_Bluetooth_Transmit("WiFi:SSID已存\r\n");
    }
    else
    {
      (void)BSP_Bluetooth_Transmit("WiFi:参数错误\r\n");
    }
  }
  else if (BluetoothCommand_MatchIgnoreCase(
               command, wifi_password_prefix, 0U) != 0U)
  {
    if (BSP_WiFi_SetPassword(
            esp01s,
            command + sizeof(wifi_password_prefix) - 1U) == HAL_OK)
    {
      (void)BSP_Bluetooth_Transmit("WiFi:密码已存\r\n");
    }
    else
    {
      (void)BSP_Bluetooth_Transmit("WiFi:参数错误\r\n");
    }
  }
  else if (BluetoothCommand_MatchIgnoreCase(command, "WIFI CONNECT", 1U) != 0U)
  {
    if (BSP_WiFi_Connect(esp01s, HAL_GetTick()) == HAL_OK)
    {
      (void)BSP_Bluetooth_Transmit("WiFi:开始连接\r\n");
    }
    else
    {
      (void)BSP_Bluetooth_Transmit("WiFi:待配置\r\n");
    }
  }
  else
  {
    (void)BSP_Bluetooth_Transmit("未知指令\r\n");
  }
}


void BluetoothCommand_Init(BSP_WiFi_HandleTypeDef *esp01s_handle)
{
  esp01s = esp01s_handle;
  if (BSP_Bluetooth_Init(&huart3) == HAL_OK)
  {
    (void)BSP_Bluetooth_Transmit("系统就绪\r\n");
  }
}

void BluetoothCommand_Process(void)
{
  char command[BSP_BLUETOOTH_RX_LINE_SIZE];
  if (BSP_Bluetooth_TakeOverflowEvent() != 0U)
  {
    (void)BSP_Bluetooth_Transmit("指令过长\r\n");
  }
  if (BSP_Bluetooth_TakeLine(command, sizeof(command)) != 0U)
  {
    BluetoothCommand_ProcessBluetoothCommand(command);
  }
}

void BluetoothCommand_NotifyUartRxComplete(UART_HandleTypeDef *uart)
{
  BSP_Bluetooth_RxCpltCallback(uart);
}

void BluetoothCommand_NotifyUartError(UART_HandleTypeDef *uart)
{
  BSP_Bluetooth_ErrorCallback(uart);
}
