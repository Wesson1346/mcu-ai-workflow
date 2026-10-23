/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	onenet.c
	*
	*	作者： 		张继瑞
	*
	*	日期： 		2017-05-08
	*
	*	版本： 		V1.1
	*
	*	说明： 		与onenet平台的数据交互接口层
	*
	*	修改记录：	V1.0：协议封装、返回判断都在同一个文件，并且不同协议接口不同。
	*				V1.1：提供统一接口供应用层使用，根据不同协议文件来封装协议相关的内容。
	************************************************************
	************************************************************
	************************************************************
**/

//单片机头文件
#include "stm32f10x.h"

//网络设备
#include "esp8266.h"

//协议文件
#include "onenet.h"
#include "MqttKit.h"

//硬件驱动
#include "bsp_usart.h"
#include "bsp_delay.h"

//C库
#include <string.h>
#include <stdio.h>

#include "cJSON.h"

/* 设备配置区（产品ID / 设备名 / 鉴权Token）与物模型主题宏统一在 onenet.h 中定义，
 * 换设备时只需要改 onenet.h 一个地方 */

extern unsigned char esp8266_buf[512];

/* 应用层注册的属性设置回调，未注册时为 0 */
static OneNet_PropertySetCb g_PropertySetCb = 0;

//==========================================================
//	函数名称：	OneNet_RegisterPropertySetCb
//
//	函数功能：	注册属性设置（App 下行指令）回调
//
//	入口参数：	cb：回调函数，传 0 表示注销
//
//	返回参数：	无
//
//	说明：		具体业务动作由应用层在回调里实现，本文件保持协议层纯净
//==========================================================
void OneNet_RegisterPropertySetCb(OneNet_PropertySetCb cb)
{
	g_PropertySetCb = cb;
}

//==========================================================
//	函数名称：	OneNet_SendPropertySetReply
//
//	函数功能：	回复属性设置结果
//
//	入口参数：	msg_id：平台下发的消息 id（必须原样带回）
//
//	返回参数：	无
//
//	说明：		OneNET 要求设备对属性设置指令回执，否则 App 端会一直等待直到超时
//==========================================================
static void OneNet_SendPropertySetReply(const char *msg_id)
{
	char msgBuf[96];

	sprintf(msgBuf, "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", msg_id);

	UsartPrintf(USART_DEBUG, "Send SetReply: %s\r\n", msgBuf);

	OneNet_Publish(ONENET_TOPIC_SET_REPLY, msgBuf);
}
//==========================================================
//	函数名称：	OneNet_DevLink
//
//	函数功能：	与onenet创建连接
//
//	入口参数：	无
//
//	返回参数：	1-成功	0-失败
//
//	说明：		与onenet平台建立连接
//==========================================================
_Bool OneNet_DevLink(void)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};					//协议包

	unsigned char *dataPtr;
	int i = 0;
	_Bool status = 1;
	
	UsartPrintf(USART_DEBUG, "OneNet_DevLink\r\n"
							"PROID: %s,	TOKEN: %s, DEVID:%s\r\n"
                        , ONENET_PROID, ONENET_TOKEN, ONENET_DEVID);
	
	if(MQTT_PacketConnect(ONENET_PROID, ONENET_TOKEN, ONENET_DEVID, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//上传平台
		dataPtr = ESP8266_GetIPD(250);									//等待平台响应

		if(dataPtr != NULL)
		{	
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0:UsartPrintf(USART_DEBUG, "Tips:	连接成功\r\n");status = 0;break;
					
					case 1:UsartPrintf(USART_DEBUG, "WARN:	连接失败：协议错误\r\n");break;
					case 2:UsartPrintf(USART_DEBUG, "WARN:	连接失败：非法的clientid\r\n");break;
					case 3:UsartPrintf(USART_DEBUG, "WARN:	连接失败：服务器失败\r\n");break;
					case 4:UsartPrintf(USART_DEBUG, "WARN:	连接失败：用户名或密码错误\r\n");break;
					case 5:UsartPrintf(USART_DEBUG, "WARN:	连接失败：非法链接(比如token非法)\r\n");break;
					
					default:UsartPrintf(USART_DEBUG, "ERR:	连接失败：未知错误\r\n");break;
				}
			}
		}
		
		MQTT_DeleteBuffer(&mqttPacket);								//删包
	}
	else
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");
	
	return status;
	
}

//==========================================================
//	函数名称：	OneNet_Subscribe
//
//	函数功能：	订阅
//
//	入口参数：	topics：订阅的topic
//				topic_cnt：topic个数
//
//	返回参数：	SEND_TYPE_OK-成功	SEND_TYPE_SUBSCRIBE-需要重发
//
//	说明：		
//==========================================================
void OneNet_Subscribe(const char *topics[], unsigned char topic_cnt)
{
	
	unsigned char i = 0;
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};							//协议包
	
	for(; i < topic_cnt; i++)
		UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topics[i]);
	
	/* 订阅用 QoS1：MQTT 规则是"实际投递 QoS = min(发布方用的 QoS, 订阅时声明的 QoS)"，
	 * 订阅时声明 QoS0 会把平台下发的属性设置降级成"最多一次"，丢了不重发，
	 * 表现就是 App 点了开关但设备收不到 */
	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL1, topics, topic_cnt, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);					//向平台发送订阅请求
		
		MQTT_DeleteBuffer(&mqttPacket);											//删包
	}

}

//==========================================================
//	函数名称：	OneNet_Publish
//
//	函数功能：	发布消息
//
//	入口参数：	topic：发布的主题
//				msg：消息内容
//
//	返回参数：	SEND_TYPE_OK-成功	SEND_TYPE_PUBLISH-需要重送
//
//	说明：		
//==========================================================
void OneNet_Publish(const char *topic, const char *msg)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};							//协议包
	
	UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);
	
	/* 新版物模型属性上报必须使用 QoS1，QoS0 会被 OneNET 平台直接丢弃 */
	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg),MQTT_QOS_LEVEL1, 0, 1, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);					//向平台发送发布消息
		
		MQTT_DeleteBuffer(&mqttPacket);											//删包
	}

}

//==========================================================
//	函数名称：	OneNet_RevPro
//
//	函数功能：	平台返回数据检测
//
//	入口参数：	dataPtr：平台返回的数据
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void OneNet_RevPro(unsigned char *cmd)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};								//协议包
	
	char *req_payload = NULL;
	char *cmdid_topic = NULL;
	
	unsigned short topic_len = 0;
	unsigned short req_len = 0;
	
	unsigned char type = 0;
	unsigned char qos = 0;
	static unsigned short pkt_id = 0;
	
	short result = 0;
	cJSON *json, *params_json, *val_json, *id_json;
	char *dataPtr = NULL;
	char *msg_id = "1";														//本次下发消息的id，回执时原样带回
	
	type = MQTT_UnPacketRecv(cmd);
	switch(type)
	{
		case MQTT_PKT_CMD:															//命令下发
			
			result = MQTT_UnPacketCmd(cmd, &cmdid_topic, &req_payload, &req_len);	//解出topic和消息体
			if(result == 0)
			{
				UsartPrintf(USART_DEBUG, "cmdid: %s, req: %s, req_len: %d\r\n", cmdid_topic, req_payload, req_len);
				
				if(MQTT_PacketCmdResp(cmdid_topic, req_payload, &mqttPacket) == 0)	//命令回复组包
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send CmdResp\r\n");
					
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//回复命令
					MQTT_DeleteBuffer(&mqttPacket);									//删包
				}
			}
		
		break;
			
		case MQTT_PKT_PUBLISH:														//接收的Publish消息
		
			//解析接收到的MQTT PUBLISH 报文
			result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id);
			if(result == 0)
			{
				UsartPrintf(USART_DEBUG, "topic: %s, topic_len: %d, payload: %s, payload_len: %d\r\n",
																	cmdid_topic, topic_len, req_payload, req_len);
				
				// 对数据包req_payload进行JSON格式解析
				json = cJSON_Parse(req_payload);
				if(json != NULL)
				{
					params_json = cJSON_GetObjectItem(json, "params");
					if(params_json != NULL)
					{
						/* 取出本次下发消息的 id，回执时必须原样带回 */
						id_json = cJSON_GetObjectItem(json, "id");
						if(id_json != NULL && id_json->valuestring != NULL)
							msg_id = id_json->valuestring;

						/* 具体业务动作（开关风扇/切模式/改阈值等）交给应用层注册的回调实现，
						 * 本文件保持协议层纯净 */
						if(g_PropertySetCb != 0)
							g_PropertySetCb(msg_id, params_json);

						/* OneNET 要求设备对属性设置指令回执，否则 App 端会一直等待直到超时 */
						OneNet_SendPropertySetReply(msg_id);

						/* 调试打印：遍历 params 下的字段 */
						val_json = params_json->child;
						while(val_json != NULL)
						{
							/* OneNET 物模型下发的属性形如 {"temp":{"value":30}}，
							 * 数值型属性的真实值在子对象 value 里，需要再取一层 */
							cJSON *value_json = cJSON_GetObjectItem(val_json, "value");

							if(value_json == NULL)
								value_json = val_json;			/* 兼容未用 {"value":...} 包装的旧格式 */

							if(value_json->valuestring != NULL)
								UsartPrintf(USART_DEBUG, "  [%s] = %s\r\n",
											val_json->string, value_json->valuestring);
							else
								UsartPrintf(USART_DEBUG, "  [%s] = %d\r\n",
											val_json->string, value_json->valueint);

							val_json = val_json->next;
						}
					}
					//释放JSON对象树内存
					cJSON_Delete(json);
				}

				/* 若平台是按 QoS1 下发的，必须回 PUBACK：
				 * 否则平台认为没送达（重连后会重发），订阅的可靠性也形同虚设 */
				if(qos == MQTT_QOS_LEVEL1)
				{
					if(MQTT_PacketPublishAck(pkt_id, &mqttPacket) == 0)
					{
						UsartPrintf(USART_DEBUG, "Tips:	Send PublishAck\r\n");
						ESP8266_SendData(mqttPacket._data, mqttPacket._len);
						MQTT_DeleteBuffer(&mqttPacket);
					}
				}
			}
		break;
			
		case MQTT_PKT_PUBACK:														//发送Publish消息，平台回复的Ack
		
			if(MQTT_UnPacketPublishAck(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Publish Send OK\r\n");
			
		break;
			
		case MQTT_PKT_PUBREC:														//发送Publish消息，平台回复的Rec，设备需回复Rel消息
		
			if(MQTT_UnPacketPublishRec(cmd) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishRec\r\n");
				if(MQTT_PacketPublishRel(MQTT_PUBLISH_ID, &mqttPacket) == 0)
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send PublishRel\r\n");
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);
					MQTT_DeleteBuffer(&mqttPacket);
				}
			}
		
		break;
			
		case MQTT_PKT_PUBREL:														//收到Publish消息，设备回复Rec后，平台回复的Rel，设备需再回复Comp
			
			if(MQTT_UnPacketPublishRel(cmd, pkt_id) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishRel\r\n");
				if(MQTT_PacketPublishComp(MQTT_PUBLISH_ID, &mqttPacket) == 0)
				{
					UsartPrintf(USART_DEBUG, "Tips:	Send PublishComp\r\n");
					ESP8266_SendData(mqttPacket._data, mqttPacket._len);
					MQTT_DeleteBuffer(&mqttPacket);
				}
			}
		
		break;
		
		case MQTT_PKT_PUBCOMP:													//发送Publish消息，平台返回Rec，设备回复Rel，平台再返回的Comp
		
			if(MQTT_UnPacketPublishComp(cmd) == 0)
			{
				UsartPrintf(USART_DEBUG, "Tips:	Rev PublishComp\r\n");
			}
		
		break;
			
		case MQTT_PKT_SUBACK:														//发送Subscribe消息的Ack
		
			if(MQTT_UnPacketSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe Err\r\n");
		
		break;
			
		case MQTT_PKT_UNSUBACK:													//发送UnSubscribe消息的Ack
		
			if(MQTT_UnPacketUnSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT UnSubscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT UnSubscribe Err\r\n");
		
		break;
		
		default:
			/* 报文类型无法识别：把类型码和首字节打出来，
			 * 便于判断是否收到了半截/错位的帧（原来这里是完全静默的） */
			UsartPrintf(USART_DEBUG, "WARN:\t无法识别的MQTT报文: type=%d, firstByte=0x%02X\r\n",
						type, (unsigned char)cmd[0]);
			result = -1;
		break;
	}
	
	/* 接收缓冲区已由 esp8266.c 按帧消费（ESP8266_TryGetFrame 只丢掉已处理的那一帧），
	 * 这里不能再整缓冲清空，否则会把缓冲区里还没处理的下一帧一起丢掉
	 * —— 这正是"App 连点开关时后面的指令丢失"的原因之一 */
	
	if(result == -1)
		return;
	
	if(type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}

}
