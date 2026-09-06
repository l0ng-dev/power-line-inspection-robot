# 输电线路巡检机器人

本项目由 STM32G474VET6 主控程序和 K230 视觉程序组成。现有代码实现了超声波测距、GY-906 温度采集、MLT-BT05 蓝牙输出、ESP-01S 联网与 MQTT 上报，以及 K230 对断裂、热损伤和磨损三类缺陷的检测、照片保存和 UART 告警。视觉告警用于通知与诊断，不直接控制电机。

## 目录结构

```text
.
├── Robot/
│   ├── PatrolRobot/              # STM32CubeMX / Keil MDK-ARM 工程
│   │   ├── Application/          # 应用调度、遥测、蓝牙命令、视觉事件
│   │   ├── BSP/                  # 蓝牙、ESP-01S、GY-906、K230、超声波
│   │   ├── Config/               # 可公开配置入口与本地配置
│   │   ├── Core/                 # CubeMX 生成的初始化和中断代码
│   │   ├── Drivers/              # STM32 HAL 与 CMSIS
│   │   ├── MDK-ARM/              # Keil 工程文件
│   │   ├── Middleware/MQTT/      # MQTT 3.1.1 编解码与状态机
│   │   └── PatrolRobot.ioc       # STM32CubeMX 配置源
│   ├── 项目架构说明.md
│   └── 机器人项目CubeMX工程重建交接报告_2026-09-02.md
├── SD/                           # K230 SD 卡脚本与部署配置
│   ├── mp_deployment_source/
│   ├── ybUtils/
│   └── 使用说明.md
└── 输电线路巡检机器人第三方使用说明书.md
```

## 硬件与开发环境

- STM32：STM32G474VET6（LQFP100），工程配置为 Cortex-M4、系统时钟 170 MHz。
- STM32 工具：Keil MDK-ARM，工程启用 Arm Compiler 6；可使用 STM32CubeMX 打开 `Robot/PatrolRobot/PatrolRobot.ioc`。
- 视觉端：Yahboom K230；脚本依赖其 CanMV/MicroPython 环境中的 `aicube`、`media`、`nncase_runtime`、`ulab`、`image`、`network` 和 `machine` 模块。
- 外设：MLT-BT05、ESP-01S、GY-906、超声波模块。模块电源和具体板级电气连接应以实物与数据手册为准。
- Keil、STM32CubeMX、CanMV 固件/IDE 的精确版本：待确认。KModel 的 `nncase_version` 配置为 2.9.0。

## 已确认的接口与协议

| 功能 | 接口与引脚 | 参数 |
|---|---|---|
| MLT-BT05 | STM32 USART3：PB10/TX、PB11/RX | 9600，8N1 |
| K230 告警 | K230 IO32/UART3_TXD → STM32 PC11/UART4_RX，共地 | 115200，8N1 |
| ESP-01S | STM32 UART5：PC12/TX、PD2/RX | 115200，8N1 |
| GY-906 | STM32 I2C1：PA15/SCL、PB7/SDA | 时序来自 CubeMX 工程 |
| 超声波 | PA4/TRIG、PA0/TIM2_CH1 ECHO | TIM2 输入捕获 |

K230 到 STM32 的当前报文格式为 `ALERT,<type>,K230_01\r\n`，其中 `<type>` 为 `break`、`heat` 或 `wear`。当前链路是单向传输；K230 IO33/UART3_RXD 和 STM32 PC10/UART4_TX 不要求连接。两块板分别供电时不要互连 5 V 或 3.3 V，只连接信号线和公共地，并在上电前确认双方均为兼容的 3.3 V UART 电平。

## 安全配置

公开版本默认关闭 STM32 的 Wi-Fi/MQTT 自动连接，且不包含任何真实凭据。本地使用方法如下：

1. 在 `Robot/PatrolRobot/Config/` 中把 `network_config.local.example.h` 和 `mqtt_secret.local.example.h` 分别复制为去掉 `.example` 的本地文件，再填入自己的 Wi-Fi、主题和巴法云私钥。
2. 在 `SD/mp_deployment_source/` 中把 `k230_cloud_secret_local.example.py` 复制为 `k230_cloud_secret_local.py`，再填写 K230 使用的 Wi-Fi、云 UID 和图片主题。
3. 这三个 `*.local.*` 文件已写入根目录 `.gitignore`。不要把真实配置粘贴到 README、日志、截图或公开问题单。

本地工作副本已保留原有配置到上述本地文件。若这些凭据曾通过其他目录、压缩包或既有仓库公开，应立即在对应平台轮换；本目录当前不是 Git 仓库，无法检查历史泄露。

## 构建与部署

### STM32

1. 用 Keil 打开 `Robot/PatrolRobot/MDK-ARM/PatrolRobot.uvprojx`。
2. 按“安全配置”准备本地头文件。
3. 选择 `PatrolRobot` 目标并构建。工程配置会生成 HEX；默认输出目录为 `Robot/PatrolRobot/MDK-ARM/PatrolRobot/`。
4. 烧录器、下载算法和目标板连接方法依赖本机 Keil 配置，公开文件无法完整确认，烧录前请在 Keil 中核对，当前步骤标记为待确认。

### K230

1. 按 `SD/使用说明.md` 将所需内容放到 SD 卡根目录，确保设备路径为 `/sdcard/main.py`。
2. 按“安全配置”创建本地云配置；不需要云上传时可以保持空值。
3. 补充与目标固件匹配的 `SD/micropython` 和 KModel 文件。它们因体积、来源/授权和二进制内容无法在当前目录内充分审计，默认不纳入 Git；公开下载地址与再分发许可待确认。
4. KModel 的期望文件名和类别顺序见 `SD/mp_deployment_source/deploy_config.json`。
5. 上电时按住板载按键可跳过自动检测并进入 IDE 维护状态。

## 大文件与仓库策略

- `SD/micropython`（约 26.9 MiB）是 ELF 二进制运行时，来源、版本和再分发许可待确认，且二进制中存在证书/私钥格式测试标记；当前默认忽略，不建议直接公开。确认官方来源和许可后，优先提供官方校验下载说明，或再评估 Git LFS。
- KModel（约 1.8 MiB）是运行必需的模型产物，但训练数据来源、模型许可和再分发权待确认；当前默认忽略。确认有权公开后，该体积可直接纳入 Git，模型版本较多时再使用 Git LFS。
- Keil 输出、日志、运行时照片和个人 IDE 状态均由 `.gitignore` 排除，可在本地保留但不应上传。

## 当前限制与验证边界

- 现有文档记录了源码/协议检查和历史构建、烧录及样例联调结果；这些记录不等同于当前公开整理版本已经重新完成板上验收。
- 三类样例链路已有历史记录，但完整数据集准确率、每类召回率、混淆矩阵和长期压力测试仍待完成。
- 当前二进制重新烧录后的整机功能、MQTT 长时间稳定性、断网恢复和实际硬件电平需在目标设备上复验。
- 模型、训练数据、K230 运行时以及项目自有源码的公开许可/第三方授权均待确认；STM32 HAL/CMSIS 自带许可文件位于 `Drivers/` 对应目录。
- 仓库根目录尚未提供项目级 `LICENSE`。公开发布前应由权利人选择合适许可证；在此之前不要假定他人拥有复制、修改或分发项目自有内容的权限。

更详细的部署、故障排查和验收步骤见 [第三方使用说明书](./输电线路巡检机器人第三方使用说明书.md)、[STM32 项目架构说明](./Robot/项目架构说明.md)、[K230 使用说明](./SD/使用说明.md) 和 [外部部署资产说明](./ASSETS.md)。
