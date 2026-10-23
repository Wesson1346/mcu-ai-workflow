/*
 * 文件名：bsp_module.h
 * 描述  ：MODULE 驱动头文件
 * 备注  ：基于 STM32F103 标准外设库，I2C/SPI 使用软件模拟
 */

#ifndef __BSP_MODULE_H
#define __BSP_MODULE_H

#include "stm32f10x.h"

/* ======================== 引脚宏定义 ======================== */
#define MODULE_SDA_GPIO_PORT      GPIOA
#define MODULE_SDA_GPIO_PIN       GPIO_Pin_0
#define MODULE_SDA_GPIO_CLK       RCC_APB2Periph_GPIOA

#define MODULE_SCL_GPIO_PORT      GPIOA
#define MODULE_SCL_GPIO_PIN       GPIO_Pin_1
#define MODULE_SCL_GPIO_CLK       RCC_APB2Periph_GPIOA

/* ======================== 参数宏定义 ======================== */
#define MODULE_I2C_ADDR           0x00

/* ======================== 类型定义 ======================== */
/*
 * 函数名：MODULE_TypeDef
 * 描述  ：MODULE 数据类型定义
 */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} MODULE_TypeDef;

/* ======================== 函数声明 ======================== */

/*
 * 函数名：MODULE_Init
 * 描述  ：初始化 MODULE
 * 输入  ：无
 * 输出  ：0-成功, -1-失败
 */
int8_t MODULE_Init(void);

/*
 * 函数名：MODULE_ReadData
 * 描述  ：读取 MODULE 数据
 * 输入  ：无
 * 输出  ：data-数据存储指针; 返回值: 0-成功, -1-失败
 */
int8_t MODULE_ReadData(MODULE_TypeDef *data);

#endif /* __BSP_MODULE_H */
