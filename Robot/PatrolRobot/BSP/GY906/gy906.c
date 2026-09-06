/**
 ******************************************************************************
 * @file    gy906.c
 * @brief   GY-906 红外温度传感器驱动
 *
 * @details
 * 主要功能：
 * 1. 通过 I2C1 读取环境温度和目标温度，并校验 SMBus PEC。
 * 2. 提供有限次数的总线恢复。
 *
 * 模块关系：
 * PatrolRobot 应用层调用本模块，本模块使用 CubeMX 生成的 I2C1 配置。
 *
 * 主要接口：
 * BSP_GY906_Init()、BSP_GY906_ReadTemperatures()、BSP_GY906_RecoverBus()。
 ******************************************************************************
 */

#include "gy906.h"
#include "main.h"

#define BSP_GY906_I2C_TIMEOUT_MS  100U
#define BSP_GY906_READY_TRIALS    3U
#define BSP_GY906_PEC_POLYNOMIAL  0x07U
#define BSP_GY906_ERROR_FLAG      0x8000U
#define BSP_GY906_KELVIN_SCALE    0.02f
#define BSP_GY906_KELVIN_OFFSET   273.15f

static BSP_GY906_StatusTypeDef BSP_GY906_ReadWord(
    BSP_GY906_HandleTypeDef *sensor,
    uint8_t command,
    uint16_t *value);

/** @brief 计算 SMBus PEC-8 校验值 */
uint8_t BSP_GY906_CalculatePEC(const uint8_t *data, uint8_t length)
{
  uint8_t crc = 0U;
  uint8_t index;
  uint8_t bit;

  if (data == NULL)
  {
    return 0U;
  }

  for (index = 0U; index < length; index++)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((crc << 1U) ^ BSP_GY906_PEC_POLYNOMIAL);
      }
      else
      {
        crc <<= 1U;
      }
    }
  }

  return crc;
}

/** @brief 绑定 I2C 句柄并探测 GY-906 */
BSP_GY906_StatusTypeDef BSP_GY906_Init(BSP_GY906_HandleTypeDef *sensor,
                                        I2C_HandleTypeDef *i2c)
{
  if ((sensor == NULL) || (i2c == NULL))
  {
    return BSP_GY906_STATUS_INVALID_ARGUMENT;
  }

  sensor->i2c = i2c;
  sensor->device_address =
      (uint16_t)(BSP_GY906_DEFAULT_ADDRESS_7BIT << 1U);
  sensor->last_hal_status = HAL_I2C_IsDeviceReady(sensor->i2c,
                                                   sensor->device_address,
                                                   BSP_GY906_READY_TRIALS,
                                                   BSP_GY906_I2C_TIMEOUT_MS);
  if (sensor->last_hal_status == HAL_BUSY)
  {
    return BSP_GY906_STATUS_BUS_BUSY;
  }

  if (sensor->last_hal_status != HAL_OK)
  {
    return BSP_GY906_STATUS_NOT_READY;
  }

  return BSP_GY906_STATUS_OK;
}

/** @brief 读取一个字寄存器并验证 PEC 和传感器错误位 */
static BSP_GY906_StatusTypeDef BSP_GY906_ReadWord(
    BSP_GY906_HandleTypeDef *sensor,
    uint8_t command,
    uint16_t *value)
{
  uint8_t response[3];
  uint8_t pec_data[5];
  uint16_t raw;

  if ((sensor == NULL) || (sensor->i2c == NULL) || (value == NULL))
  {
    return BSP_GY906_STATUS_INVALID_ARGUMENT;
  }

  sensor->last_hal_status = HAL_I2C_Mem_Read(sensor->i2c,
                                              sensor->device_address,
                                              command,
                                              I2C_MEMADD_SIZE_8BIT,
                                              response,
                                              sizeof(response),
                                              BSP_GY906_I2C_TIMEOUT_MS);
  if (sensor->last_hal_status != HAL_OK)
  {
    return BSP_GY906_STATUS_I2C_ERROR;
  }

  /*
   * PEC 覆盖写地址、命令、读地址和两个响应数据字节，
   * 不包含传感器返回的 PEC 字节本身。
   */
  pec_data[0] = (uint8_t)(sensor->device_address & 0xFEU);
  pec_data[1] = command;
  pec_data[2] = (uint8_t)(sensor->device_address | 0x01U);
  pec_data[3] = response[0];
  pec_data[4] = response[1];

  if (BSP_GY906_CalculatePEC(pec_data, sizeof(pec_data)) != response[2])
  {
    return BSP_GY906_STATUS_PEC_ERROR;
  }

  raw = (uint16_t)response[0] | ((uint16_t)response[1] << 8U);
  if ((raw & BSP_GY906_ERROR_FLAG) != 0U)
  {
    return BSP_GY906_STATUS_SENSOR_ERROR;
  }

  *value = raw;
  return BSP_GY906_STATUS_OK;
}

/** @brief 读取并换算环境温度和目标温度 */
BSP_GY906_StatusTypeDef BSP_GY906_ReadTemperatures(
    BSP_GY906_HandleTypeDef *sensor,
    BSP_GY906_DataTypeDef *data)
{
  BSP_GY906_StatusTypeDef status;
  uint16_t raw_ambient;
  uint16_t raw_object;

  if (data == NULL)
  {
    return BSP_GY906_STATUS_INVALID_ARGUMENT;
  }

  status = BSP_GY906_ReadWord(sensor,
                              BSP_GY906_REGISTER_AMBIENT,
                              &raw_ambient);
  if (status != BSP_GY906_STATUS_OK)
  {
    return status;
  }

  status = BSP_GY906_ReadWord(sensor,
                              BSP_GY906_REGISTER_OBJECT_1,
                              &raw_object);
  if (status != BSP_GY906_STATUS_OK)
  {
    return status;
  }

  data->raw_ambient = raw_ambient;
  data->raw_object = raw_object;
  data->ambient_c = ((float)raw_ambient * BSP_GY906_KELVIN_SCALE) -
                    BSP_GY906_KELVIN_OFFSET;
  data->object_c = ((float)raw_object * BSP_GY906_KELVIN_SCALE) -
                   BSP_GY906_KELVIN_OFFSET;

  return BSP_GY906_STATUS_OK;
}

/**
 * @brief  尝试释放被占用的 I2C1 总线
 * @note   最多输出九个 SCL 脉冲，再生成停止条件并重新初始化 I2C1。
 */
HAL_StatusTypeDef BSP_GY906_RecoverBus(BSP_GY906_HandleTypeDef *sensor)
{
  GPIO_InitTypeDef gpio_init = {0};
  HAL_StatusTypeDef init_status;
  uint32_t pulse;

  if ((sensor == NULL) || (sensor->i2c == NULL) ||
      (sensor->i2c->Instance != I2C1))
  {
    return HAL_ERROR;
  }

  /*
   * 恢复期间暂时关闭 I2C 外设，以 GPIO 开漏方式发送最多九个时钟，
   * 随后生成停止条件，使从设备有机会释放 SDA。
   */
  (void)HAL_I2C_DeInit(sensor->i2c);

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(I2C1_SDA_GPIO_Port, I2C1_SDA_Pin, GPIO_PIN_SET);

  gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

  gpio_init.Pin = I2C1_SCL_Pin;
  HAL_GPIO_Init(I2C1_SCL_GPIO_Port, &gpio_init);
  gpio_init.Pin = I2C1_SDA_Pin;
  HAL_GPIO_Init(I2C1_SDA_GPIO_Port, &gpio_init);

  HAL_Delay(1U);

  for (pulse = 0U; pulse < 9U; pulse++)
  {
    if (HAL_GPIO_ReadPin(I2C1_SDA_GPIO_Port, I2C1_SDA_Pin) == GPIO_PIN_SET)
    {
      break;
    }

    HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin, GPIO_PIN_RESET);
    HAL_Delay(1U);
    HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin, GPIO_PIN_SET);
    HAL_Delay(1U);
  }

  HAL_GPIO_WritePin(I2C1_SDA_GPIO_Port, I2C1_SDA_Pin, GPIO_PIN_RESET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin, GPIO_PIN_SET);
  HAL_Delay(1U);
  HAL_GPIO_WritePin(I2C1_SDA_GPIO_Port, I2C1_SDA_Pin, GPIO_PIN_SET);
  HAL_Delay(1U);

  __HAL_RCC_I2C1_FORCE_RESET();
  __HAL_RCC_I2C1_RELEASE_RESET();

  init_status = HAL_I2C_Init(sensor->i2c);
  sensor->last_hal_status = init_status;
  if (init_status != HAL_OK)
  {
    return init_status;
  }

  if ((HAL_GPIO_ReadPin(I2C1_SCL_GPIO_Port, I2C1_SCL_Pin) == GPIO_PIN_RESET) ||
      (HAL_GPIO_ReadPin(I2C1_SDA_GPIO_Port, I2C1_SDA_Pin) == GPIO_PIN_RESET))
  {
    sensor->last_hal_status = HAL_ERROR;
    return HAL_ERROR;
  }

  return HAL_OK;
}
