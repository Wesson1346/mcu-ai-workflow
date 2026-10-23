#ifndef _ESP8266_H_
#define _ESP8266_H_

/* ==================================================================
 * ESP8266 AT 指令驱动（USART2）
 *
 * ⚠️ 本文件的接收层是"修复版"：旧版 OneNET 参考代码在发送路径上调用
 *    ESP8266_Clear() 会连同正在到达的下行帧一起清掉，导致 App 下发的
 *    指令偶发收不到且串口一个字都不打印。详见 references/onenet-guide.md
 *    第七节「ESP8266 接收缓冲区管理」。
 *    修改本文件前请先读那一节，不要把 ESP8266_Clear() 加回发送路径。
 *
 * ⚠️ 本文件的发送层也是"修复版"：查找 AT 应答关键字必须用 ESP8266_FindInBuf()
 *    （按字节比较），**绝不能用 strstr** —— 接收缓冲区里可能停着还没被应用层
 *    取走的 +IPD 帧，帧内容是二进制 MQTT 报文（含 0x00），strstr 遇到 0x00
 *    就停下，帧后面的 OK / > / SEND OK 全部"隐身"，导致数据静默发不出去
 *    （平台收不到、也没有 PUBACK）。详见 references/onenet-guide.md 第八节。
 *    另外每发完一包要等 "SEND OK"（ESP8266_WaitSendDone），
 *    否则紧接着的下一包会撞上 ESP8266"忙"（回 "busy p..."）而拿不到 '>'。
 * ================================================================== */

#define REV_OK		0	//接收完成标志
#define REV_WAIT	1	//接收未完成标志


void ESP8266_Init(void);

/* 清空接收缓冲区（整体清零）。
 * ⚠️ 只允许在"确认缓冲区里没有下行数据"时调用；发送 AT 指令前后请用
 *    内部函数 ESP8266_DropAtEcho()，它会保留 +IPD 帧 */
void ESP8266_Clear(void);

_Bool ESP8266_SendCmd(char *cmd, char *res);

void ESP8266_SendData(unsigned char *data, unsigned short len);

/* 取出一帧下行数据（"+IPD,<len>:<payload>"），返回 payload 指针。
 * 返回值指向驱动内部的静态缓存 g_IpdFrame，会被下一次调用覆盖。
 * timeOut：0-非阻塞，只尝试一次，取不到立即返回 NULL；
 *          其他值-最多等待 timeOut×5ms。
 * 按 <len> 长度精确取帧，凑齐才返回；返回后只把这一帧从接收缓冲区消费掉 */
unsigned char *ESP8266_GetIPD(unsigned short timeOut);


#endif
