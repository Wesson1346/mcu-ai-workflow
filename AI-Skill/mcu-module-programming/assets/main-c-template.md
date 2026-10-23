# 主控逻辑 `main.c` 骨架（HAL 库 / 节拍式主循环）

> 用法：**分两阶段**使用本骨架。
>
> - **阶段一（本地）**：只保留本地模块（传感器 / 显示 / 执行器 / 按键 / 报警），
>   `App_Task5000ms` 里的云平台代码先整块注释掉，直到通过"拔掉 WiFi 本地仍能完整运行"。
> - **阶段二（联网）**：通信模块单独验证通过后，取消注释，接入上报与下行，然后重跑阶段一验收。
>
> `[方括号]` 为占位，按模块实施顺序表替换。
>
> **标注约定**：★ = 课件要求（第 13 章 / 9–12 模块章 / 拆解与搭建文档）；
> ☆ = 工程实践补充（课件未涉及，如节拍分频、阈值回差、`g_` 前缀）。

```c
/* ============================================================
 * 文件名  ：main.c
 * 描述    ：[系统名称] 主控逻辑
 * 备注    ：基于 STM32F103 + HAL 库生成
 * 说明    ：变量统一定义在 main 之外，全局变量用 g_ 前缀；
 *           主循环采用 1ms 节拍 + 计数分频，禁止长阻塞延时
 * ============================================================ */
#include "main.h"
#include "bsp_delay.h"
#include "bsp_usart.h"
#include "bsp_oled.h"
/* [按模块实施顺序表依次包含各驱动头文件] */
/* #include "bsp_dht11.h" */
/* #include "bsp_fan.h"    */
/* #include "bsp_key.h"    */
/* #include "bsp_buzzer.h" */

/* ============ 全局状态（全部定义在 main 之外） ============ */
float   g_Temperature   = 0.0f;    /* 当前温度         */
float   g_Humidity      = 0.0f;    /* 当前湿度         */
uint8_t g_FanSpeed      = 0;       /* 风扇 PWM 档位 0-100 */
uint8_t g_WorkMode      = 0;       /* 0=自动 1=手动    */
uint8_t g_AlarmFlag     = 0;       /* 超温报警标志     */
float   g_ThresholdHigh = 30.0f;   /* 上限阈值         */
float   g_ThresholdLow  = 28.0f;   /* 下限阈值（回差） */

/* ============ 节拍计数（由 1ms 中断累加） ============ */
volatile uint8_t  g_Tick1msFlag = 0;   /* 1ms 到标志 */
volatile uint32_t g_Cnt1ms      = 0;   /* 1ms 累计   */

static void App_Init(void);
static void App_Task1ms(void);
static void App_Task20ms(void);
static void App_Task100ms(void);
static void App_Task1000ms(void);
static void App_Task5000ms(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();          /* 调试串口，USART1 保留给 printf 类输出 */
    /* [其他 CubeMX 生成的 MX_xxx_Init()] */

    DelayMs(100);                   /* 上电等待模块稳定 */

    App_Init();

    while (1)
    {
        if (g_Tick1msFlag)
        {
            g_Tick1msFlag = 0;
            g_Cnt1ms++;

            App_Task1ms();                                   /* 每 1ms  */
            if ((g_Cnt1ms % 20)   == 0) { App_Task20ms();   }  /* 每 20ms 按键扫描 */
            if ((g_Cnt1ms % 100)  == 0) { App_Task100ms();  }  /* 每 100ms 刷新显示 */
            if ((g_Cnt1ms % 1000) == 0) { App_Task1000ms(); }  /* 每 1s 采集与控制 */
            if ((g_Cnt1ms % 5000) == 0) { App_Task5000ms(); }  /* 每 5s 云平台上报 */
        }
    }
}

/* ============ 初始化：严格按模块编号顺序 ============ */
static void App_Init(void)
{
    /* 模块1 */
    /* DHT11_Init();  */
    /* 模块2 */
    /* OLED_Init();   */
    /* 模块3 */
    /* Fan_Init();    */
    /* ... */
    UsartPrintf(USART_DEBUG, "App init done\r\n");
}

/* ============ 1ms 任务（一般留空或做软计时） ============ */
static void App_Task1ms(void)
{
}

/* ============ 20ms：按键扫描（含消抖） ============ */
static void App_Task20ms(void)
{
    /* Key_Scan();  内部做消抖，返回键值后在此分发 */
}

/* ============ 100ms：刷新显示 ============ */
static void App_Task100ms(void)
{
    char buf[32];

    /* OLED 固定文字用英文（中文字库仅 12 字） */
    OLED_ShowString(0, 0,  (unsigned char *)"TEMP:");
    OLED_ShowString(0, 2,  (unsigned char *)"MODE:");
    OLED_ShowString(0, 4,  (unsigned char *)"FAN :");

    /* 数字先 sprintf 再显示 */
    sprintf(buf, "%5.1f C  ", g_Temperature);
    OLED_ShowString(40, 0, (unsigned char *)buf);

    sprintf(buf, "%s      ", (g_WorkMode == 0) ? "AUTO" : "MANU");
    OLED_ShowString(40, 2, (unsigned char *)buf);

    sprintf(buf, "%3d %%   ", g_FanSpeed);
    OLED_ShowString(40, 4, (unsigned char *)buf);
}

/* ============ 1s：采集 + 阈值控制 ============ */
static void App_Task1000ms(void)
{
    float temp = 0.0f;

    /* 采集：读失败时保留上一次的值，不要直接把 0 写进状态 */
    /* if (DHT11_ReadData(&temp, &g_Humidity) == 0) { g_Temperature = temp; } */

    if (g_WorkMode == 0)   /* 自动模式 */
    {
        /* 带回差的阈值控制：超过上限才开，低于下限才关 */
        if (g_Temperature >= g_ThresholdHigh)
        {
            g_AlarmFlag = 1;
            g_FanSpeed  = 100;
        }
        else if (g_Temperature <= g_ThresholdLow)
        {
            g_AlarmFlag = 0;
            g_FanSpeed  = 0;
        }
        /* 中间区间保持上一次的状态 —— 这就是回差，避免临界抖动 */
    }

    /* 执行：风扇 */
    /* Fan_SetSpeed(g_FanSpeed); */

    /* 报警：有源蜂鸣器，GPIO 电平驱动 */
    /* Buzzer_Set(g_AlarmFlag); */

    UsartPrintf(USART_DEBUG, "T=%.1fC Fan=%d%% Mode=%d Alarm=%d\r\n",
                g_Temperature, g_FanSpeed, g_WorkMode, g_AlarmFlag);
}

/* ============ 5s：云平台上报 + 下行处理（★ 阶段二才启用） ============ */
/* 阶段一请整块注释掉，先让本地闭环跑通 */
static void App_Task5000ms(void)
{
    /* 上行：一次打包多属性，属性值必须 {"value":...} 包装，QoS1 */
    /* Onenet_PublishProps(g_Temperature, g_FanSpeed, g_WorkMode); */

    /* 下行：非阻塞取帧，不要在里做长耗时解析 */
    /* uint8_t *p;
       while ((p = ESP8266_GetIPD(0)) != 0)
       {
           Onenet_HandleSetMsg(p);   内部回调改 g_ThresholdHigh / g_WorkMode
       } */
}

/* ============ SysTick 回调：产生 1ms 节拍 ============ */
void HAL_SYSTICK_Callback(void)
{
    g_Tick1msFlag = 1;
}
```

## 使用要点

| 约定 | 说明 | 来源 |
| --- | --- | --- |
| 禁止长阻塞 | `while(1)` 里不要出现 `HAL_Delay(1000)`，否则按键与下行全部失联 | ☆ |
| 回差必须设 | 只有上限没有下限，临界值处会反复开关 | ☆ |
| 采集失败不覆盖 | 读传感器失败时保留上次值，避免 0℃ 误触发报警 | ☆ |
| 上报分频 | 云平台 5–10s 一次足够，每轮都发会被限流 | ☆ |
| 属性必须包装 | 上报值要 `{"value": ...}`，bool 用 `true/false`，否则整包被拒 | ★ |
| 下行非阻塞 | 用 `ESP8266_GetIPD(0)` 取帧，不在中断里长耗时解析 | ☆ |
| 变量位置 | 全部在 `main()` 之外定义，其他文件用 `extern` 引用 | ☆ |
| `MODULE_TEST` | 组装完成后必须置 0，否则主控逻辑不生效 | — |

> ☆ 项是工程实践补充，课件未涉及；与课件冲突时以课件为准。
