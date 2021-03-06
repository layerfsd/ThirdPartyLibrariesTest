#pragma once
#include <string>
#include <list>
#include <map>
#include <Shlwapi.h>
#include <strsafe.h>
#include <ShlObj.h>
#include <ShellAPI.h>

using std::string;
using std::wstring;
using std::map;
using std::list;
using std::multimap;


//QQ特殊消息路径描述
//图片文件路径：tencent\mobileQQ\photo
//				tencent\mobileQQ\diskcache
//				tencent\QQ_Images
//音频文件路径：tencent\MobileQQ\*******\ptt
//视频文件路径：tencent\MobileQQ\shortvideo

//微信特殊消息路径描述
//图片文件路径：tencent\Micromsg\*************\image2
//音频文件路径：tencent\Micromsg\*************\voice2
//视频文件路径：tencent\Micromsg\*************\video


#define QQ_MSG_TXT	       -1000   //文本消息
#define QQ_SIGNATURE       -1034   //签名
#define QQ_SYSTEM_TXT      -1043   //系统消息
#define QQ_MSG_PIC	       -2000   //图片消息
#define QQ_MSG_AUDIO       -2002   //语音消息
#define QQ_MSG_FILE        -2005   //文件消息
#define QQ_DYNAMIC_FACE    -2007   //动态表情消息
#define QQ_MSG_CALL        -2009   //QQ电话和QQ视频
#define QQ_MSG_VIDEO       -2022   //视频消息
#define QQ_EVENT_REMID	   -2023   //提醒事件
#define QQ_MSG_MAP		   -2011   //地图位置
#define QQ_RCMD_CONTACT	   -2025   //推荐联系人
#define QQ_MSG_OTHER	   -3000   //其他消息
#define QQ_START_CALL	   -2016   //发起QQ电话、视频聊天
#define QQ_MAG_MULTI	   -1035   //多种类型
#define QQ_POKE	           -5012   //戳了一下
#define QQ_SIGNIN          -2054   //群签到
#define QQ_TOPIC           -2044   //话题



#define WEIXIN_MSG_TEXT		1		//文本消息
#define WEIXIN_MSG_PIC		3		//图片消息
#define WEIXIN_MSG_AUDIO	34		//语音消息
#define WEIXIN_MSG_CARD		42		//名片
#define WEIXIN_MSG_VIDEO	43		//视频
#define WEIXIN_DYNAMIC_FACE	47		//动态表情消息
#define WEIXIN_MSG_MAP		48		//地图位置
#define WEIXIN_MSG_CALL		50		//语音通话、视频通话
#define WEIXIN_MSG_PREVIDEO 62      //即时拍摄视频
#define WEIXIN_INTERPHONE	10000	//实时对讲
#define WEIXIN_MSG_OTHER	20000	//其他消息
#define WEIXIN_WEB_PIC      -1      //网络图片


struct FriendInfo
{
	string account;
	string nickName;
	string remark;
	string signature;
	int groupId;
	string groupName;
	int iType;
	string alias;
};

struct GroupInfo
{
	int groupId;
	string groupName;
	int memberCount;
};

struct TroopInfo
{
	string	troopUin;
	string troopName;
	string troopMemo;
};

struct DiscInfo
{
	string discussUin;
	string discussName;
	string createTime;
};

typedef struct MULTIMSG_ 
{
	int msgType; //文本,图片，视频，语音
	string content;
}MultiMsg, *LPMultiMsg;

struct ChatHistory
{
	string time;                    // 时间
	int msgType;                    // 消息类型
	int isSend;                     // 发送 - 1 接收 - 0
	string message;                 // 文本消息正文
	string filePath;                // 图片/录音/视频文件路径
	string thumbPath;               // 图片的缩略图路径
	string duration;                // 时长，单位:秒
	string location;                // 位置描述
	string latitude;                // 纬度
	string longitude;               // 经度
	string contactAccount;          // 共享联系人账号
	string contactNickName;         // 共享联系人昵称
	string contactCity;             // 共享联系人所在城市
	string contactSex;              // 共享联系人性别
	string senderUin;               // 发送者qq号
	string senderName;              // 发送者昵称
	int markType;
	list<MultiMsg> multiMsgLst;
};

typedef list<ChatHistory> ChatHistoryList;

struct ChatInfo 
{
	list<ChatHistory> chatList;
	list<ChatHistory>::iterator chatItera;
};

enum WeiXinAvatarType
{
	WXAT_ANDROID_DKJL = 1,  //安卓微信多开精灵
	WXAT_ANDROID_FSMS = 2,  //安卓微信分身秘书
	WXAT_ANDROID_DKMS = 3,  //安卓微信多开秘书
	WXAT_ANDROID_DKZS = 4,  //安卓微信多开助手
    WXAT_ANDROID_DKWSB = 5, //安卓微信多开微商版
    WXAT_ANDROID_FSHBB = 6, //安卓微信分身红包版
	WXAT_ANDROID_SELF = 7, //安卓自带分身

	WXAT_IOS_XYZS = 50,     //苹果微信XY助手

	WXAT_UNKNOW = 99,       //未知
};

struct AccountInfoEx
{
    string account;
    string weixinNumber;
    string nickName;
};