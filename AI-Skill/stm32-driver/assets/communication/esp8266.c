
//单片机头文件
#include "stm32f10x.h"

//网络设备驱动
#include "esp8266.h"

//硬件驱动
#include "bsp_delay.h"
#include "bsp_usart.h"

//C库
#include <string.h>
#include <stdio.h>


/* ================== 通信配置区（按你的环境修改） ================== */
/* WiFi 账号和密码 */
#define ESP8266_WIFI_SSID	"YOUR_WIFI_SSID"
#define ESP8266_WIFI_PWD	"YOUR_WIFI_PWD"

/* 接入的云平台 MQTT 服务器地址和端口 */
#define ESP8266_TCP_SERVER	"mqtts.heclouds.com"
#define ESP8266_TCP_PORT	"1883"
/* ================================================================ */

#define ESP8266_WIFI_INFO		"AT+CWJAP=\"" ESP8266_WIFI_SSID "\",\"" ESP8266_WIFI_PWD "\"\r\n"
#define ESP8266_ONENET_INFO		"AT+CIPSTART=\"TCP\",\"" ESP8266_TCP_SERVER "\"," ESP8266_TCP_PORT "\r\n"

#define ESP8266_RX_BUF_SIZE		(sizeof(esp8266_buf) - 1)	/* 接收缓冲区可用长度（末字节留给字符串结束符） */
#define ESP8266_IPD_FRAME_SIZE	256							/* 单帧下行数据的最大长度 */

unsigned char esp8266_buf[512];
unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;

/* 单帧下行数据缓存：取帧后把数据拷到这里，与接收缓冲区解耦，
 * 这样上层解析过程中即使接收缓冲区被整理也不会被破坏 */
static unsigned char g_IpdFrame[ESP8266_IPD_FRAME_SIZE + 1];


//==========================================================
//	函数名称：	ESP8266_Clear
//
//	函数功能：	清空缓存
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_Clear(void)
{

	/* 清缓冲期间关中断，避免与 USART2 接收中断并发改动计数 */
	NVIC_DisableIRQ(USART2_IRQn);

	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;
	esp8266_cntPre = 0;				/* 计数基准一并复位，防止"两次计数相同"被误判成接收完成 */

	NVIC_EnableIRQ(USART2_IRQn);

}

/*
 * 函数名：ESP8266_FindInBuf
 * 描述  ：在接收缓冲区 [0, esp8266_cnt) 范围内按字节查找子串（不受其中的 0x00 影响）
 * 输入  ：needle-要查找的关键字
 * 输出  ：找到返回在缓冲区中的指针；没找到返回 0
 * 备注  ：⚠️ 查找 AT 应答关键字时绝不能用 strstr。缓冲区里可能停着还没被应用层取走的
 *         +IPD 帧，帧内容是二进制 MQTT 报文（例如 SUBACK 的 90 04 00 14 00 00，含 0x00），
 *         而 strstr 遇到第一个 0x00 就停下，帧后面的 AT 应答（OK / > / SEND OK）就永远看不见。
 *         症状：订阅刚发完紧接着发属性上报 → 拿不到 '>' → 整包数据一个字节都发不出去；
 *         更糟的是 ESP8266 那边已经打印 '>' 并进入"等数据"状态，我们不发，
 *         它就会把后面每一包 AT 指令都当成数据吞掉，从此永久失败。
 */
static char *ESP8266_FindInBuf(const char *needle)
{
	unsigned short nlen = (unsigned short)strlen(needle);
	unsigned short i = 0;

	if(nlen == 0 || esp8266_cnt < nlen)
	{
		return 0;
	}

	for(i = 0; i <= (unsigned short)(esp8266_cnt - nlen); i++)
	{
		if(memcmp(esp8266_buf + i, needle, nlen) == 0)
		{
			return (char *)(esp8266_buf + i);
		}
	}

	return 0;
}

/* ======================== 接收缓冲区管理（私有函数区） ======================== */

/*
 * 函数名：ESP8266_DropRx
 * 描述  ：从接收缓冲区头部丢弃 n 字节（剩余数据前移）
 * 输入  ：n-要丢弃的字节数
 * 输出  ：无
 * 备注  ：用于"消费一帧"或"丢弃 AT 回显"；操作期间短暂关中断，
 *         避免与接收中断并发修改 esp8266_cnt 造成数据错位
 */
static void ESP8266_DropRx(unsigned short n)
{
	if(n == 0)
	{
		return;
	}

	NVIC_DisableIRQ(USART2_IRQn);					/* 关中断：移位过程中不允许接收中断改计数 */

	if(n >= esp8266_cnt)
	{
		memset(esp8266_buf, 0, sizeof(esp8266_buf));
		esp8266_cnt = 0;
	}
	else
	{
		memmove(esp8266_buf, esp8266_buf + n, (unsigned short)(esp8266_cnt - n));
		esp8266_cnt = (unsigned short)(esp8266_cnt - n);
		esp8266_buf[esp8266_cnt] = 0;
	}

	esp8266_cntPre = 0;

	NVIC_EnableIRQ(USART2_IRQn);
}

/*
 * 函数名：ESP8266_TailIsIpdHead
 * 描述  ：判断缓冲区末尾是不是 "+IPD," 的一部分（即帧头只收到了一半）
 * 输入  ：无
 * 输出  ：1-是（此时绝不能清缓冲，否则这一帧会被丢掉）；0-否
 */
static unsigned char ESP8266_TailIsIpdHead(void)
{
	const char *pat = "+IPD,";
	unsigned short n = 0;
	short i = 0;

	n = (esp8266_cnt < 5) ? esp8266_cnt : 5;

	for(i = (short)n; i > 0; i--)
	{
		if(memcmp(esp8266_buf + esp8266_cnt - i, pat, (unsigned short)i) == 0)
		{
			return 1;
		}
	}

	return 0;
}

/*
 * 函数名：ESP8266_DropAtEcho
 * 描述  ：丢弃缓冲区里第一个 "IPD," 之前的 AT 回显/应答，保留已经收到的下行数据帧
 * 输入  ：无
 * 输出  ：无
 * 备注  ：⚠️ 发送 AT 指令时绝不能用 ESP8266_Clear() 粗暴清空整个缓冲区，
 *         否则会把正在到达（或已到达）的下行帧一起抹掉 ——
 *         这正是"App 点了开关单片机没反应、串口也没有任何打印"的根因
 */
static void ESP8266_DropAtEcho(void)
{
	char *ipd = strstr((char *)esp8266_buf, "IPD,");

	if(ipd != 0)
	{
		ESP8266_DropRx((unsigned short)(ipd - (char *)esp8266_buf));
		return;
	}

	/* 没找到完整帧头，但末尾是 "+IPD," 的一半 —— 说明帧头还在接收中，必须留着 */
	if(ESP8266_TailIsIpdHead() != 0)
	{
		return;
	}

	ESP8266_Clear();								/* 缓冲区里确实没有下行数据，可以整体清空 */
}

/*
 * 函数名：ESP8266_TryGetFrame
 * 描述  ：尝试从接收缓冲区取出一帧完整的 "+IPD,<len>:<payload>"
 * 输入  ：无
 * 输出  ：1-取到一帧（内容在 g_IpdFrame 中）；0-还没有完整帧
 * 备注  ：按 <len> 长度精确取帧，凑齐才返回；
 *         取到的数据拷贝到独立的 g_IpdFrame，与接收缓冲区解耦；
 *         只把这一帧从缓冲区消费掉，后面的帧继续留着，避免连点时丢帧
 */
static unsigned char ESP8266_TryGetFrame(void)
{
	char *ipd = 0;
	char *colon = 0;
	char *p = 0;
	unsigned short cnt = 0;
	unsigned short len = 0;
	unsigned short headLen = 0;
	unsigned short frameLen = 0;
	unsigned short i = 0;

	/* 1. 先把 "IPD," 之前的 AT 回显丢掉，让数据帧对齐到缓冲区开头 */
	ESP8266_DropAtEcho();

	/* 2. 取一份稳定的计数快照，避免解析期间计数变化 */
	NVIC_DisableIRQ(USART2_IRQn);
	cnt = esp8266_cnt;
	NVIC_EnableIRQ(USART2_IRQn);

	if(cnt == 0)
	{
		return 0;
	}

	/* 3. 定位 "+IPD," 头和 ':' */
	ipd = strstr((char *)esp8266_buf, "IPD,");
	if(ipd == 0)
	{
		return 0;
	}

	colon = strchr(ipd, ':');
	if(colon == 0)
	{
		return 0;									/* 长度字段还没收全，等下一轮 */
	}

	/* 4. 解析 "<len>"，例如 +IPD,53:{"id":...} 中的 53 */
	for(p = ipd + 4; p < colon; p++)
	{
		if(*p >= '0' && *p <= '9')
		{
			len = (unsigned short)(len * 10 + (*p - '0'));
		}
	}

	headLen = (unsigned short)(colon + 1 - (char *)esp8266_buf);	/* "+IPD,<len>:" 的长度 */
	frameLen = (unsigned short)(headLen + len);

	if(cnt < frameLen)
	{
		return 0;									/* 整帧还没收完，等下一轮 */
	}

	if(len == 0)
	{
		ESP8266_DropRx(frameLen);					/* 空帧直接丢掉，避免卡住缓冲区 */
		return 0;
	}

	/* 5. 拷贝到独立缓存（超长帧截断，保证不越界） */
	if(len > ESP8266_IPD_FRAME_SIZE)
	{
		len = ESP8266_IPD_FRAME_SIZE;
	}

	for(i = 0; i < len; i++)
	{
		g_IpdFrame[i] = esp8266_buf[headLen + i];
	}
	g_IpdFrame[len] = 0;

	/* 6. 只消费掉这一帧，缓冲区里后续的帧保留给下一次取值 */
	ESP8266_DropRx(frameLen);

	return 1;
}

//==========================================================
//	函数名称：	ESP8266_WaitRecive
//
//	函数功能：	等待接收完成
//
//	入口参数：	无
//
//	返回参数：	REV_OK-接收完成		REV_WAIT-接收超时未完成
//
//	说明：		循环调用检测是否接收完成
//==========================================================
_Bool ESP8266_WaitRecive(void)
{

	if(esp8266_cnt == 0) 							//如果接收计数为0 则说明没有处于接收数据中，所以直接跳出，结束函数
		return REV_WAIT;
		
	if(esp8266_cnt == esp8266_cntPre)				//如果上一次的值和这次相同，则说明接收完毕
	{
		/* ⚠️ 这里原来执行的是 esp8266_cnt = 0：
		 * 计数被清零后，随后到达的字节会从缓冲区下标 0 开始覆盖，
		 * 把正在接收（或已收到但还没处理）的下行帧冲掉。
		 * 现在只返回"计数已稳定"，绝不再改动接收缓冲区 */
			
		return REV_OK;								//返回接收完成标志
	}
		
	esp8266_cntPre = esp8266_cnt;					//置为相同
	
	return REV_WAIT;								//返回接收未完成标志

}

//==========================================================
//	函数名称：	ESP8266_SendCmd
//
//	函数功能：	发送命令
//
//	入口参数：	cmd：命令
//				res：需要检查的返回指令
//
//	返回参数：	0-成功	1-失败
//
//	说明：		
//==========================================================
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
	
	unsigned char timeOut = 200;

	Usart_SendString(USART2, (unsigned char *)cmd, strlen((const char *)cmd));
	
	while(timeOut--)
	{
		if(ESP8266_WaitRecive() == REV_OK)							//如果收到数据
		{
			/* ⚠️ 必须用按字节查找：缓冲区里可能停着含 0x00 的 +IPD 二进制帧，
			 *    strstr 遇到 0x00 就停，帧后面的 AT 应答会被完全漏掉 */
			if(ESP8266_FindInBuf(res) != 0)							//如果检索到关键词
			{
				ESP8266_DropAtEcho();								//只丢 AT 回显，保留已收到的下行帧
				
				return 0;
			}
		}
		
		DelayMs(10);
	}
	
	return 1;

}

//==========================================================
//	函数名称：	ESP8266_WaitSendDone
//
//	函数功能：	等待 ESP8266 把上一包数据发完（回 "SEND OK"）
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		⚠️ ESP8266 收到数据后需要时间发出去，此时它会把紧接着到达的
//				AT+CIPSEND 当成"忙"而回 "busy p..."，拿不到 '>'，数据就发不出去。
//				这里最多等 300ms，等它回 "SEND OK"/"SEND FAIL" 再返回。
//				只读缓冲区、不做任何清理，绝不会吃掉同时到达的 +IPD 下行帧。
//==========================================================
static void ESP8266_WaitSendDone(void)
{
	unsigned char timeOut = 0;

	for(timeOut = 0; timeOut < 30; timeOut++)
	{
		if(ESP8266_FindInBuf("SEND OK") != 0)
			return;

		if(ESP8266_FindInBuf("SEND FAIL") != 0)
			return;

		DelayMs(10);
	}
}

//==========================================================
//	函数名称：	ESP8266_SendData
//
//	函数功能：	发送数据
//
//	入口参数：	data：数据
//				len：长度
//
//	返回参数：	无
//
//	说明：		⚠️ 拿不到 '>' 时原来直接放弃，一个字节都发不出去，串口也不打印任何东西
//				（表现为：工程里明明在"发布"，平台却收不到、没有 PUBACK、物模型不更新）
//				现在失败会等一会儿重试一次，成功后等 "SEND OK" 再做下一件事
//==========================================================
void ESP8266_SendData(unsigned char *data, unsigned short len)
{

	char cmdBuf[32];
	
	ESP8266_DropAtEcho();							//只丢 AT 回显：清空整个缓冲区会吃掉正在到达的下行帧
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);		//发送命令

	if(ESP8266_SendCmd(cmdBuf, ">"))				//收到‘>’时可以发送数据
	{
		/* 拿不到 '>'：ESP8266 上一包还没发完（回的是 "busy p..."），等一会儿重试一次 */
		DelayMs(300);

		ESP8266_DropAtEcho();

		if(ESP8266_SendCmd(cmdBuf, ">"))
		{
			UsartPrintf(USART_DEBUG, "WARN: CIPSEND %d Failed!\r\n", len);
			return;
		}
	}

	Usart_SendString(USART2, data, len);			//发送设备连接请求数据

	ESP8266_WaitSendDone();							//等 ESP8266 发完("SEND OK")，下一包才不会撞上"忙"

}

//==========================================================
//	函数名称：	ESP8266_GetIPD
//
//	函数功能：	获取平台返回的数据
//
//	入口参数：	等待的时间(乘以10ms)
//
//	返回参数：	平台返回的原始数据
//
//	说明：		不同网络设备返回的格式不同，需要去调试
//				如ESP8266的返回格式为	"+IPD,x:yyy"	x代表数据长度，yyy是数据内容
//==========================================================
unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{
	unsigned short i = 0;

	/* 取帧改为"按 +IPD 长度精确取帧 + 拷贝到独立缓存"，见 ESP8266_TryGetFrame；
	 * timeOut 传 0 表示非阻塞：只尝试一次，取不到立即返回，
	 * 这样主循环可以一次把缓冲区里已收到的多帧全部处理完 */
	do
	{
		if(ESP8266_TryGetFrame() != 0)
		{
			return g_IpdFrame;
		}

		if(timeOut == 0)
		{
			break;
		}

		DelayMs(5);
		i++;
	} while(i < timeOut);

	return NULL;
}

/* ---------------- 以下是旧实现，仅作参考，已废弃 ----------------
 * 缺点1：靠"两次计数相同"判断接收是否完成，可能把半截帧当成完整帧，
 *        解析失败后被协议层静默丢弃（串口一个字都不打印）；
 * 缺点2：返回的是接收缓冲区内部指针，后续任何一次清缓冲都会把数据破坏掉。
 * 上面新的 ESP8266_GetIPD + ESP8266_TryGetFrame 已解决这两个问题 */
#if 0
unsigned char *ESP8266_GetIPD_Obsolete(unsigned short timeOut)
{

	char *ptrIPD = NULL;
	
	do
	{
		if(ESP8266_WaitRecive() == REV_OK)								//如果接收完成
		{	
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");				//搜索“IPD”头
			if(ptrIPD == NULL)											//如果没找到，可能是IPD头的延迟，还是需要等待一会，但不会超过设定的时间
			{
				UsartPrintf(USART_DEBUG, "\"IPD\" not found\r\n");
			}
			else
			{
				ptrIPD = strchr(ptrIPD, ':');							//找到':'
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;
				
			}
		}
		
		DelayMs(5);
        timeOut--;		//延时等待
	} while(timeOut>0);
	
	return NULL;														//超时还未找到，返回空指针

}

#endif	/* 旧实现结束 */

//==========================================================
//	函数名称：	ESP8266_Init
//
//	函数功能：	初始化ESP8266
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void ESP8266_Init(void)
{
		
		ESP8266_Clear();
	
		/*测试ESP8266模块是否正常工作，是最基础的“唤醒”指令*/
		UsartPrintf(USART_DEBUG, "1. AT\r\n");
		while(ESP8266_SendCmd("AT\r\n", "OK"))
		DelayMs(500);
	
		/*设置ESP8266的工作模式为 Station模式（STA模式），即作为客户端连接到WiFi路由器。*/
		UsartPrintf(USART_DEBUG, "2. CWMODE\r\n");
		while(ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
		DelayMs(500);
	
		/*当 ESP8266 作为客户端连接路由器时，自动从路由器获取 IP 地址、子网掩码、网关等信息*/
		UsartPrintf(USART_DEBUG, "3. AT+CWDHCP\r\n");
		while(ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK"))
		DelayMs(500);
	
		/*连接到指定的WiFi热点，即输入WiFi的名称和密码。*/
		UsartPrintf(USART_DEBUG, "4. CWJAP\r\n");
		while(ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
		DelayMs(500);
	
		/*建立TCP或UDP网络连接，连接到指定的远程服务器。*/
		UsartPrintf(USART_DEBUG, "5. CIPSTART\r\n");
		while(ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT"))
		DelayMs(500);
	
		UsartPrintf(USART_DEBUG, "6. ESP8266 Init OK\r\n");

}

//==========================================================
//	函数名称：	USART2_IRQHandler
//
//	函数功能：	串口2收发中断
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void USART2_IRQHandler(void)
{
	unsigned short drop = 0;

	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) //接收中断
	{
		/* 缓冲区满：丢掉最旧的 1/4，保住最新数据。
		 * 不能像原来那样把计数直接归零 —— 那会把一帧数据从中间截断；
		 * 这里做移位是安全的：主循环整理缓冲区时会关闭本中断，两者不会并发 */
		if(esp8266_cnt >= ESP8266_RX_BUF_SIZE)
		{
			drop = ESP8266_RX_BUF_SIZE / 4;
			memmove(esp8266_buf, esp8266_buf + drop, (unsigned short)(esp8266_cnt - drop));
			esp8266_cnt = (unsigned short)(esp8266_cnt - drop);
			esp8266_cntPre = 0;
		}

		esp8266_buf[esp8266_cnt++] = USART2->DR;
		esp8266_buf[esp8266_cnt] = 0;			/* 维持字符串结尾，便于 strstr 查找 */

		USART_ClearFlag(USART2, USART_FLAG_RXNE);
	}

}
