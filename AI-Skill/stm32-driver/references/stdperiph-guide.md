# STM32F103 标准外设库（StdPeriph）API 速查

本文档提供 STM32F103 StdPeriph 驱动开发中常用外设的初始化模板代码，供生成驱动时直接引用。

---

## 1. GPIO 配置

### 使能 GPIO 时钟

```c
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
```

### GPIO 输出配置（推挽输出）

```c
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   // 推挽输出
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOA, &GPIO_InitStructure);
```

### GPIO 输入配置（上拉输入）

```c
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;      // 上拉输入
GPIO_Init(GPIOA, &GPIO_InitStructure);
```

### GPIO 输入配置（浮空输入，用于 I2C 等开漏模式）

```c
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;    // 复用开漏输出（硬件 I2C）
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOB, &GPIO_InitStructure);
```

### GPIO 开漏输出（模拟 I2C/SPI）

```c
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;   // 开漏输出
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOB, &GPIO_InitStructure);
```

### GPIO 操作

```c
GPIO_SetBits(GPIOA, GPIO_Pin_0);       // 置高
GPIO_ResetBits(GPIOA, GPIO_Pin_0);     // 置低
GPIO_WriteBit(GPIOA, GPIO_Pin_0, Bit_SET);   // 写指定位
uint8_t bit = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);  // 读输入
uint8_t bit = GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_0); // 读输出
```

---

## 2. 软件模拟 I2C

统一使用 GPIO 软件模拟 I2C，不使用硬件 I2C 外设，可灵活分配任意 GPIO 引脚。

### 引脚初始化（开漏输出 + 外部上拉）

```c
GPIO_InitTypeDef GPIO_InitStructure;

/* 使能 GPIO 时钟 */
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

/* SCL 和 SDA 配置为开漏输出，外部接上拉电阻 */
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;       /* SCL */
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; /* 开漏输出 */
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOB, &GPIO_InitStructure);

GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;       /* SDA */
GPIO_Init(GPIOB, &GPIO_InitStructure);

/* 总线空闲，SCL 和 SDA 拉高 */
GPIO_SetBits(GPIOB, GPIO_Pin_6);
GPIO_SetBits(GPIOB, GPIO_Pin_7);
```

### SCL/SDA 宏定义模板

```c
#define I2C_SCL_GPIO_PORT   GPIOB
#define I2C_SCL_GPIO_PIN    GPIO_Pin_6
#define I2C_SCL_GPIO_CLK    RCC_APB2Periph_GPIOB

#define I2C_SDA_GPIO_PORT   GPIOB
#define I2C_SDA_GPIO_PIN    GPIO_Pin_7
#define I2C_SDA_GPIO_CLK    RCC_APB2Periph_GPIOB

#define I2C_SCL_HIGH()      GPIO_SetBits(I2C_SCL_GPIO_PORT, I2C_SCL_GPIO_PIN)
#define I2C_SCL_LOW()       GPIO_ResetBits(I2C_SCL_GPIO_PORT, I2C_SCL_GPIO_PIN)
#define I2C_SDA_HIGH()      GPIO_SetBits(I2C_SDA_GPIO_PORT, I2C_SDA_GPIO_PIN)
#define I2C_SDA_LOW()       GPIO_ResetBits(I2C_SDA_GPIO_PORT, I2C_SDA_GPIO_PIN)
#define I2C_SDA_READ()      GPIO_ReadInputDataBit(I2C_SDA_GPIO_PORT, I2C_SDA_GPIO_PIN)
```

### 起始信号

```c
static void I2C_Start(void)
{
    I2C_SDA_HIGH();
    I2C_SCL_HIGH();
    DelayUs(5);
    I2C_SDA_LOW();    /* SCL 高电平时 SDA 下降沿 */
    DelayUs(5);
    I2C_SCL_LOW();
    DelayUs(5);
}
```

### 停止信号

```c
static void I2C_Stop(void)
{
    I2C_SDA_LOW();
    I2C_SCL_HIGH();
    DelayUs(5);
    I2C_SDA_HIGH();   /* SCL 高电平时 SDA 上升沿 */
    DelayUs(5);
}
```

### 发送一个字节

```c
static void I2C_SendByte(uint8_t byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80)
            I2C_SDA_HIGH();
        else
            I2C_SDA_LOW();
        byte <<= 1;
        DelayUs(2);
        I2C_SCL_HIGH();
        DelayUs(5);
        I2C_SCL_LOW();
        DelayUs(2);
    }
}
```

### 读取一个字节

```c
static uint8_t I2C_ReadByte(uint8_t ack)
{
    uint8_t i, byte = 0;
    
    I2C_SDA_HIGH();   /* 释放 SDA，准备读取 */
    
    for (i = 0; i < 8; i++)
    {
        I2C_SCL_HIGH();
        DelayUs(5);
        byte <<= 1;
        if (I2C_SDA_READ())
            byte |= 0x01;
        I2C_SCL_LOW();
        DelayUs(2);
    }
    
    /* 发送 ACK/NACK */
    if (ack)
        I2C_SDA_LOW();   /* ACK */
    else
        I2C_SDA_HIGH();  /* NACK */
    DelayUs(2);
    I2C_SCL_HIGH();
    DelayUs(5);
    I2C_SCL_LOW();
    DelayUs(2);
    
    I2C_SDA_HIGH();   /* 释放 SDA */
    
    return byte;
}
```

### 等待 ACK

```c
static int8_t I2C_WaitAck(void)
{
    int8_t ack;
    
    I2C_SDA_HIGH();
    DelayUs(2);
    I2C_SCL_HIGH();
    DelayUs(5);
    
    if (I2C_SDA_READ())
        ack = -1;     /* NACK */
    else
        ack = 0;      /* ACK */
    
    I2C_SCL_LOW();
    DelayUs(2);
    
    return ack;
}
```

### 写寄存器

```c
void I2C_WriteReg(uint8_t devAddr, uint8_t reg, uint8_t data)
{
    I2C_Start();
    I2C_SendByte(devAddr);     /* 设备地址 + 写 */
    I2C_WaitAck();
    I2C_SendByte(reg);         /* 寄存器地址 */
    I2C_WaitAck();
    I2C_SendByte(data);        /* 数据 */
    I2C_WaitAck();
    I2C_Stop();
}
```

### 读寄存器（多字节）

```c
void I2C_ReadReg(uint8_t devAddr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    I2C_Start();
    I2C_SendByte(devAddr);     /* 设备地址 + 写 */
    I2C_WaitAck();
    I2C_SendByte(reg);         /* 寄存器地址 */
    I2C_WaitAck();
    
    I2C_Start();               /* Restart */
    I2C_SendByte(devAddr | 1); /* 设备地址 + 读 */
    I2C_WaitAck();
    
    while (len)
    {
        *buf++ = I2C_ReadByte(len == 1 ? 0 : 1);  /* 最后一个字节发 NACK */
        len--;
    }
    
    I2C_Stop();
}
```

---

## 3. 软件模拟 SPI

统一使用 GPIO 软件模拟 SPI，不使用硬件 SPI 外设，可灵活分配任意 GPIO 引脚。

### 引脚初始化（推挽输出）

```c
GPIO_InitTypeDef GPIO_InitStructure;

/* 使能 GPIO 时钟 */
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

/* SCK, MOSI, CS 配置为推挽输出 */
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;  /* SCK, MOSI */
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;         /* 推挽输出 */
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOA, &GPIO_InitStructure);

/* CS 引脚单独配置 */
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;                /* CS */
GPIO_Init(GPIOA, &GPIO_InitStructure);

/* MISO 配置为上拉输入 */
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;                /* MISO */
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
GPIO_Init(GPIOA, &GPIO_InitStructure);

/* CS 默认高电平（未选中），SCK 默认低电平（CPOL=0） */
GPIO_SetBits(GPIOA, GPIO_Pin_4);   /* CS = 1 */
GPIO_ResetBits(GPIOA, GPIO_Pin_5); /* SCK = 0 */
```

### SCK/MOSI/MISO/CS 宏定义模板

```c
#define SPI_SCK_GPIO_PORT    GPIOA
#define SPI_SCK_GPIO_PIN     GPIO_Pin_5
#define SPI_SCK_GPIO_CLK     RCC_APB2Periph_GPIOA

#define SPI_MOSI_GPIO_PORT   GPIOA
#define SPI_MOSI_GPIO_PIN    GPIO_Pin_7
#define SPI_MOSI_GPIO_CLK    RCC_APB2Periph_GPIOA

#define SPI_MISO_GPIO_PORT   GPIOA
#define SPI_MISO_GPIO_PIN    GPIO_Pin_6
#define SPI_MISO_GPIO_CLK    RCC_APB2Periph_GPIOA

#define SPI_CS_GPIO_PORT     GPIOA
#define SPI_CS_GPIO_PIN      GPIO_Pin_4
#define SPI_CS_GPIO_CLK      RCC_APB2Periph_GPIOA

#define SPI_SCK_HIGH()       GPIO_SetBits(SPI_SCK_GPIO_PORT, SPI_SCK_GPIO_PIN)
#define SPI_SCK_LOW()        GPIO_ResetBits(SPI_SCK_GPIO_PORT, SPI_SCK_GPIO_PIN)
#define SPI_MOSI_HIGH()      GPIO_SetBits(SPI_MOSI_GPIO_PORT, SPI_MOSI_GPIO_PIN)
#define SPI_MOSI_LOW()       GPIO_ResetBits(SPI_MOSI_GPIO_PORT, SPI_MOSI_GPIO_PIN)
#define SPI_MISO_READ()      GPIO_ReadInputDataBit(SPI_MISO_GPIO_PORT, SPI_MISO_GPIO_PIN)
#define SPI_CS_LOW()         GPIO_ResetBits(SPI_CS_GPIO_PORT, SPI_CS_GPIO_PIN)
#define SPI_CS_HIGH()        GPIO_SetBits(SPI_CS_GPIO_PORT, SPI_CS_GPIO_PIN)
```

### SPI 读写一个字节（Mode0: CPOL=0, CPHA=0, MSB 优先）

```c
static uint8_t SPI_ReadWriteByte(uint8_t byte)
{
    uint8_t i, read = 0;
    
    for (i = 0; i < 8; i++)
    {
        /* MOSI 输出 */
        if (byte & 0x80)
            SPI_MOSI_HIGH();
        else
            SPI_MOSI_LOW();
        byte <<= 1;
        DelayUs(2);
        
        /* SCK 上升沿，从机采样 MOSI */
        SPI_SCK_HIGH();
        DelayUs(2);
        
        /* MISO 读取，主机在上升沿后采样 */
        read <<= 1;
        if (SPI_MISO_READ())
            read |= 0x01;
        
        /* SCK 下降沿 */
        SPI_SCK_LOW();
        DelayUs(2);
    }
    
    return read;
}
```

### SPI 读写一个字节（Mode3: CPOL=1, CPHA=1, MSB 优先）

```c
static uint8_t SPI_ReadWriteByte_Mode3(uint8_t byte)
{
    uint8_t i, read = 0;
    
    SPI_SCK_HIGH();  /* CPOL=1，空闲高电平 */
    
    for (i = 0; i < 8; i++)
    {
        /* SCK 下降沿，主机输出 MOSI */
        SPI_SCK_LOW();
        if (byte & 0x80)
            SPI_MOSI_HIGH();
        else
            SPI_MOSI_LOW();
        byte <<= 1;
        DelayUs(2);
        
        /* SCK 上升沿，采样 MISO */
        SPI_SCK_HIGH();
        DelayUs(2);
        read <<= 1;
        if (SPI_MISO_READ())
            read |= 0x01;
    }
    
    return read;
}
```

### CS 片选控制

```c
/* 选中从机 */
SPI_CS_LOW();

/* 通信操作... */
/* SPI_ReadWriteByte(cmd); */
/* SPI_ReadWriteByte(data); */

/* 释放从机 */
SPI_CS_HIGH();
```

---

## 4. USART 配置

### 使能时钟

```c
RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
```

### USART1 初始化（PA9=TX, PA10=RX）

```c
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;    // TX
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOA, &GPIO_InitStructure);

GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;   // RX
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
GPIO_Init(GPIOA, &GPIO_InitStructure);

USART_InitTypeDef USART_InitStructure;
USART_InitStructure.USART_BaudRate = 115200;
USART_InitStructure.USART_WordLength = USART_WordLength_8b;
USART_InitStructure.USART_StopBits = USART_StopBits_1;
USART_InitStructure.USART_Parity = USART_Parity_No;
USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
USART_Init(USART1, &USART_InitStructure);
USART_Cmd(USART1, ENABLE);
```

### USART 中断接收配置

```c
USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

NVIC_InitTypeDef NVIC_InitStructure;
NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
NVIC_Init(&NVIC_InitStructure);
```

### USART 发送数据

```c
void USART_SendByte(USART_TypeDef *USARTx, uint8_t data)
{
    USART_SendData(USARTx, data);
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET);
}

void USART_SendString(USART_TypeDef *USARTx, char *str)
{
    while (*str)
    {
        USART_SendByte(USARTx, *str++);
    }
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);
}
```

---

## 5. 定时器（TIM）配置

### 使能时钟

```c
RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);   // TIM2~7 在 APB1
RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);   // TIM1 在 APB2
```

### 基本定时器（定时中断）

```c
void TIM2_Init(uint16_t arr, uint16_t psc)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period = arr;           // 自动重装载值
    TIM_TimeBaseStructure.TIM_Prescaler = psc;        // 预分频
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}
```

**定时时间计算**：`T = (arr+1) * (psc+1) / 72MHz`

例如 1ms 定时：arr=999, psc=71 → T = 1000 × 72 / 72MHz = 1ms

### PWM 输出（TIM3 CH1/CH2 → PA6/PA7）

```c
void TIM3_PWM_Init(uint16_t arr, uint16_t psc)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    TIM_OCInitStructure.TIM_Pulse = 0;  // 初始占空比 0
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OC2Init(TIM3, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);
    
    TIM_Cmd(TIM3, ENABLE);
}
```

**PWM 频率计算**：`f = 72MHz / ((arr+1) * (psc+1))`

例如 10kHz PWM：arr=719, psc=0 → f = 72MHz / 720 = 100kHz（调整 arr 和 psc）

设置占空比：`TIM_SetCompare1(TIM3, dutyValue);`（dutyValue 范围 0~arr）

### 输入捕获（TIM2 CH2 → PA1）

```c
void TIM2_Capture_Init(uint16_t arr, uint16_t psc)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;  // TIM2_CH2
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    
    TIM_ICInitTypeDef TIM_ICInitStructure;
    TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
    TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter = 0x00;
    TIM_ICInit(TIM2, &TIM_ICInitStructure);
    
    TIM_ITConfig(TIM2, TIM_IT_CC2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}
```

---

## 6. NVIC 配置

### NVIC 分组（在 main.c 初始化时调用一次）

```c
NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  // 2位抢占 + 2位响应
```

### 中断配置通用模板

```c
NVIC_InitTypeDef NVIC_InitStructure;
NVIC_InitStructure.NVIC_IRQChannel = XXX_IRQn;               // 中断通道
NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;    // 抢占优先级
NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;           // 响应优先级
NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
NVIC_Init(&NVIC_InitStructure);
```

### 常用中断通道

| 外设 | 中断通道 |
|------|----------|
| USART1 | USART1_IRQn |
| USART2 | USART2_IRQn |
| TIM1 | TIM1_UP_IRQn |
| TIM2 | TIM2_IRQn |
| TIM3 | TIM3_IRQn |
| I2C1 | I2C1_EV_IRQn / I2C1_ER_IRQn |
| SPI1 | SPI1_IRQn |
| EXTI0~4 | EXTI0_IRQn ~ EXTI4_IRQn |
| EXTI9_5 | EXTI9_5_IRQn |
| EXTI15_10 | EXTI15_10_IRQn |

---

## 7. 外部中断（EXTI）配置

### 使能 AFIO 时钟（必须）

```c
RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
```

### EXTI 配置模板（PA0 为例）

```c
GPIO_InitTypeDef GPIO_InitStructure;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
GPIO_Init(GPIOA, &GPIO_InitStructure);

GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);  // 映射 PA0 到 EXTI0

EXTI_InitTypeDef EXTI_InitStructure;
EXTI_InitStructure.EXTI_Line = EXTI_Line0;
EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;  // 下降沿触发
EXTI_InitStructure.EXTI_LineCmd = ENABLE;
EXTI_Init(&EXTI_InitStructure);

NVIC_InitTypeDef NVIC_InitStructure;
NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
NVIC_Init(&NVIC_InitStructure);
```

---

## 8. 延时函数参考

以下函数假设用户已实现，驱动代码中直接调用：

```c
void DelayMs(uint32_t ms);   // 毫秒延时
void DelayUs(uint32_t us);   // 微秒延时
```

若用户未实现，可基于 SysTick 提供参考实现：

```c
// 基于 SysTick 的延时函数（在 main.c 中实现）
static uint8_t fac_us = 0;
static uint16_t fac_ms = 0;

void Delay_Init(void)
{
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
    fac_us = SystemCoreClock / 8000000;   // 72MHz/8 = 9
    fac_ms = (uint16_t)fac_us * 1000;     // 9000
}

void DelayUs(uint32_t nus)
{
    uint32_t temp;
    SysTick->LOAD = nus * fac_us;
    SysTick->VAL = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    do {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0x00;
}

void DelayMs(uint32_t nms)
{
    uint32_t temp;
    SysTick->LOAD = (uint32_t)nms * fac_ms;
    SysTick->VAL = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    do {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0x00;
}
```
