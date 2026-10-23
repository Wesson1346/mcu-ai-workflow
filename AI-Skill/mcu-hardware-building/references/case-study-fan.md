# 案例：STM32 智能温控风扇系统（完整四步法示例）

本案例展示"硬件搭建四步法"的完整过程，作为执行 skill 时的参考。用户提供新系统的硬件模块清单时，按此流程推进。

## 第一步：梳理硬件模块清单

**输入**：智能温控风扇系统硬件选型清单 / 功能拆解文档

| 模块 | 型号 | 接口 | 引脚需求 |
| --- | --- | --- | --- |
| 温度采集 | DHT11 | 单总线 | 普通 GPIO（需上拉） |
| OLED 显示 | 0.96寸 SSD1306 | I2C | SCL/SDA |
| 风扇控制 | 自带驱动的风扇模块 | PWM | TIM 通道 |
| 按键 ×3 | 6×6轻触按键 | GPIO | 3 个 GPIO |
| 蜂鸣器 | 有源蜂鸣器 | GPIO | 普通 GPIO |
| 电位器 | 10K电位器 | ADC | ADC 模拟输入 |
| WiFi | ESP8266 ESP-01S | USART | 一个 USART（TX/RX） |

**资源需求**：1 单总线 + 1 I2C(SCL/SDA) + 1 PWM + 3 GPIO + 1 ADC + 1 USART(TX/RX)。STM32F103C8T6 够用。

## 第二步：分配引脚

对照 `common-pin-mapping.md` 引脚资源参考，先分特殊功能，再分普通 GPIO：

```
温度传感器   DHT11           单总线    PA11（DATA）
OLED显示屏   0.96寸 SSD1306  I2C       PB6(SCL), PB7(SDA)
风扇         自带驱动风扇模块  PWM       PB8（TIM4_CH3）
WiFi模块     ESP8266 ESP-01S USART2    PA2(TX), PA3(RX)
蜂鸣器       有源蜂鸣器       GPIO      PB10
电位器       10K旋转电位器    ADC1      PB1（ADC1_IN9）
按键 ×3      6×6轻触按键×3   GPIO      PB12, PB13, PB14
```

**执行器接线方式**：本例风扇是**模块化直驱**（自带驱动），信号端直接接 PB8（TIM4_CH3），无需 S8050。

## 第三步：检查冲突

- ✅ 每个引脚只分给一个模块，无重复占用
- ✅ 风扇 PWM 用 TIM4_CH3（PB8）
- ✅ ESP8266 独享 USART2（USART1 保留给单片机调试打印）
- ✅ I2C SCL/SDA 成对（PB6/PB7）
- ✅ 电位器 ADC（PB1/ADC1_IN9）未复用为数字功能
- ✅ 引脚编号与 STM32F103C8T6 一致
- ✅ 按键 PB12/PB13/PB14 为普通 GPIO，无 JTAG/SWD 冲突，无需禁用 JTAG
- ⚠️ 蜂鸣器 PB10 是 USART3_TX 复用引脚，本设计 ESP8266 用 USART2、未占用 USART3，可作普通 GPIO；DHT11 用 PA11（USB D- 复用脚），本设计不用 USB，可作普通 GPIO

**结果：无引脚冲突，无需禁用 JTAG。**

## 第四步：输出硬件搭建文档

基于 `assets/hardware-building-template.md` 模板，生成 `智能温控风扇系统硬件搭建文档.md`，保存到当前工作目录。内容包含：搭建依据、元器件清单、搭建总思路、引脚分配速查、各模块详细接线、接线总表、整体接线图、接线注意事项、验证建议。

**各模块接线要点**：

```
DHT11：VCC→3.3V, GND→GND, DATA→PA11（需上拉）
OLED： VCC→3.3V, GND→GND, SCL→PB6, SDA→PB7（软件I2C）
风扇模块：VCC→5V(按供电要求), GND→GND, PWM→PB8（模块化直驱，TIM4_CH3）
ESP8266：VCC→3.3V(必须), GND→GND, TX→PA3(USART2_RX), RX→PA2(USART2_TX)（CH_PD 已内部上拉，无需外接）
蜂鸣器：VCC→3.3V, GND→GND, I/O→PB10（高电平响）
电位器：两端→3.3V/GND, 中间抽头→PB1（ADC1_IN9）
按键×3：一脚→PB12/13/14, 另一脚→GND（上拉输入）
```

> ⚠️ 蜂鸣器用的 PB10 是 USART3_TX 复用引脚，本设计未用 USART3 所以可作普通 GPIO；若后续要接其他 USART3 模块需调整。DHT11 用的 PA11 是 USB D-/CAN_RX 复用脚，本设计不用 USB/CAN 可作普通 GPIO。风扇 PWM 用 TIM4_CH3（PB8），编程时需使能 TIM4 时钟。
> ⚠️ 电源噪声提醒：若用面包板搭建电路，且由电脑 USB 口给单片机供电，尽量不使用外接 HDMI 显示器（尤其是笔记本电脑）——某些情况下外接显示器会带来电源噪声，干扰单片机正常工作

## 小结

硬件搭建四步法：梳理模块清单 → 分配引脚 → 检查冲突 → 输出接线文档。

核心原则：
- 特殊功能引脚优先（PWM/USART/I2C/ADC 绑外设）
- 一个引脚只给一个模块，冲突必须调整
- 接线一步到位（信号引脚 + 电源 VCC/GND），不写驱动代码
