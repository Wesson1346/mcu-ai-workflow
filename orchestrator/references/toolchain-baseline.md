# 工具链基线（全局约束）

本仓库的所有步骤都按下面这套工具链假设工作。下游 skill 里若出现旧写法（Keil / 标准库），一律按本文件的换算表处理。

## 一、基线定义

| 项 | 采用 | 备注 |
| --- | --- | --- |
| IDE | AI IDE（CodeBuddy / Qoder / VS Code 系） | 能读写文件、能跑终端 |
| 扩展 | **STM32Cube 扩展**（内含 CubeMX） | 生成工程 |
| 构建 | **CMake + Ninja** | 由扩展生成 |
| 库 | **HAL**（LL 按 HAL 处理） | 标准库仅限用户明确的遗留工程 |
| 芯片 | STM32F103C8T6（默认） | 其他型号按用户指定 |
| 驱动目录 | `Module/` | 必须挂进 CMake |
| 编译 | 用户自有工具 | **流程不写编译指令** |
| 烧录 | 用户自有工具 | **流程不写烧录指令** |

## 二、标准工程结构

```
[工程根目录]/
├── Core/Inc/          main.h、外设头文件
├── Core/Src/          main.c、stm32f1xx_it.c、MX_xxx_Init
├── Drivers/           CMSIS + STM32F1xx_HAL_Driver
├── cmake/             toolchain 等，CubeMX 维护，不要手改
├── Module/            ★ 自建，放 bsp_xxx.c/.h
├── CMakeLists.txt     ★ 需加 Module 三行
└── [工程名].ioc       CubeMX 配置
```

## 三、CMake 必加三行

```cmake
file(GLOB MODULE_SOURCES "Module/*.c")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MODULE_SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE Module)
```

**CubeMX 重新生成工程后必须复查**这三行是否被覆盖（`CMakeLists.txt` 不在 USER CODE 保护范围）。
详见 `{{AI_SKILL_ROOT}}/mcu-module-programming/references/cmake-integration.md`。

## 四、三处 API 换算（最容易踩）

| 用途 | ❌ 标准库 / Keil 写法 | ✅ HAL / 本基线写法 |
| --- | --- | --- |
| 微秒延时 | `Delay_us(10)` | `DelayUs(10)`（HAL 无内置微秒延时，需 TIM 或 `__NOP` 校准自实现） |
| 毫秒延时 | `Delay_ms(100)` / `delay_ms(100)` | `DelayMs(100)`；**主循环里不要用它做长等待** |
| 串口打印 | `printf()` / `UsartPrintf` 直接用 | `UsartPrintf(USART_DEBUG, ...)`，底层基于 `HAL_UART_Transmit` 自封装（放 `Module/bsp_usart.c`） |
| 禁用 JTAG | `GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE)` | `__HAL_AFIO_REMAP_SWJ_NOJTAG()` |
| GPIO 初始化 | `GPIO_Init(GPIOA, &GPIO_InitStructure)` | `HAL_GPIO_Init(GPIOA, &GPIO_InitStruct)` |
| 时钟使能 | `RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)` | `__HAL_RCC_GPIOA_CLK_ENABLE()` |
| PWM 启停 | `TIM_Cmd(TIM4, ENABLE)` | `HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3)` |
| 设置占空比 | `TIM_SetCompare3(TIM4, val)` | `__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, val)` |

## 五、工程约定 8 条（来自 `stm32-driver`，全局强制）

1. 延时：`DelayUs(unsigned short)` / `DelayMs(unsigned short)`
2. 串口：`UsartPrintf(USART_DEBUG, ...)`
3. 驱动只放 `Module/`
4. `main.c` 变量全部定义在 main **外**，全局变量 `g_` 前缀
5. 判空用 `0`，不用 `NULL`
6. 数字显示先 `sprintf()` 到缓冲区
7. OLED 固定文字用**英文**（中文字库仅 12 字：自/手/动/模/式/网/络/连/接/中/叶/℃）
8. `OLED_ShowCH(u8 x, u8 y, u8 *chs)` 第三参是 `u8*`，缓冲区声明为 `unsigned char`

## 六、AI 的能力边界（必须如实告知用户）

| 能做 | 不能做 |
| --- | --- |
| 生成驱动代码、主控逻辑 | 编译（本环境无工具链） |
| 静态自检（include / 命名 / 库调用一致性） | 烧录、跑真机 |
| 检查 CMake 是否登记了 `Module/` | 看到实物接线 |
| 根据用户回报的现象定位问题 | 读示波器/逻辑分析仪 |
| 设计测试用例与验收矩阵 | 判断"看起来对不对"（必须靠现象） |

**所以每一步的验证都要靠用户回报现象。** 方案卡里要明确写"这一步需要你做什么验证"。

## 七、与课件的差异声明

课件（IMA 知识库 17 篇）是在 **Keil MDK + 标准库** 语境下讲的。
本基线切换到 AI IDE + Cube 扩展后，读课件时需自动换算：

| 课件说法 | 本基线 |
| --- | --- |
| 装 Keil MDK-ARM、Pack Installer | Cube 扩展自动处理 |
| Add Files 加到工程分组 | CMake GLOB + Reconfigure |
| 标准库 API | HAL API（见第四节） |

换算对照表也在 `{{AI_SKILL_ROOT}}/course-notes/00-index.md`。
