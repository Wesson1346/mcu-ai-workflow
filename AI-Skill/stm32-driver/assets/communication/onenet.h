#ifndef _ONENET_H_
#define _ONENET_H_

/* ================== OneNET 设备配置区（在 OneNET 平台创建产品/设备后填入） ================== */
/* 产品 ID（OneNET 控制台 -> 产品 -> 产品ID） */
#define ONENET_PROID	"YOUR_PROID"

/* 设备名称（OneNET 控制台 -> 设备 -> 设备名称） */
#define ONENET_DEVID	"YOUR_DEVID"

/* 鉴权 Token（OneNET 控制台 -> 设备详情 -> 鉴权信息，或由 Token 工具生成） */
#define ONENET_TOKEN	"version=2018-10-31&res=products%2FYOUR_PROID%2Fdevices%2FYOUR_DEVID&et=YOUR_EXPIRE_TS&method=md5&sign=YOUR_SIGN"

/* ================== 物模型主题（由上面的配置自动拼接，一般无需修改） ================== */
/* 属性上报：设备 -> 平台（发布） */
#define ONENET_TOPIC_POST		"$sys/" ONENET_PROID "/" ONENET_DEVID "/thing/property/post"
/* 属性设置：平台 -> 设备（订阅，用于接收 App 下发的指令） */
#define ONENET_TOPIC_SET		"$sys/" ONENET_PROID "/" ONENET_DEVID "/thing/property/set"
/* 属性设置回复：设备 -> 平台（发布，收到设置指令后回执） */
#define ONENET_TOPIC_SET_REPLY	"$sys/" ONENET_PROID "/" ONENET_DEVID "/thing/property/set_reply"


/* ================== 属性设置（App 下行指令）回调接口 ================== */
/* 前向声明，避免本头文件依赖 cJSON.h；应用层需要遍历 params 时自行包含 cJSON.h */
struct cJSON;

/*
 * 函数名：OneNet_PropertySetCb（函数指针类型）
 * 描述  ：物模型属性设置（平台 -> 设备）的应用层回调
 * 输入  ：msg_id-本次下发消息的 id；params-平台下发的 params 对象
 * 备注  ：params 指向协议层临时解析出的 cJSON 对象，仅在回调期间有效，
 *         不要保存该指针；业务动作（开关风扇/切模式/改阈值等）在回调里实现
 */
typedef void (*OneNet_PropertySetCb)(const char *msg_id, struct cJSON *params);


_Bool OneNet_DevLink(void);

void OneNet_Subscribe(const char *topics[], unsigned char topic_cnt);

void OneNet_RevPro(unsigned char *cmd);

void OneNet_Publish(const char *topic, const char *msg);

/*
 * 函数名：OneNet_RegisterPropertySetCb
 * 描述  ：注册属性设置回调（应用层在连接云平台前调用一次）
 * 输入  ：cb-回调函数（传 0 表示注销）
 * 输出  ：无
 */
void OneNet_RegisterPropertySetCb(OneNet_PropertySetCb cb);


#endif
