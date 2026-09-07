# 输电线路巡检机器人

这是一个以 STM32G474 为主控的输电线路巡检机器人项目。当前仓库覆盖距离与温度采集、蓝牙状态输出、ESP-01S 联网、MQTT 遥测，以及接收外部 K230 视觉告警的 UART 链路。

> 项目仍处于原型验证阶段。视觉结果用于告警、记录和上报，不直接驱动电机或其他执行器。

## 功能与状态

| 模块 | 当前实现 | 验证状态 |
| --- | --- | --- |
| STM32 传感器 | GY-906 温度采集、超声波测距 | 当前优化固件已完成板上复验；50/60 mm 滞回、无回波恢复和 5 秒遥测已通过 |
| 本地交互 | MLT-BT05 蓝牙状态与外部视觉告警输出 | 当前优化固件已完成板上复验；三类视觉告警样例链路已有联调记录 |
| STM32 网络 | ESP-01S、MQTT 3.1.1、遥测与视觉事件队列 | 当前优化固件已完成板上复验；MQTT 30 分钟稳定性测试已通过 |
| K230 接口 | STM32 接收 `break`、`heat`、`wear` 三类 UART 告警 | 接收端源码保留；K230 发送端程序不在本仓库发布 |
| 运动控制 | 无 | 当前不在项目范围内 |

“构建通过”“样例联调通过”和“整机长期验收”是不同层级的证据，请勿将其中一项等同于全部完成。

## 项目结构

```text
.
├── Robot/
│   ├── PatrolRobot/              # STM32CubeMX / Keil MDK-ARM 工程
│   │   ├── Application/          # 应用调度、遥测、蓝牙命令、视觉事件
│   │   ├── BSP/                  # 外部模块的板级驱动
│   │   ├── Config/               # 公开配置入口与本地配置示例
│   │   ├── Core/                 # CubeMX 生成的初始化和中断代码
│   │   ├── Drivers/              # STM32 HAL 与 CMSIS
│   │   ├── MDK-ARM/              # Keil 工程文件
│   │   ├── Middleware/MQTT/      # MQTT 3.1.1 编解码与状态机
│   │   └── PatrolRobot.ioc       # STM32CubeMX 配置源
│   ├── 项目架构说明.md
│   └── STM32开发与验证记录_2026-09-02.md
├── CONTRIBUTING.md               # 贡献指南
└── SECURITY.md                   # 安全问题报告说明
```

## 硬件与开发环境

- 主控：STM32G474VET6（LQFP100），工程系统时钟为 170 MHz。
- STM32 工具链：Keil MDK-ARM，工程记录 Arm Compiler 6.24；可使用 STM32CubeMX 6.18.1 打开 `Robot/PatrolRobot/PatrolRobot.ioc`，固件包记录为 STM32CubeG4 1.6.3。
- 视觉端：历史联调使用 Yahboom K230；发送端脚本、模型、运行时和 SD 卡内容不在本仓库发布。
- 外设：MLT-BT05、ESP-01S、GY-906、超声波模块。
- Keil MDK 主程序的精确版本：待确认。

模块供电、电平与具体板卡接法必须以实物丝印、原理图和数据手册为准。

## 已确认接口

| 功能 | 接口与引脚 | 参数 |
| --- | --- | --- |
| MLT-BT05 | STM32 USART3：PB10/TX、PB11/RX | 9600，8N1 |
| K230 告警 | K230 IO32/UART3_TXD → STM32 PC11/UART4_RX，共地 | 115200，8N1 |
| ESP-01S | STM32 UART5：PC12/TX、PD2/RX | 115200，8N1 |
| GY-906 | STM32 I2C1：PA15/SCL、PB7/SDA | 100 kHz，开漏，需确认外部上拉 |
| 超声波 | PA4/TRIG、PA0/TIM2_CH1 ECHO | TIM2 输入捕获 |

K230 告警格式为 `ALERT,<type>,K230_01\r\n`，其中 `<type>` 为 `break`、`heat` 或 `wear`。当前链路为单向通信，IO33/UART3_RXD 与 PC10/UART4_TX 可不连接。

两块板分别供电时不要互连 5 V 或 3.3 V，只连接 UART 信号线和公共地；上电前确认双方均使用兼容的 3.3 V 逻辑电平。

## 快速开始

### 1. 准备本地配置

公开仓库不包含真实凭据，并默认关闭 STM32 的 Wi-Fi/MQTT 自动连接。

- 将 `Robot/PatrolRobot/Config/network_config.local.example.h` 复制为 `network_config.local.h`，填写本地 Wi-Fi、服务器和主题配置。
- 将 `Robot/PatrolRobot/Config/mqtt_secret.local.example.h` 复制为 `mqtt_secret.local.h`，填写 MQTT 凭据。

这些本地文件已由 `.gitignore` 排除。不要把其中的值复制到源码、文档、截图、日志、Issue 或 Pull Request。

### 2. 构建 STM32 固件

1. 用 Keil 打开 `Robot/PatrolRobot/MDK-ARM/PatrolRobot.uvprojx`。
2. 选择 `PatrolRobot` 目标并构建。
3. 使用与目标板匹配的调试器和 Flash 算法进行下载；首次操作前请在 Keil 中核对目标器件和连接设置。

构建生成的 HEX、AXF、对象文件和日志保留在本地，不纳入 Git。公开配置默认值和本地配置均曾通过 Keil 构建，结果为 0 Error、0 Warning；该结果不代替烧录和硬件测试。

## 文档

- [K230 对接指南](./K230对接指南.md)：自行准备视觉模型和发送端时所需的模型、协议、接线与联调要求。
- [STM32 项目架构](./Robot/项目架构说明.md)：模块职责、数据流、时序和维护约束。
- [历史开发与验证记录](./Robot/STM32开发与验证记录_2026-09-02.md)：保留特定日期的构建与板上验证证据。
- [贡献指南](./CONTRIBUTING.md) 与 [安全说明](./SECURITY.md)。

## 当前限制

- 公开仓库不包含 K230 发送端脚本、SD 卡内容、运行时、模型或训练数据，不能单独完成视觉端部署。
- 当前优化固件已完成传感器、蓝牙、Wi-Fi、MQTT、K230 UART 和整机并行运行复验；更换固件、硬件或接线后仍需重新验证。
- MQTT 使用明文 TCP，不能视为适合不可信网络的安全控制通道。
- MQTT 下行尚未定义执行器语义，收到消息不会直接控制机器人。
- 超声波 ECHO 电平、GY-906 供电与 I²C 上拉、ESP-01S 启动脚状态需要按实际模块确认。

## 贡献与安全

提交修改前请阅读 [CONTRIBUTING.md](./CONTRIBUTING.md)。发现可能泄露凭据或影响设备安全的问题时，请不要在公开 Issue 中粘贴敏感信息，按照 [SECURITY.md](./SECURITY.md) 的方式报告。

## 许可证

项目中权属明确的自有代码和文档采用 [Apache License 2.0](./LICENSE)，版权声明为 `Copyright 2026 l0ng-dev`。

许可证范围不包括以下内容：

- STM32 HAL、CMSIS 及其他第三方组件；它们继续遵循各自文件中保留的原始许可证和版权声明。
- K230 发送端脚本、SD 卡内容、模型、训练数据、CanMV 运行时，以及其他未随仓库发布的外部资产；项目采用 Apache-2.0 不会自动授予这些资产的使用或再分发权。
- 来源或权属尚未明确、且文件自身另有许可证或声明的内容。

使用或再分发时，请同时检查相关目录中的第三方许可证文件。
