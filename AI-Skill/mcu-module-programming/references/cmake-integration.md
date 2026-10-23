# `Module/` 目录接入 CMake 工程

适用工程：STM32Cube 扩展（CubeMX）生成的 **CMake + HAL** 工程（AI IDE 打开，如 CodeBuddy / Qoder / VS Code）。

## 一、标准工程结构

```
[工程根目录]/
├── Core/
│   ├── Inc/
│   └── Src/            main.c、stm32f1xx_it.c、外设 MX_xxx_Init
├── Drivers/
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/
├── cmake/              toolchain、编译选项等，由 CubeMX 维护
├── Module/             ★ 我们自己放驱动的地方（需手动创建）
├── CMakeLists.txt      ★ 主要改这里
└── [工程名].ioc        CubeMX 配置文件
```

## 二、必加的三行

在 `CMakeLists.txt` 中 `target_sources` / `target_include_directories` 附近加入：

```cmake
# ---- 用户模块驱动（Module/） ----
file(GLOB MODULE_SOURCES "Module/*.c")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MODULE_SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE Module)
```

含义：

| 行 | 作用 |
| --- | --- |
| `file(GLOB MODULE_SOURCES "Module/*.c")` | 收集 `Module/` 下所有 `.c`，新加驱动不用再改这个文件 |
| `target_sources(...)` | 把收集到的源文件加入编译目标 |
| `target_include_directories(...)` | 让 `#include "bsp_xxx.h"` 能被找到 |

## 三、两种可选写法

### 写法 A：GLOB（推荐，省事）

上面三行。新增 `bsp_dht11.c` 后**不需要**再动 `CMakeLists.txt`，
但需要在 IDE 里触发一次 **CMake 重新配置（Reconfigure）**，否则新文件不会被扫到。

### 写法 B：显式列表（更可控）

```cmake
set(MODULE_SOURCES
    Module/bsp_delay.c
    Module/bsp_usart.c
    Module/bsp_dht11.c
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MODULE_SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE Module)
```

优点：文件名一目了然，出问题容易定位；缺点：**每加一个模块都要改这里**，
忘记改就会报 `undefined reference`。

> 本仓库默认用写法 A；如果用户工程原本是显式列表风格，就跟着用写法 B，不要混。

## 四、⚠️ CubeMX 重新生成后的复查点

CubeMX 生成的 `CMakeLists.txt` **不在 USER CODE 保护范围内**，
重新生成工程时我们加的三行可能被覆盖掉。

**每次从 `.ioc` 重新生成工程之后，必须复查：**

```
□ CMakeLists.txt 里 file(GLOB MODULE_SOURCES "Module/*.c") 还在吗
□ target_sources 里的 ${MODULE_SOURCES} 还在吗
□ target_include_directories 里的 Module 还在吗
```

不在就补回去，然后重新配置一次 CMake。

## 五、常见编译问题对照

| 报错 | 原因 | 修法 |
| --- | --- | --- |
| `fatal error: bsp_xxx.h: No such file or directory` | include 路径没加 | 补 `target_include_directories(... Module)` |
| `undefined reference to 'XXX_Init'` | 源文件没进编译（GLOB 没重配置 / 显式列表漏加） | Reconfigure 或手动加 `.c` |
| `multiple definition of 'g_xxx'` | 变量在头文件里定义（不是在 .c 里） | 头文件只放 `extern` 声明 |
| `implicit declaration of function 'DelayUs'` | 没 include `bsp_delay.h` | 补 include |
| `HAL_xxx` 未定义 | 没用 HAL 而用了标准库 API，或反之 | 核对库类型，统一写法 |
| `#error "Please select first the target STM32F1xx device"` | 器件宏没定义 | 检查 CubeMX 生成是否完整 |

## 六、与编译烧录的边界

本仓库**不负责**编译与烧录的具体命令——用户有自己的工具链。
本 skill 只保证：

1. 驱动文件落在 `Module/`
2. `Module/` 已被构建系统纳入
3. 代码符合所选库的 API 写法

编译时的报错由用户反馈现象，AI 据此定位修改。
