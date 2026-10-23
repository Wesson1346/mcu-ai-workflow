# 传感器/外设模块目录

本文档包含常见 STM32F103 传感器/外设模块的关键参数，供驱动生成时参考。

---

## 1. OLED 显示屏（0.96寸 SSD1306）

- **输出文件名**：`bsp_oled.c` / `bsp_oled.h`
- **通信协议**：软件模拟 I2C（默认）或 软件模拟 SPI
- **I2C 地址**：0x78（8位地址）或 0x3C（7位地址）
- **分辨率**：128 × 64
- **默认引脚连接（软件模拟 I2C）**：
  - SCL → PB6（任意 GPIO）
  - SDA → PB7（任意 GPIO）
- **关键时序参数**：
  - I2C 速率：最高 400kHz（Fast Mode）
  - 上电复位后等待 200ms
- **必备函数**：
  - `OLED_Init()` — 初始化显示屏
  - `OLED_Clear()` — 清屏
  - `OLED_ShowChar(uint8_t x, uint8_t y, char ch)` — 显示字符
  - `OLED_ShowString(uint8_t x, uint8_t y, char *str)` — 显示字符串
  - `OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len)` — 显示数字
  - `OLED_ShowCH(uint8_t x, uint8_t y, uint8_t *chs)` — 中英文混合显示（推荐）
  - `OLED_Refresh()` — 刷新显存到屏幕

### ⚠️ OLED 驱动经验（实测验证，务必遵守）

> 以下为实际可用的 OLED（SSD1306，软件 I2C）驱动要点，生成时必须包含：

1. **软件 I2C 时序（4 个基础函数）**
   必须实现 `IIC_Start` / `IIC_Stop` / `IIC_WaitAck` / `Write_IIC_Byte` 四个基础函数，配合微秒级空循环延时（如 `for(i=0;i<10;i++);`）。

2. **命令/数据写入区分**
   - 写命令：地址 → `0x00`（控制字节，命令）→ 命令
   - 写数据：地址 → `0x40`（控制字节，数据）→ 数据

3. **初始化寄存器序列（固定）**
   `0xAE` 关显示 → 对比度 `0x81/0xFF` → 段重映射 `0xA1` → 正常显示 `0xA6` → 多路复用 `0xA8/0x3F` → COM 扫描 `0xC8` → 电荷泵 `0x8D/0x14` → `0xAF` 开显示。序列错误会导致花屏/不亮。

4. **坐标定位（OLED_Set_Pos）**
   `0xB0+y` 设页，`(x+2)` 设列。注意 OLED 有 2 列偏移量。

5. **引脚配置**
   推挽输出 50MHz，SCL/SDA 两个引脚初始化后发送一个 Stop 信号复位总线。

6. **字库文件**
   中文字库（16x16）和 ASCII 字库（8x16）放在独立的 `bsp_oledfont.h` 中。汉字显示依赖 `hz_index[]` 索引表，字库顺序必须与取模顺序一致。

7. **显示数字前先 sprintf 转换**
   显示数字时先用 `sprintf` 转成字符串，再调用 `OLED_ShowCH` 显示，且注意像素点不重叠。

---

## 2. DHT11 温湿度传感器

- **输出文件名**：`bsp_dht11.c` / `bsp_dht11.h`
- **通信协议**：单总线（One-Wire），GPIO 模拟
- **测量范围**：温度 0~50°C（精度 ±2°C），湿度 20~90%RH（精度 ±5%RH）
- **默认引脚连接**：
  - DATA → PA0（或任意 GPIO），需上拉电阻 4.7K~10K
- **关键时序参数**：
  - 主机起始信号：拉低 ≥18ms，拉高 20~40μs
  - 响应信号：DHT11 拉低 80μs，拉高 80μs
  - 数据位：50μs 低电平起始 → 26~28μs 高电平 = "0"，70μs 高电平 = "1"
  - 读取周期：≥1 秒
- **数据格式**：40 位 = 湿度整数(8bit) + 湿度小数(8bit) + 温度整数(8bit) + 温度小数(8bit) + 校验和(8bit)
- **必备函数**：
  - `DHT11_Init()` — 初始化 GPIO
  - `DHT11_ReadData(uint8_t *temp, uint8_t *humi)` — 读取温湿度数据
  - `DHT11_Check()` — 检测传感器是否存在

### ⚠️ DHT11 驱动常见坑（务必遵守）

> 以下是实际调试中验证过的经验，生成 DHT11 驱动时必须严格实现，否则会出现校验和错误（读取到湿度偏大、温度偏小的异常数据）：

1. **响应信号必须等待 3 段（最易错）**
   主机发送起始信号（拉低≥18ms → 拉高20~40μs → 切换输入）后，必须依次等待：
   - ① 等待 DHT11 拉低（响应开始，约 80μs）
   - ② 等待 DHT11 拉低结束（响应低电平约 80μs）
   - ③ **等待 DHT11 拉高结束（响应高电平约 80μs）**
   
   **漏掉第 3 段"等待响应高电平结束"会导致位读取从错误的起始点开始**，40 位数据时序整体错位，校验和必然失败。这是调试中发现的根本原因。

2. **起始信号要"拉高 30μs 再切输入"**
   主机拉低 20ms 后应：先主机拉高 20~40μs（保持推挽输出），**再切换为输入模式**。不要直接切输入靠上拉电阻拉高（高电平建立慢，可能影响 DHT11 检测起始信号结束）。

3. **数据位读取：40μs 延时采样法**
   每 bit 等待低电平起始结束（约 50μs）→ 延时 40μs 采样：bit"0"高电平26-28μs已变低，bit"1"高电平70μs仍为高。判断最清晰、最稳定。

4. **等待循环必须带超时保护**
   所有 `while (读电平 == 某值)` 等待循环都要加超时计数（如 `timeout < 100`），防止总线异常时死循环。

5. **读取完 5 字节后恢复 GPIO 为输出并拉高**
   `DHT11_ReadData` 读完后应 `GPIO 输出模式 + 拉高`，为下次读取做准备。

---

## 3. 蜂鸣器（有源/无源）

- **输出文件名**：`bsp_buzzer.c` / `bsp_buzzer.h`
- **通信协议**：GPIO 控制（有源）或 PWM 驱动（无源）
- **默认引脚连接**：
  - 有源蜂鸣器：控制引脚 → PB5（低电平触发常见）
  - 无源蜂鸣器：PWM 引脚 → PA6（TIM3_CH1）或 PA0（TIM2_CH1）
- **关键时序参数**：
  - 有源：仅需 GPIO 高低电平控制
  - 无源：PWM 频率决定音调，常用 1kHz~5kHz，占空比 50%
- **必备函数**：
  - `Buzzer_Init()` — 初始化 GPIO/TIM
  - `Buzzer_On()` — 开启蜂鸣
  - `Buzzer_Off()` — 关闭蜂鸣
  - `Buzzer_SetFreq(uint32_t freq)` — 设置频率（无源蜂鸣器）

---

## 4. HC-SR04 超声波测距模块

- **输出文件名**：`bsp_hcsr04.c` / `bsp_hcsr04.h`
- **通信协议**：GPIO 脉冲触发 + 定时器捕获回波
- **测距范围**：2cm ~ 400cm
- **默认引脚连接**：
  - Trig → PA1（输出，触发信号）
  - Echo → PA2（输入，回波信号，建议 TIM 捕获）
- **关键时序参数**：
  - 触发信号：拉高 ≥10μs
  - 回波信号：高电平持续时间 = 声波往返时间
  - 距离计算：`距离(cm) = 回波高电平时间(μs) × 0.017`（声速 340m/s）
  - 测量周期：≥60ms
- **必备函数**：
  - `HCSR04_Init()` — 初始化 GPIO 和定时器
  - `HCSR04_GetDistance(void)` — 获取距离值（cm）
  - `HCSR04_StartMeasure()` — 启动一次测量

---

## 5. L298N 电机驱动模块

- **输出文件名**：`bsp_motor.c` / `bsp_motor.h`
- **通信协议**：GPIO 方向控制 + PWM 速度控制
- **驱动能力**：2 路 DC 电机，每路最大 2A
- **默认引脚连接（双电机）**：
  - IN1 → PB0，IN2 → PB1（电机 A 方向）
  - IN3 → PB2，IN4 → PB10（电机 B 方向）
  - ENA → PA6（TIM3_CH1，PWM 控制 A 速度）
  - ENB → PA7（TIM3_CH2，PWM 控制 B 速度）
- **关键时序参数**：
  - PWM 频率：10kHz~20kHz（避免可闻噪声）
  - 占空比：0%~100% 控制速度
- **必备函数**：
  - `Motor_Init()` — 初始化 GPIO 和 PWM 定时器
  - `Motor_SetSpeed(uint8_t motor, int8_t speed)` — 设置电机速度（-100~100）
  - `Motor_Stop(uint8_t motor)` — 停止指定电机

---

## 6. MPU6050 六轴姿态传感器

- **输出文件名**：`bsp_mpu6050.c` / `bsp_mpu6050.h`
- **通信协议**：软件模拟 I2C
- **I2C 地址**：0x68（AD0=0）或 0x69（AD0=1）
- **量程**：加速度 ±2/4/8/16g，陀螺仪 ±250/500/1000/2000°/s
- **默认引脚连接（软件模拟 I2C）**：
  - SCL → PB6（任意 GPIO）
  - SDA → PB7（任意 GPIO）
  - AD0 → GND（地址 0x68）
  - INT → PA3（可选，中断输出）
- **关键时序参数**：
  - I2C 速率：最高 400kHz
  - 上电后等待 100ms
  - 配置寄存器需在 PWR_MGMT_1 唤醒后设置
- **关键寄存器**：
  - PWR_MGMT_1 (0x6B)：电源管理，写 0x00 唤醒
  - SMPLRT_DIV (0x19)：采样率分频
  - CONFIG (0x1A)：低通滤波
  - GYRO_CONFIG (0x1B)：陀螺仪量程
  - ACCEL_CONFIG (0x1C)：加速度计量程
  - ACCEL_XOUT_H (0x3B)~ACCEL_ZOUT_L (0x40)：加速度原始数据
  - GYRO_XOUT_H (0x43)~GYRO_ZOUT_L (0x48)：陀螺仪原始数据
- **必备函数**：
  - `MPU6050_Init()` — 初始化 I2C 和传感器配置
  - `MPU6050_GetAccel(int16_t *ax, int16_t *ay, int16_t *az)` — 读取加速度
  - `MPU6050_GetGyro(int16_t *gx, int16_t *gy, int16_t *gz)` — 读取陀螺仪
  - `MPU6050_WriteReg(uint8_t reg, uint8_t data)` — 写寄存器
  - `MPU6050_ReadReg(uint8_t reg, uint8_t *data, uint8_t len)` — 读寄存器

---

## 7. DS18B20 温度传感器

- **输出文件名**：`bsp_ds18b20.c` / `bsp_ds18b20.h`
- **通信协议**：单总线（1-Wire），GPIO 模拟
- **精度**：±0.5°C（-10~+85°C 范围内）
- **测温范围**：-55°C ~ +125°C
- **默认引脚连接**：
  - DQ → PA0（需 4.7K 上拉电阻）
- **关键时序参数**：
  - 复位脉冲：拉低 480μs，释放等待 15~60μs，存在脉冲 60~240μs
  - 写 "0"：拉低 60~120μs
  - 写 "1"：拉低 1~15μs，释放
  - 读时隙：拉低 1μs 启动，15μs 内采样
  - 温度转换时间：750ms（12 位精度）
- **关键命令**：
  - 0x33：读取 ROM
  - 0x55：匹配 ROM
  - 0xCC：跳过 ROM
  - 0x44：启动温度转换
  - 0xBE：读暂存器
- **必备函数**：
  - `DS18B20_Init()` — 初始化 GPIO
  - `DS18B20_GetTemp(float *temp)` — 获取温度值
  - `DS18B20_StartConvert()` — 启动温度转换
  - `DS18B20_Reset()` — 复位总线

---

## 8. ESP8266 WiFi 模块（AT 指令模式）

- **输出文件名**：`bsp_esp8266.c` / `bsp_esp8266.h`
- **通信协议**：USART
- **默认波特率**：115200
- **默认引脚连接（USART2，USART1 保留给单片机调试打印）**：
  - TX → PA3（USART2_RX，STM32 接收）
  - RX → PA2（USART2_TX，STM32 发送）
  - CH_PD → 3.3V（使能）
  - RST → 可选 GPIO（硬复位）
- **串口分配约定**：USART1（PA9/PA10）保留给单片机 printf 调试打印，模块串口优先使用 USART2（PA2/PA3）或 USART3（PB10/PB11）
- **关键时序参数**：
  - AT 指令以 `\r\n` 结尾
  - 指令响应超时：500ms~3000ms
  - 上电后等待 500ms~1000ms 初始化
- **常用 AT 指令**：
  - `AT` — 测试
  - `AT+RST` — 重启
  - `AT+CWMODE=1` — Station 模式
  - `AT+CWJAP="SSID","PWD"` — 连接 WiFi
  - `AT+CIPSTART="TCP","ip",port` — 建立 TCP 连接
  - `AT+CIPSEND=n` — 发送 n 字节数据
- **必备函数**：
  - `ESP8266_Init()` — 初始化 USART
  - `ESP8266_SendCmd(char *cmd, char *ack, uint32_t timeout)` — 发送 AT 指令并等待响应
  - `ESP8266_ConnectWiFi(char *ssid, char *pwd)` — 连接 WiFi
  - `ESP8266_SendData(char *data, uint16_t len)` — 发送数据
- **OneNET 云平台属性上报**：参见 `references/onenet-guide.md`
  - ⚠️ **属性值必须 `{"value":...}` 包装**：`{"id":"1","version":"1.0","params":{"temp":{"value":20}}}`，写成 `{"temp":20}` 会被平台静默丢弃（ACK 成功但数据不更新）
  - 属性 identifier 必须与平台物模型一致
