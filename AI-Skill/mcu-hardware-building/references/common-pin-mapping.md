# STM32F103C8T6 引脚资源参考

分配引脚时，参考本文件。STM32F103C8T6 是 48 引脚最小系统板，常用资源如下。

## 一、特殊功能引脚（必须按外设选，不能乱用）

### PWM 输出（定时器通道）

| 定时器 | 通道 | 引脚 | 备注 |
| --- | --- | --- | --- |
| TIM2 | CH1 | PA0 | 常用 |
| TIM2 | CH2 | PA1 | 注意：PA1 也是 ADC1_IN1 |
| TIM2 | CH3 | PA2 | 注意：PA2 也是 USART2_TX |
| TIM3 | CH1 | PA6 | 常用 |
| TIM3 | CH2 | PA7 | 常用 |
| TIM4 | CH1 | PB6 | 注意：PB6 也是 I2C1_SCL |
| TIM4 | CH2 | PB7 | 注意：PB7 也是 I2C1_SDA |
| TIM4 | CH3 | PB8 | 风扇 PWM 常用 |
| TIM4 | CH4 | PB9 | |

> 选 PWM 引脚时，避开已被 I2C/USART/ADC 占用的引脚。

### USART 串口

| USART | TX | RX | 备注 |
| --- | --- | --- | --- |
| USART1 | PA9 | PA10 | **保留给单片机调试打印（printf），不给模块用** |
| USART2 | PA2 | PA3 | 注意：PA2/PA3 也是 TIM2_CH3/CH4 |
| USART3 | PB10 | PB11 | |

> 一个 USART 只能给一个模块。**USART1 保留给单片机调试打印**，模块（WiFi/蓝牙等）优先使用 USART2 或 USART3。

### I2C

| I2C | SCL | SDA | 备注 |
| --- | --- | --- | --- |
| I2C1（硬件） | PB6 | PB7 | 硬件 I2C |
| 软件 I2C（模拟） | 任意两个空闲 GPIO | | 本教程 OLED 常用软件 I2C，任意两脚模拟 SCL/SDA |

### ADC 模拟输入

| ADC1 通道 | 引脚 |
| --- | --- |
| ADC1_IN0 | PA0 |
| ADC1_IN1 | PA1 |
| ADC1_IN2 | PA2 |
| ADC1_IN3 | PA3 |
| ADC1_IN4 | PA4 |
| ADC1_IN5 | PA5 |
| ADC1_IN6 | PA6 |
| ADC1_IN7 | PA7 |
| ADC1_IN8 | PB0 |
| ADC1_IN9 | PB1 |

> ADC 引脚不要复用为数字输出功能（会读不到模拟值）。

## 二、普通 GPIO（任意空闲引脚即可）

- 按键、蜂鸣器、LED、单总线（DHT11）等纯数字 IO
- 建议避开特殊功能引脚（TIM/USART/I2C/ADC），避免将来加功能时冲突
- ⚠️ **必须避开 JTAG/SWD 调试引脚**：PA13/PA14/PA15、PB3/PB4 上电默认被调试功能占用，用作 GPIO 前需先禁用对应调试功能（见第四节）

## 三、电源引脚（接线阶段用）

- 3.3V、5V、GND 由供电电路决定；ESP8266 必须 3.3V，不能接 5V

## 四、冲突规避速查

| 容易撞车的引脚 | 撞车原因 | 规避建议 |
| --- | --- | --- |
| PA0 | TIM2_CH1 和 ADC1_IN0 复用 | 明确用途：PWM 还是 ADC，二选一 |
| PA1 | TIM2_CH2 和 ADC1_IN1 复用 | 同上 |
| PA2/PA3 | USART2 和 TIM2_CH3/CH4 复用 | 优先留给 USART 或 PWM，别混用 |
| PB6/PB7 | I2C1 和 TIM4_CH1/CH2 复用 | 优先做 I2C（OLED），别占来做 PWM |
| PA9/PA10 | USART1 专用 | 留给单片机调试打印，别再当普通 GPIO |
| PA11 | USBDM / USART1_RX(重映射) | 不用 USB/USART1 时可作普通 GPIO（DHT11 常用） |
| PB10/PB11 | USART3 TX/RX | 若占作普通 GPIO（如蜂鸣器），USART3 不能再接模块 |
| PA13/PA14 | SWDIO/SWCLK（SWD 调试） | **保留给调试**，勿用作 GPIO |
| PA15 | JTDI（JTAG） | 需禁用 JTAG 才能用 |
| PB3 | JTDO（JTAG） | 需禁用 JTAG（JTAGDisable）才能当 GPIO |
| PB4 | NJTRST（JTAG） | 需禁用 JTAG（JTAGDisable）才能当 GPIO |

## 五、JTAG/SWD 调试引脚（重要，易踩坑）

STM32F103 上电后以下引脚默认被**调试功能**占用，不能直接当普通 GPIO 用，否则模块无反应：

| 引脚 | 默认功能 | 说明 |
| --- | --- | --- |
| PA13 | SWDIO | SWD 调试数据 |
| PA14 | SWCLK | SWD 调试时钟 |
| PA15 | JTDI | JTAG 数据输入 |
| PB3 | JTDO | JTAG 数据输出 |
| PB4 | NJTRST | JTAG 复位 |

**用法**：若要用 PA15/PB3/PB4 做普通 GPIO，需在代码初始化时关闭 JTAG：

```c
RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);   /* 关 JTAG，留 SWD */
```

- `GPIO_Remap_SWJ_JTAGDisable`：关 JTAG（释放 PA15/PB3/PB4），**保留 SWD**（PA13/PA14 仍可调试），最常用
- `GPIO_Remap_SWJ_Disable`：连 SWD 一起关（失去调试能力，慎用）
- **PA13/PA14（SWD）原则上不做 GPIO**，否则无法下载调试

> 分配普通 GPIO 时，**优先避开这 5 个引脚**；实在要用 PB3/PB4/PB15，必须在文档"注意事项"里注明需要禁用 JTAG，避免后续编程踩坑。

## 六、常见模块推荐引脚（示例）

| 模块 | 推荐引脚 | 说明 |
| --- | --- | --- |
| DHT11（单总线） | PA11 | 任意 GPIO，需上拉（PA11 是 USB D- 复用脚，不用 USB 即可作 GPIO） |
| OLED（I2C） | PB6(SCL)/PB7(SDA) | 软件 I2C |
| 小风扇（PWM） | PB8（TIM4_CH3） | 模块化直驱或经 S8050 |
| ESP8266 | PA2(TX)/PA3(RX)（USART2）或 PB10(TX)/PB11(RX)（USART3） | 3.3V 供电，优先 USART2/3；CH_PD 已内部上拉，无需外接（若模块不工作再接 3.3V） |
| 蜂鸣器（GPIO） | PB10 | 高电平响。⚠️ PB10 是 USART3_TX，占用后 USART3 不可再接模块 |
| 电位器（ADC） | PB1（ADC1_IN9） | 模拟输入 |
| 按键×3（GPIO） | PB12/PB13/PB14 | 上拉输入，普通 GPIO，无 JTAG/SWD 冲突 |
