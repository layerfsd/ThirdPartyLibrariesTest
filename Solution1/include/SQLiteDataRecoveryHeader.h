#ifndef SQLITEDATARECOVERYHEADER_H
#define SQLITEDATARECOVERYHEADER_H

//#include <tchar.h>
#include <Windows.h>
#include <string>
#include <list>


#ifdef SQLITEDATARECOVERY_EXPORTS
    #define SQLITEDATARECOVERY_API extern "C" __declspec(dllexport)
#else
    #define SQLITEDATARECOVERY_API extern "C" __declspec(dllimport)
#endif

enum RecoveryAppType
{
    ANDROID_RECOVERY_CONTACT = -1,          // Android 通讯录
    ANDROID_RECOVERY_SMS = -2,              // Android 短信
    ANDROID_RECOVERY_RECORD = -3,           // Android 通话记录
    ANDROID_RECOVERY_WHATSAPP = -4,         // Android WhatsApp
    IOS_RECOVERY_CONTACT = -5,              // IOS 通讯录
    IOS_RECOVERY_SMS = -6,                  // IOS 短信
    IOS_RECOVERY_RECORD = -7,               // IOS 通话记录
    IOS_RECOVERY_QQ = -8,                   // IOS QQ
    IOS_RECOVERY_WEIXIN = -9,               // IOS 微信
    IOS_RECOVERY_WHATSAPP = -10,            // IOS WhatsApp
    IOS_RECOVERY_TELEGRAM = -11,             // IOS Telegram
    ANDROID_RECOVERY_WEIXIN = -12,          // Android 微信

};

struct TargetTableInfo
{
    RecoveryAppType recoveryAppType;                            // 恢复应用类型
    std::wstring mainColumnName;                                // 关键列列名
    std::wstring uniqueColumnName;                              // 唯一列列名，该列内容唯一，作为恢复结果的过滤项
    std::wstring srcTableName;                                  // 原数据所在表名
    std::wstring dstTableName;                                  // 恢复数据所在表名
    std::string chatAccount;                                    // IOS 好友/群/讨论组QQ账号
};


class SQLiteDataRecovery
{
public:
    virtual void StartRecovery(const std::wstring &dbPath, const std::list<TargetTableInfo> &targetTableInfos) = 0;
};


SQLITEDATARECOVERY_API SQLiteDataRecovery* CreateRecoveryClass();

SQLITEDATARECOVERY_API void DestoryRecoveryClass(SQLiteDataRecovery *obj);

#endif // SQLITEDATARECOVERYHEADER_H