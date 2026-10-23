---
name: stm32-driver
description: 自动生成 STM32F103 单片机传感器/外设驱动代码（.c 和 .h 文件）。当用户需要生成、创建、编写 STM32 或 STM32F103 的传感器驱动、外设驱动、模块驱动代码时触发此 Skill。覆盖 OLED、DHT11、蜂鸣器、超声波、电机驱动、MPU6050 等常见模块，自动检测并适配工程实际的底层库（标准外设库 StdPeriph / HAL 库），生成规范化的 C 语言驱动代码。
---

# STM32F103 驱动代码生成器

## 概述

本 Skill 用于自动生成 STM32F103 单片机各类传感器/外设的驱动代码（.c 和 .h 文件）。

**核心特性：自动适配底层库**——生成代码前先检测当前工程实际使用的是 **标准外设库（StdPeriph）** 还是 **HAL 库**，然后全程使用对应库的 API，确保生成的驱动能直接编译通过，不与工程现有风格冲突。

> ⚠️ 不要假定所有工程都用标准外设库。CubeMX 生成的工程、以及近年新建的工程多数是 HAL 库，两者 API 完全不同，混用会导致编译失败。

## 触发条件

当用户请求中出现以下模式时触发本 Skill：
- "生成 XX 驱动"、"写一个 XX 驱动"、"帮我写 XX 的 .c 和 .h"
- "STM32 XX 模块代码"、"XX 传感器驱动"
- 涉及 STM32F103 外设/传感器模块的代码生成需求

## 工作流

### 第 0 步：检测工程库类型（必须在生成代码前完成）

⚠️ **不要默认使用标准外设库**。先检测当前工程实际使用的底层库。

**检测方法**（按顺序检查，命中即确定）：

1. 查找标志性文件：
   - 存在 `stm32f1xx_hal.h` / `stm32f1xx_hal_conf.h` / `Drivers/STM32F1xx_HAL_Driver/` → **HAL 库**
   - 存在 `stm32f10x.h` + `stm32f10x_gpio.h` + `stm32f10x_rcc.h` → **标准外设库（StdPeriph）**
   - 存在 `stm32f1xx_ll_*.h` → **LL 库**（按 HAL 分支处理，函数用 `LL_` 前缀）
2. 查看 `main.c` 内容：
   - 有 `HAL_Init()` / `MX_GPIO_Init()` / `SystemClock_Config()` → HAL 库
   - 有 `RCC_APB2PeriphClockCmd()` / `GPIO_Init()` → StdPeriph
3. 存在 `.ioc` 文件（CubeMX 工程）→ 通常是 HAL 库
4. **无法判断时**：必须先询问用户，不要擅自假设。用户明确指定（如"用 HAL 库生成"）则以用户要求为准。

**检测完成后**：

- 全程严格使用对应库的 API（见下方"库适配规范"）
- **同一份驱动内不得混用两种库风格**（`GPIO_SetBits()` 与 `HAL_GPIO_WritePin()` 不能同时出现）
- 生成完成后，在说明中明确告知用户"本驱动基于 XX 库生成"

### 第 1 步：确认模块信息

1. 识别用户请求的传感器/外设模块名称
2. 读取 `references/sensor-catalog.md`，查找该模块的通信协议、典型引脚连接、关键时序参数和必备函数列表
3. 若涉及 ESP8266/WiFi/OneNET/云平台/物模型属性上报/App 下发指令，**必须**再读取 `references/onenet-guide.md`，按其中的约定生成上报与接收代码（重点：属性值要 `{"value":...}` 包装；**bool 属性必须用 `true`/`false`，不能写 `1`/`0`**；**接收层必须整份复用修复版 `esp8266.c`，不能把 `ESP8266_Clear()` 放回发送路径**；**发送层必须保留 `ESP8266_FindInBuf()` 按字节查找（不能改回 `strstr`）与 `ESP8266_WaitSendDone()`**）。**若 OneNET 控制台尚未配置**（产品/设备/物模型还没建、Token 没生成），先读 `references/onenet-setup-guide.md` 完成配置并用 MQTT.fx 验证平台侧，再生成代码——平台没配通时，代码写得再对也连不上
4. 若目录中无该模块，根据通用知识推断并告知用户

### 第 2 步：确认引脚配置

向用户确认以下信息（若用户未指定则使用目录中的默认引脚）：
- GPIO 引脚分配（如 I2C 的 SCL/SDA、SPI 的 MOSI/MISO/SCK/CS）
- USART 波特率（如需要）
- 定时器通道（如需要）
- 其他特殊配置参数

**串口分配约定**：USART1（PA9/PA10）保留给单片机调试打印，不给模块使用；需要串口的模块（WiFi/蓝牙等）优先使用 USART2（PA2/PA3）或 USART3（PB10/PB11）。

**HAL 工程额外确认**：
- 工程是否已有 `bsp_delay.h` 提供微秒延时（没有则需一并生成，见"库适配规范 → 延时处理"）
- 调试打印走 `HAL_UART_Transmit()` 还是已重定向的 `printf()`

### 第 3 步：确定输出路径

1. 优先检查工程是否存在 `Module/` 目录（用户工程约定，传感器/模块驱动放这里）
2. 若存在 `Module/` 目录，驱动文件输出到 `Module/` 目录下
3. 若不存在 `Module/` 目录，**新建 `Module/`** 后输出；⚠️ 绝不输出到 `APP/` 或工程根目录（与本 skill「工程约定」第 3 条一致）
4. 输出文件名格式：`bsp_module.c` 和 `bsp_module.h`（module 为小写模块名，如 `bsp_dht11.c`、`bsp_oled.h`）
5. **CMake 登记（HAL + CMake 工程必做）**：确认工程 `CMakeLists.txt` 含以下三行，缺失则补上；CubeMX 重新生成工程后需复查是否被覆盖
   ```cmake
   file(GLOB MODULE_SOURCES "Module/*.c")
   target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MODULE_SOURCES})
   target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE Module)
   ```

### 第 4 步：生成驱动代码

1. 读取 `assets/driver_template.h` 和 `assets/driver_template.c` 获取输出格式模板
2. **按第 0 步检测的库类型加载对应参考**（**只读一个，不要两份都读**）：
   - **StdPeriph** → 读取 `references/stdperiph-guide.md`
   - **HAL / LL** → 读取 `references/hal-guide.md`
   
   从中获取对应通信协议的初始化与读写代码参考
   - ⚠️ **本仓库默认 HAL + CMake 工程，`stdperiph-guide.md`（19.6KB）不要读**——读了也用不上
   - ⚠️ **同一会话内只读一次**：生成第 2 个及以后的模块驱动时，复用已读到的 API 写法，不要重复 `Read`
   - 只需读该文件中**与本次通信协议相关的那一节**（如做 OLED 只看软件 I2C 部分），不必整份读
3. 若生成的是 **OLED 驱动**，必须读取 `assets/bsp_oledfont.h` 获取已验证的真实字库数据，作为 `bsp_oledfont.h` 输出（见下方"字库使用约定"）
4. **若生成的是 OneNET/WiFi/云通信相关代码**，必须读取 `assets/communication/` 下的已验证源码，直接复用并复制输出，不要现场从零编写（见下方"OneNET 通信代码生成约定"）
5. 按照编码规范（见下方）生成 .h 和 .c 文件
6. 将生成的代码写入第 3 步确定的输出路径

#### OneNET 通信代码生成约定（务必遵守）

**`assets/communication/` 目录已包含一套完整跑通的 OneNET + ESP8266 + MQTT + JSON 通信源码**，生成相关代码时直接读取并复制这些文件，不要重写：

- `esp8266.c/h`：ESP8266 AT 指令驱动（USART2 收发，`esp8266_buf[512]` 缓存 + `USART2_IRQHandler` 中断接收）。
  **接收层是修复版**：按 `+IPD,<len>:` 长度精确取帧、取帧后拷贝到独立缓存 `g_IpdFrame`、发送路径用 `ESP8266_DropAtEcho()` 只丢 AT 回显。
  ⚠️ **绝不要把 `ESP8266_Clear()` 加回发送路径或 `OneNet_RevPro` 结尾**——那会吃掉正在到达/排队中的下行帧，导致 App 下发的指令偶发收不到且串口无任何打印（详见 `references/onenet-guide.md` 第七节）
  **发送层也是修复版**：AT 应答关键字用 `ESP8266_FindInBuf()` 按字节查找（`strstr` 会被缓冲区里二进制 `+IPD` 帧的 `0x00` 截断，帧后面的 `>` / `SEND OK` 全部看不见，数据静默发不出去、平台收不到也没有 PUBACK）；每发完一包等 `SEND OK`（`ESP8266_WaitSendDone()`），拿不到 `>` 时等 300ms 重试一次
  ⚠️ **绝不要把 `ESP8266_FindInBuf()` 改回 `strstr()`，也不要删掉 `ESP8266_WaitSendDone()`**（详见 `references/onenet-guide.md` 第八节）
- `onenet.c/h`：OneNET 应用层。`onenet.h` 含**设备配置区**（`ONENET_PROID` / `ONENET_DEVID` / `ONENET_TOKEN`）与**物模型主题宏**（`ONENET_TOPIC_POST` / `_SET` / `_SET_REPLY`）；`onenet.c` 提供 `OneNet_DevLink` 连接 / `OneNet_Subscribe` 订阅（QoS1）/ `OneNet_Publish` 发布（QoS1）/ `OneNet_RevPro` 接收处理，以及**下行指令回调接口** `OneNet_RegisterPropertySetCb()` 与属性设置回执
- `MqttKit.c/h` + `Common.h`：MQTT 协议组包/解包层
- `cJSON.c/h`：JSON 解析（接收下行指令用）
- **依赖的 BSP 接口**（需在工程中已存在）：`bsp_usart.h`（`UsartPrintf`/`Usart_SendString`/`USART_DEBUG`=USART1）、`bsp_delay.h`（`DelayMs`/`DelayUs`）
- **串口分配**：ESP8266 用 **USART2**（PA2/PA3），USART1（PA9/PA10）保留给调试打印
- **依赖的堆空间**：MQTT 组包和 cJSON 解析都用 `malloc`，启动文件 `Heap_Size` 建议 ≥ `0x400`（1KB），否则组包会随机失败
- 生成后提醒用户修改两处**配置区**：`onenet.h` 顶部 `ONENET_PROID/ONENET_DEVID/ONENET_TOKEN`（OneNET 平台信息）、`esp8266.c` 顶部 WiFi 账号密码与 MQTT 服务器地址
- **应用层接入三件套**：①注册属性设置回调 ②订阅时把 `ONENET_TOPIC_SET` 一起带上 ③主循环里
  `while ((p = ESP8266_GetIPD(0)) != 0) OneNet_RevPro(p);`（`0`=非阻塞，一次把缓冲区里所有帧处理完）

> ⚠️ **HAL 工程注意**：`assets/communication/` 这套源码是**基于标准外设库**编写的（直接操作 `USART2_IRQHandler`、调用 `USART_SendData`/`USART_ReceiveData`）。
> 若目标工程是 HAL 库，不能直接照搬，需做适配：
> - 串口收发改用 `HAL_UART_Transmit()` / `HAL_UART_Receive_IT()`
> - 中断接收改用 `HAL_UART_RxCpltCallback()` 回调
> - **接收缓冲区管理（`ESP8266_DropRx` / `DropAtEcho` / `TailIsIpdHead` / `TryGetFrame`）与 AT 指令解析逻辑与库无关，必须整段保留**，
>   只替换 `ESP8266_Clear` / `SendData` / `SendCmd` / 中断里的串口收发调用即可
> - 适配时**只改串口收发层**，上层 `onenet.c` / `MqttKit.c` / `cJSON.c` 无需改动

### 第 5 步：补充说明

生成完成后提供：
- 文件使用说明（需要在 main.c 中调用的初始化函数）
- **依赖的底层库文件**（按第 0 步检测结论给出）：
  - StdPeriph：`stm32f10x_gpio.h` 等（I2C/SPI 软件模拟无需 `stm32f10x_i2c.h` / `stm32f10x_spi.h`）
  - HAL：`stm32f1xx_hal.h`（按需追加 `stm32f1xx_hal_uart.h` / `stm32f1xx_hal_tim.h` / `stm32f1xx_hal_adc.h`）
- 引脚连接说明
- 如有时序关键操作，提示延时要求
- **明确声明本驱动基于哪种库生成**，若工程库类型与生成时不一致需重新生成

---

## 库适配规范（生成代码前必读）

确定库类型后，**全程使用同一套 API**。以下是两种库的关键对照：

### 头文件包含

| 库类型 | 需包含的头文件 |
| --- | --- |
| StdPeriph | `stm32f10x.h`、`stm32f10x_gpio.h`、`stm32f10x_rcc.h`（按需加 `stm32f10x_usart.h` / `stm32f10x_tim.h` / `stm32f10x_adc.h`） |
| HAL | `stm32f1xx_hal.h`（已间接包含 GPIO/RCC；按需加 `stm32f1xx_hal_uart.h` / `stm32f1xx_hal_tim.h` / `stm32f1xx_hal_adc.h`） |

### GPIO 操作对照（最常用）

| 功能 | StdPeriph | HAL |
| --- | --- | --- |
| 时钟使能 | `RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);` | `__HAL_RCC_GPIOA_CLK_ENABLE();` |
| 引脚宏 | `GPIO_Pin_0` | `GPIO_PIN_0` |
| 初始化 | `GPIO_Init(GPIOA, &GPIO_InitStruct);` | `HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);` |
| 结构体成员 | `GPIO_Mode` / `GPIO_Speed` / `GPIO_Pin` | `Mode` / `Speed` / `Pin` / `Pull` |
| 推挽输出 | `GPIO_Mode_Out_PP` | `GPIO_MODE_OUTPUT_PP` |
| 开漏输出 | `GPIO_Mode_Out_OD` | `GPIO_MODE_OUTPUT_OD` |
| 上拉输入 | `GPIO_Mode_IPU` | `GPIO_MODE_INPUT` + `GPIO_PULLUP` |
| 浮空输入 | `GPIO_Mode_IN_FLOATING` | `GPIO_MODE_INPUT` + `GPIO_NOPULL` |
| 速度 | `GPIO_Speed_50MHz` | `GPIO_SPEED_FREQ_HIGH` |
| 输出高 | `GPIO_SetBits(GPIOA, GPIO_Pin_0);` | `HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);` |
| 输出低 | `GPIO_ResetBits(GPIOA, GPIO_Pin_0);` | `HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);` |
| 翻转 | 读 + 写两步 | `HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);` |
| 读输入 | `GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)` | `HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0)` |

> 注意 HAL 结构体多了 `Pull` 成员，输入模式**必须**显式设置 `Pull`，否则默认浮空可能读到不确定电平。

### 延时处理（两种库差异最大）

| 场景 | StdPeriph | HAL |
| --- | --- | --- |
| 毫秒延时 | `DelayMs(ms)`（`bsp_delay.h`） | `HAL_Delay(ms)`（HAL 自带，基于 SysTick） |
| 微秒延时 | `DelayUs(us)`（`bsp_delay.h`） | **HAL 没有现成的 us 级延时，必须自行解决** |

**HAL 工程的微秒延时方案**（按优先级）：
1. 若工程已有 `bsp_delay.h` 提供 `DelayUs()` → 直接复用
2. 用通用定时器实现（推荐 TIM2/TIM3，配置成 1MHz 计数，读 CNT 差值计时）
3. 用 `HAL_TIM_Base_Start()` + `__HAL_TIM_GET_COUNTER()` 组合实现
4. 对精度要求不高的场景，可用 `__NOP()` 空转近似（不精确，仅作保底，需按主频校准）

> ⚠️ DHT11、DS18B20、超声波等单总线/时序敏感模块**必须**先解决 us 延时再生成驱动，否则读不到数据。

### 串口（USART）

| 功能 | StdPeriph | HAL |
| --- | --- | --- |
| 初始化 | `USART_Init(USART1, &USART_InitStruct);` + `USART_Cmd()` | `HAL_UART_Init(&huart1);`（通常由 CubeMX 生成 `MX_USART1_UART_Init()`） |
| 发送字节 | `USART_SendData(USART1, ch);` + 等 TXE | `HAL_UART_Transmit(&huart1, &ch, 1, HAL_MAX_DELAY);` |
| 接收中断 | `USART_ITConfig()` + `USART1_IRQHandler` | `HAL_UART_Receive_IT()` + `HAL_UART_RxCpltCallback()` |
| 中断入口 | 自己写 `USARTx_IRQHandler` | HAL 已统一管理，用户只写**回调函数** |

### PWM（定时器）

| 功能 | StdPeriph | HAL |
| --- | --- | --- |
| 时基初始化 | `TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStruct);` | `HAL_TIM_PWM_Init(&htim2);` |
| 通道配置 | `TIM_OCInit()` + `TIM_OC1Init()` | `HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1);` |
| 启动 | `TIM_Cmd(TIM2, ENABLE);` + `TIM_CtrlPWMOutputs()` | `HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);` |
| 改占空比 | `TIM_SetCompare1(TIM2, val);` | `__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, val);` |

### ADC

| 功能 | StdPeriph | HAL |
| --- | --- | --- |
| 初始化 | `ADC_Init()` + `ADC_RegularChannelConfig()` | `HAL_ADC_Init()` + `HAL_ADC_ConfigChannel()` |
| 启动转换 | `ADC_SoftwareStartConvCmd(ADC1, ENABLE);` | `HAL_ADC_Start(&hadc1);` |
| 读值 | `ADC_GetConversionValue(ADC1)` | `HAL_ADC_PollForConversion()` + `HAL_ADC_GetValue()` |

### 软件模拟 I2C / SPI（两种库通用思路）

软件模拟的**时序逻辑在两种库下完全相同**，差异只在 GPIO 操作层。

**推荐做法**：按检测到的库类型**直接生成该库的原生代码**（清晰易读、便于教学与调试）。

若用户明确要求"一份驱动同时兼容两种库"，才使用条件编译宏抽象：

```c
/* ======================== 库类型适配 ======================== */
#ifdef USE_HAL_DRIVER          /* HAL 工程由 CubeMX 自动定义此宏 */
    #include "stm32f1xx_hal.h"
    #define MODULE_CLK_ENABLE()        __HAL_RCC_GPIOB_CLK_ENABLE()
    #define MODULE_PIN_H(port, pin)    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET)
    #define MODULE_PIN_L(port, pin)    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET)
    #define MODULE_PIN_R(port, pin)    HAL_GPIO_ReadPin(port, pin)
#else
    #include "stm32f10x.h"
    #define MODULE_CLK_ENABLE()        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE)
    #define MODULE_PIN_H(port, pin)    GPIO_SetBits(port, pin)
    #define MODULE_PIN_L(port, pin)    GPIO_ResetBits(port, pin)
    #define MODULE_PIN_R(port, pin)    GPIO_ReadInputDataBit(port, pin)
#endif
```

> `USE_HAL_DRIVER` 是 CubeMX/HAL 工程的标准全局宏，可直接用于判断，无需用户手动定义。

---

## 工程约定（用户工程通用规范，生成代码时务必遵守）

以下为用户工程中经过实测确认的固定约定，生成任何驱动代码时都必须遵循，无需每次向用户确认：

### 1. 延时函数名

**标准库（StdPeriph）工程**：
- 微秒延时：`DelayUs(unsigned short us)`
- 毫秒延时：`DelayMs(unsigned short ms)`
- ⚠️ **不要用** `Delay_us()` / `Delay_ms()`（skill 默认模板名不匹配用户工程）
- 对应头文件：`bsp_delay.h`

**HAL 工程**：
- 毫秒延时：`HAL_Delay(ms)`
- 微秒延时：优先复用工程的 `DelayUs()`；没有则按"库适配规范 → 延时处理"生成实现

### 2. 串口打印

**标准库（StdPeriph）工程**：
- 使用 `UsartPrintf(USART_DEBUG, ...)`（USART1/PA9/PA10 用于调试打印）
- ⚠️ **不要用** 标准库 `printf()`（用户工程未重定向标准 printf）
- 包含头文件：`bsp_usart.h`
- 常量：`USART_DEBUG`

**HAL 工程**：
- 使用 `HAL_UART_Transmit(&huart1, buf, len, HAL_MAX_DELAY)`
- 若工程已重定向 `printf`（实现过 `fputc`），可直接用 `printf()`
- 若工程存在 `bsp_usart.h`，沿用工程既有方式

### 3. 驱动文件输出目录
- 驱动文件统一放在工程 **`Module/`** 目录下
- ⚠️ **不要** 输出到 `APP/` 或工程根目录（用户工程没有 APP 目录）
- 若工程存在 `BSP/` 目录，基础外设（delay/usart）在 `BSP/` 下，传感器/模块驱动在 `Module/` 下

### 4. main.c 变量定义规则
- 所有变量（含全局变量）**定义在 main 函数外部**，不要在 main 函数内部定义
- 全局变量建议以 `g_` 前缀命名（如 `g_Temp`、`g_Humi`）

### 5. 标准库兼容性
- 优先使用 `0` 判断空指针，避免使用 `NULL`（Keil 环境可能未定义 `NULL` 导致编译报错）
- 若必须使用标准库宏，需确认包含对应头文件

### 6. 数据格式与打印
- OLED 显示数字时，先 `sprintf()` 转换再显示，注意像素点不重叠

### 7. OLED 显示语言（英文优先）
- OLED 上显示的固定文字尽量使用**英文**；较长英文用**缩写**（如温度 `Temp`、湿度 `Humi`、单位 `C`/`RH`）。
- ⚠️ **不要直接用中文**做 OLED 界面文字：OLED 字库 `hz[]` 只收录了少量汉字（当前仅"自手动模式网络连接中叶℃"12 字），字库外的汉字 `OLED_ShowCH` 会查不到匹配而**显示空白**。
- 需要新增汉字显示时，必须按 assets 字库约定用取模软件补充字模（见资源引用区的字库使用约定）。
- ASCII 显示可用 `OLED_ShowCH`（内部走 ASCII 字库 `zf[]`）或 `OLED_ShowChar`。

### 8. OLED_ShowCH 参数类型
- `OLED_ShowCH(u8 x, u8 y, u8 *chs)` 的第 3 参是 `u8 *`（即 `unsigned char *`）。
- 用 `char` 数组变量传入会触发 Keil `#167-D` 类型不兼容警告，应：
  - 显示缓冲区声明为 `unsigned char`（如 `unsigned char g_OledBuf[16];`）
  - 若该缓冲区同时用于 `sprintf`（其第 1 参是 `char *`），调用 `sprintf` 时显式强转 `(char *)g_OledBuf`

---

## 编码规范

### 命名规范
- 输出文件名：`bsp_module.c` / `bsp_module.h`（module 为小写模块名，如 `bsp_dht11.c`、`bsp_oled.h`）
- 函数命名：`MODULE_Action`（如 `DHT11_Init`、`DHT11_ReadData`、`Motor_SetSpeed`）
- 类型命名：`MODULE_TypeDef`（如 `MPU6050_AccelData`）
- 宏定义：全大写 + 下划线（如 `DHT11_GPIO_PIN`、`OLED_WIDTH`）
- 私有函数：`static` 修饰，仍遵循 `MODULE_Action` 命名

### 注释规范
- 文件头注释（**备注行按实际库类型填写**）：
  ```c
  /*
   * 文件名：MODULE.h
   * 描述  ：MODULE 驱动头文件
   * 备注  ：基于 STM32F103 标准外设库
   */
  ```
  HAL 工程改为：`* 备注  ：基于 STM32F103 HAL 库`
- 函数注释（中文块注释风格）：
  ```c
  /*
   * 函数名：DHT11_Init
   * 描述  ：初始化 DHT11 传感器
   * 输入  ：无
   * 输出  ：无
   */
  ```
  有参数时：
  ```c
  /*
   * 函数名：DHT11_ReadData
   * 描述  ：读取 DHT11 温湿度数据
   * 输入  ：无
   * 输出  ：temp-温度值, humi-湿度值
   */
  ```
  有返回值时：
  ```c
  /*
   * 函数名：DHT11_Check
   * 描述  ：检测 DHT11 传感器是否存在
   * 输入  ：无
   * 输出  ：0-存在, -1-不存在
   */
  ```
- 行内注释：使用 `/* 中文注释 */` 格式
- 分区注释：`/* ======================== 分区名 ======================== */`

### 通信协议规范
- I2C 和 SPI 统一使用 GPIO 软件模拟，不使用硬件 I2C/SPI 外设
- 优势：引脚分配灵活，可使用任意 GPIO，避免硬件外设引脚冲突
- 软件模拟 I2C：开漏输出 + 外部上拉，手动翻转 SCL/SDA 实现时序
- 软件模拟 SPI：推挽输出，手动翻转 SCK/MOSI/CS 实现时序
- USART 和 PWM（定时器）仍使用硬件外设

### 头文件规范
- 使用 `#ifndef __BSP_MODULE_H` / `#define __BSP_MODULE_H` / `#endif` 保护
- 必须包含：引脚宏定义区、类型定义区（如需要）、函数声明区
- 函数声明需带中文块注释

### 源文件规范
- 包含自身头文件和所需的底层库头文件（**按第 0 步检测结论选择 StdPeriph 或 HAL**）
- 分区组织：私有变量区 → 私有函数区 → 初始化函数 → 核心功能函数
- 核心操作使用中文行内注释说明关键步骤
- 延时函数按库类型选择（StdPeriph 用 `DelayMs()`/`DelayUs()`，HAL 用 `HAL_Delay()` + 自实现 us 延时）

### 引脚宏定义格式

**标准外设库（StdPeriph）**：
```c
#define MODULE_SDA_GPIO_PORT  GPIOA
#define MODULE_SDA_GPIO_PIN   GPIO_Pin_0
#define MODULE_SDA_GPIO_CLK   RCC_APB2Periph_GPIOA
```

**HAL 库**：
```c
#define MODULE_SDA_GPIO_PORT  GPIOA
#define MODULE_SDA_GPIO_PIN   GPIO_PIN_0
#define MODULE_SDA_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()
```

引脚宏按功能命名（如 SCL/SDA/MOSI/MISO/SCK/CS/TRIG/ECHO），不使用 xxx 占位

## 资源引用

### references/sensor-catalog.md
传感器/外设模块目录，包含各模块的通信协议、典型引脚连接、关键时序参数和必备函数列表。在第 1 步中按需加载，搜索关键词定位具体模块。

### references/stdperiph-guide.md
**标准外设库（StdPeriph）** API 速查参考，覆盖 GPIO/I2C/SPI/USART/TIM/NVIC 的初始化模板代码。检测为 StdPeriph 工程时，在第 4 步加载。

### references/hal-guide.md
**HAL 库** API 速查参考，覆盖 GPIO/USART/TIM-PWM/ADC/延时/NVIC 的初始化模板代码，含 CubeMX 工程结构与回调机制说明。检测为 HAL 工程时，在第 4 步加载。

### references/onenet-setup-guide.md
**OneNET 平台侧配置指南**（据课件 14/15/16 章整理）：注册 → 创建产品（MQTT / OneJSON / WIFI / 自定义方案）
→ 创建设备 → 创建物模型 → 生成 Token → **用 MQTT.fx 先验证平台，不碰单片机**。
含课件给的物模型标识符示例（`alarm`/`fan_on`/`mode`/`fan_duty`/`temp`/`temp_high`/`temp_low`）、
代码侧必改的 5 处参数、三层验证顺序（串口→平台→App），以及**课件内部两处矛盾**
（Broker 域名两篇写法不同、物模型标识符两套对不上）。
生成云通信代码前先读这份，能省掉大量"到底哪个名字才对"的来回。

### references/onenet-guide.md
OneNET 云平台接入要点（属性上报 + 指令下发），含**四个已实测的核心坑点**：
①属性值必须 `{"value":...}` 包装（写成 `{"temp":20}` 会被平台静默丢弃，ACK 成功但数据不更新）；
②**bool 类型属性必须上报 `true`/`false`，写成 `1`/`0` 会因 params 整包校验失败导致所有属性都不更新**；
③**ESP8266 接收缓冲区管理**——旧参考代码在发送路径上调用 `ESP8266_Clear()`、`WaitRecive()` 里清零计数、`OneNet_RevPro` 结尾清缓冲，会把正在到达或排队中的下行帧吃掉，表现为"App 点了开关设备收不到、串口一个字都不打印"，属偶发竞态，极难排查；
④**ESP8266 发送方向的静默失败**——`ESP8266_SendCmd()` 原来是 `strstr` 找 AT 应答关键字，缓冲区里若有还没被应用层取走的二进制 `+IPD` 帧（含 `0x00`），`strstr` 到这里就停，帧后面的 `>` / `SEND OK` 全部看不见，数据一个字节都发不出去且无任何报错；表现为"订阅能成功、上报永远失败、平台没有 PUBACK、物模型数据不更新"，且 ESP8266 会卡在"等数据"状态导致**永久失败**（必须断电重启）。修法：`ESP8266_FindInBuf()` 按字节查找 + `ESP8266_WaitSendDone()` 等 `SEND OK` + 拿不到 `>` 时重试。
另含正确上报 JSON 模板、bool 混排的多属性模板、以及三套排查清单（"平台数据不更新"、"设备收不到 App 指令"、"平台收不到上报"）。生成 ESP8266/WiFi/OneNET/云平台通信代码时**必读**。

### assets/communication/
**一套完整跑通并实测可用的 OneNET 通信源码**（ESP8266 AT 驱动 + onenet 应用层 + MqttKit 协议层 + cJSON）。生成 OneNET/WiFi/云通信代码时**整份复制这些文件**，不要现场从零编写。包含：
- `esp8266.c/h`（USART2 收发、AT 指令、WiFi/服务器连接）。**接收层为修复版**：按 `+IPD,<len>:` 长度精确取帧、取帧后拷贝到独立缓存、发送路径只丢 AT 回显、缓冲区操作带临界区。**发送层同样为修复版**：AT 应答关键字用 `ESP8266_FindInBuf()` 按字节查找（不用 `strstr`）、每发完一包等 `SEND OK`、拿不到 `>` 时重试一次。旧实现在文件内以 `#if 0` 保留作对照
- `onenet.c/h`（`onenet.h` 含设备配置区与物模型主题宏；`onenet.c` 含 `OneNet_DevLink` / `OneNet_Subscribe`（QoS1）/ `OneNet_Publish`（QoS1）/ `OneNet_RevPro` / `OneNet_RegisterPropertySetCb` 下行指令回调）
- `MqttKit.c/h` + `Common.h`（MQTT 组包/解包）
- `cJSON.c/h`（JSON 解析）
- 依赖 `bsp_usart.h`、`bsp_delay.h`；需要 `Heap_Size ≥ 0x400`
- ⚠️ 本套源码基于 **StdPeriph** 编写，HAL 工程需适配串口收发层，但**接收缓冲区管理逻辑必须整段保留**（见第 4 步"OneNET 通信代码生成约定"）

### assets/driver_template.h 和 assets/driver_template.c
驱动文件模板，包含占位符标记。在第 4 步生成代码时读取，替换占位符后输出。

### assets/bsp_oledfont.h
OLED 已验证字库文件（8×16 ASCII + 16×16 汉字），取模方式为**共阴——列行式——逆向输出**。生成 OLED 驱动时，直接读取此文件内容作为 `bsp_oledfont.h` 输出，不要凭空编造点阵数据。

**字库使用约定（务必遵守）：**
- `zf[]` 对应 `zf_index[]`（95 个可打印 ASCII），`hz[]` 对应 `hz_index[]`（当前 12 个字：自手动模式网络连接中叶℃）。
- **字库顺序必须与取模顺序一致，且不能有相同字。**
- ⚠️ **新增汉字无法凭空生成点阵**：如需显示字库以外的汉字，必须让用户用取模软件（如 PCtoLCD2002）按"共阴—列行式—逆向输出"生成 32 字节数据，追加到 `hz[]`，并在 `hz_index[]` 末尾同步追加该汉字，再更新 `HZ = Oledfont_NUM()` 计数。
- 生成驱动时如用户的显示内容超出 assets 字库覆盖范围，应提示用户提供取模数据，而不是伪造数据。
