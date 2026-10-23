# HAL 库 API 速查参考（STM32F1 系列）

> 当检测到目标工程为 **HAL 库** 时加载本文件，用于生成驱动代码。
> 标准外设库工程请改用 `stdperiph-guide.md`。

## 目录

- [一、如何识别 HAL 工程](#一如何识别-hal-工程)
- [二、CubeMX 工程注意事项](#二cubemx-工程注意事项)
- [三、GPIO](#三gpio)
- [四、延时（重点：HAL 没有微秒延时）](#四延时重点hal-没有微秒延时)
- [五、USART 串口](#五usart-串口)
- [六、TIM PWM 输出](#六tim-pwm-输出)
- [七、ADC 采集](#七adc-采集)
- [八、外部中断](#八外部中断)
- [九、软件模拟 I2C / SPI](#九软件模拟-i2c--spi)

---

## 一、如何识别 HAL 工程

出现以下任一特征即可判定为 HAL 工程：

| 特征 | 说明 |
| --- | --- |
| 文件 | `stm32f1xx_hal.h`、`stm32f1xx_hal_conf.h`、`Drivers/STM32F1xx_HAL_Driver/` |
| main.c | 含 `HAL_Init()`、`SystemClock_Config()`、`MX_GPIO_Init()` 等 `MX_xxx_Init()` 函数 |
| 全局宏 | 定义了 `USE_HAL_DRIVER` |
| 工程文件 | 存在 `.ioc`（CubeMX 工程文件） |
| 句柄 | 使用 `UART_HandleTypeDef`、`TIM_HandleTypeDef` 等句柄结构体 |

> 若同时存在 `stm32f1xx_ll_*.h`，说明混用 LL 库。GPIO 操作可用 `LL_GPIO_SetOutputPin()` 等 LL 函数，其余按 HAL 处理。

---

## 二、CubeMX 工程注意事项

生成代码放入 CubeMX 工程时，**必须提醒用户**：

1. **用户代码要写在保护块内**，否则重新生成工程时会被覆盖：
   ```c
   /* USER CODE BEGIN 2 */
   DHT11_Init();
   /* USER CODE END 2 */
   ```

2. **外设句柄**（`huart1`、`htim2`、`hadc1` 等）由 CubeMX 在 `main.c` 中定义。若驱动文件需要使用，应在头文件中用 `extern` 声明：
   ```c
   extern UART_HandleTypeDef huart2;   /* 在 main.c 中定义 */
   ```

3. **外设初始化**通常由 `MX_xxx_Init()` 完成，驱动里**不要重复初始化时钟和外设**，只做模块自身的初始化（如 GPIO 已由 CubeMX 配置好则直接使用）。

4. **中断服务函数**由 HAL 统一管理（在 `stm32f1xx_it.c` 中调用 `HAL_UART_IRQHandler()`），用户只需重写**回调函数**，不要自己写 `USART2_IRQHandler()`。

---

## 三、GPIO

### 初始化（手动方式，非 CubeMX 生成时）

```c
GPIO_InitTypeDef GPIO_InitStruct = {0};

__HAL_RCC_GPIOA_CLK_ENABLE();                 /* 使能时钟 */

GPIO_InitStruct.Pin   = GPIO_PIN_0;
GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;  /* 推挽输出 */
GPIO_InitStruct.Pull  = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

### 模式宏对照

| 用途 | Mode | Pull |
| --- | --- | --- |
| 推挽输出 | `GPIO_MODE_OUTPUT_PP` | `GPIO_NOPULL` |
| 开漏输出（软件 I2C 用） | `GPIO_MODE_OUTPUT_OD` | `GPIO_NOPULL`（外部上拉）或 `GPIO_PULLUP` |
| 上拉输入（按键用） | `GPIO_MODE_INPUT` | `GPIO_PULLUP` |
| 下拉输入 | `GPIO_MODE_INPUT` | `GPIO_PULLDOWN` |
| 浮空输入 | `GPIO_MODE_INPUT` | `GPIO_NOPULL` |
| 复用推挽（USART/TIM） | `GPIO_MODE_AF_PP` | `GPIO_NOPULL` |

> ⚠️ F1 系列 HAL 的 `GPIO_InitTypeDef` 成员为 `Pin`、`Mode`、`Pull`、`Speed`（部分版本含 `Alternate`）。
> 输入模式**必须显式设置 Pull**，否则默认浮空，按键/单总线会读到不确定电平。

### 读写操作

```c
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);    /* 输出高 */
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);  /* 输出低 */
HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);                 /* 翻转 */
GPIO_PinState st = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);/* 读输入 */
```

`GPIO_PinState` 取值为 `GPIO_PIN_SET` / `GPIO_PIN_RESET`。

---

## 四、延时（重点：HAL 没有微秒延时）

### 毫秒延时

```c
HAL_Delay(100);   /* 延时 100ms，基于 SysTick，HAL 自带 */
```

### 微秒延时（HAL 缺失，必须自行实现）

**方案一：通用定时器实现（推荐，精度高）**

以 TIM2 为例，配置成 1 MHz 计数（1 个计数 = 1 µs）：

```c
/* 定时器基础配置（72MHz 主频，预分频 72-1 → 1MHz） */
htim2.Instance = TIM2;
htim2.Init.Prescaler = 72 - 1;
htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
htim2.Init.Period = 0xFFFF;
htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
HAL_TIM_Base_Init(&htim2);
```

```c
/*
 * 函数名：DelayUs
 * 描述  ：微秒级延时（基于 TIM2 计数器）
 * 输入  ：us - 延时微秒数（建议 < 60000）
 * 输出  ：无
 */
void DelayUs(unsigned short us)
{
    __HAL_TIM_SET_COUNTER(&htim2, 0);      /* 计数器清零 */
    HAL_TIM_Base_Start(&htim2);            /* 启动定时器 */
    while (__HAL_TIM_GET_COUNTER(&htim2) < us);  /* 等待计数值到达 */
    HAL_TIM_Base_Stop(&htim2);             /* 停止定时器 */
}
```

> ⚠️ `HAL_TIM_Base_Start/Stop` 本身有函数调用开销（约 1~2 µs），对 DHT11 等严格要求 µs 级时序的模块，
> 建议**只启动一次、全程不停止**，仅清零计数器：
> ```c
> HAL_TIM_Base_Start(&htim2);              /* 初始化时启动一次 */
> /* 每次延时：清零 → 等待 */
> __HAL_TIM_SET_COUNTER(&htim2, 0);
> while (__HAL_TIM_GET_COUNTER(&htim2) < us);
> ```

**方案二：`__NOP()` 空转（精度低，仅作保底）**

```c
/*
 * 函数名：DelayUs
 * 描述  ：微秒级延时（空转近似，72MHz 主频下校准）
 * 输入  ：us - 延时微秒数
 * 输出  ：无
 */
void DelayUs(unsigned short us)
{
    unsigned short i;
    while (us--)
    {
        i = 8;                 /* 需按实际主频与编译优化等级校准 */
        while (i--) __NOP();
    }
}
```

> 该方案受**编译优化等级**影响极大（`-O0` 与 `-O3` 差异可达数倍），仅用于精度要求不高的场景。

---

## 五、USART 串口

### 轮询发送（简单场景）

```c
uint8_t ch = 'A';
HAL_UART_Transmit(&huart2, &ch, 1, HAL_MAX_DELAY);      /* 发送单字节 */
HAL_UART_Transmit(&huart2, buf, len, HAL_MAX_DELAY);    /* 发送缓冲区 */
```

### 中断接收（推荐，用于 ESP8266 等模块）

启动接收（一般在初始化时调用一次）：

```c
uint8_t g_Uart2Byte;                                /* 单字节接收缓冲 */
HAL_UART_Receive_IT(&huart2, &g_Uart2Byte, 1);      /* 开启中断接收 */
```

重写接收完成回调（**不要自己写 `USART2_IRQHandler`**）：

```c
/*
 * 函数名：HAL_UART_RxCpltCallback
 * 描述  ：串口接收完成回调函数（每收到 1 字节触发一次）
 * 输入  ：huart - 触发回调的串口句柄
 * 输出  ：无
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        /* 在此处理接收到的字节 g_Uart2Byte，如存入环形缓冲 */
        // esp8266_buf[esp8266_cnt++] = g_Uart2Byte;

        HAL_UART_Receive_IT(&huart2, &g_Uart2Byte, 1);   /* 必须重新开启，否则只接收一次 */
    }
}
```

> ⚠️ **关键**：中断模式下每次接收完成后**必须重新调用 `HAL_UART_Receive_IT()`**，否则只能收到第一个字节。

### printf 重定向（可选）

若工程已重定向，可直接使用 `printf()`。重定向方式：

```c
#include <stdio.h>
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
```

---

## 六、TIM PWM 输出

### 初始化（以 TIM2_CH1 / PA0 为例）

```c
TIM_HandleTypeDef htim2;
TIM_OC_InitTypeDef sConfigOC = {0};

__HAL_RCC_TIM2_CLK_ENABLE();
__HAL_RCC_GPIOA_CLK_ENABLE();

/* PA0 配置为复用推挽 */
GPIO_InitTypeDef GPIO_InitStruct = {0};
GPIO_InitStruct.Pin   = GPIO_PIN_0;
GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* 时基：1kHz PWM（72MHz / 72 / 1000） */
htim2.Instance = TIM2;
htim2.Init.Prescaler = 72 - 1;
htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
htim2.Init.Period = 1000 - 1;
htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
HAL_TIM_PWM_Init(&htim2);

/* PWM 通道配置 */
sConfigOC.OCMode     = TIM_OCMODE_PWM1;
sConfigOC.Pulse      = 500;                    /* 初始占空比 50% */
sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1);

/* 启动 PWM 输出 */
HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
```

### 修改占空比

```c
/*
 * 函数名：Fan_SetDuty
 * 描述  ：设置风扇 PWM 占空比
 * 输入  ：duty - 占空比 0~100
 * 输出  ：无
 */
void Fan_SetDuty(unsigned char duty)
{
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, duty * 10);   /* Period=999 → 0~1000 映射 */
}
```

---

## 七、ADC 采集

### 轮询方式（简单场景）

```c
/*
 * 函数名：ADC_GetValue
 * 描述  ：读取指定 ADC 通道的转换值
 * 输入  ：无
 * 输出  ：12位转换结果 0~4095
 */
unsigned short ADC_GetValue(void)
{
    HAL_ADC_Start(&hadc1);                    /* 启动转换 */
    HAL_ADC_PollForConversion(&hadc1, 10);    /* 等待转换完成，超时 10ms */
    return (unsigned short)HAL_ADC_GetValue(&hadc1);
}
```

### 多通道 / DMA 采集

需要连续采样时使用 DMA：

```c
HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_AdcBuf, ADC_BUF_LEN);
/* 转换完成回调 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    /* g_AdcBuf 中已是完整一轮采样数据 */
}
```

---

## 八、外部中断

```c
/* 初始化：PA0 上升沿触发 */
GPIO_InitStruct.Pin  = GPIO_PIN_0;
GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
GPIO_InitStruct.Pull = GPIO_PULLDOWN;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

HAL_NVIC_SetPriority(EXTI0_IRQn, 2, 0);
HAL_NVIC_EnableIRQ(EXTI0_IRQn);
```

```c
/* 回调函数 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        /* 处理中断 */
    }
}
```

触发模式宏：`GPIO_MODE_IT_RISING`、`GPIO_MODE_IT_FALLING`、`GPIO_MODE_IT_RISING_FALLING`。

---

## 九、软件模拟 I2C / SPI

软件模拟的**时序逻辑与库无关**，仅 GPIO 操作层不同。HAL 下的典型实现：

### 软件 I2C

```c
/* SCL/SDA 配置为开漏输出 + 外部上拉 */
#define I2C_SCL_H()   HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET)
#define I2C_SCL_L()   HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET)
#define I2C_SDA_H()   HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET)
#define I2C_SDA_L()   HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET)
#define I2C_SDA_R()   HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN)

/*
 * 函数名：I2C_Start
 * 描述  ：产生 I2C 起始信号
 * 输入  ：无
 * 输出  ：无
 */
void I2C_Start(void)
{
    I2C_SDA_H();
    I2C_SCL_H();
    DelayUs(5);
    I2C_SDA_L();      /* SCL 高电平期间 SDA 下降沿 = 起始信号 */
    DelayUs(5);
    I2C_SCL_L();
}
```

> 时序延时使用 `DelayUs()`，务必先按第四节解决 HAL 的微秒延时问题。

### 切换输入/输出方向（读取 SDA 时）

HAL 下切换 GPIO 方向需重新调用 `HAL_GPIO_Init()`，为避免频繁切换，可直接将 SDA 配置为**开漏输出 + 上拉**，读数据时直接 `HAL_GPIO_ReadPin()` 读取（开漏输出状态下仍可读取引脚电平）。

---

## 十、常见编译问题

| 问题 | 原因 | 解决 |
| --- | --- | --- |
| `undefined reference to huart2` | 句柄在 main.c 定义 | 驱动头文件中 `extern UART_HandleTypeDef huart2;` |
| 中断只进一次 | 未重新开启接收 | 回调末尾重新调用 `HAL_UART_Receive_IT()` |
| `HAL_Delay()` 卡死 | SysTick 优先级过低，在中断中被调用 | 调高 SysTick 抢占优先级，或避免在中断中用 `HAL_Delay()` |
| PWM 无输出 | 未调用 `HAL_TIM_PWM_Start()` | 初始化后必须显式启动通道 |
| GPIO 读值一直为 0 | 输入模式未设置 Pull | 显式设置 `GPIO_PULLUP` / `GPIO_PULLDOWN` |
| 软件 I2C 无响应 | 微秒延时不准 | 用定时器实现 `DelayUs()`，不要用 `__NOP()` 近似 |
