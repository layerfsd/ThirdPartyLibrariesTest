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
    ANDROID_RECOVERY_CONTACT = -1,          // Android ͨѶ¼
    ANDROID_RECOVERY_SMS = -2,              // Android ����
    ANDROID_RECOVERY_RECORD = -3,           // Android ͨ����¼
    ANDROID_RECOVERY_WHATSAPP = -4,         // Android WhatsApp
    IOS_RECOVERY_CONTACT = -5,              // IOS ͨѶ¼
    IOS_RECOVERY_SMS = -6,                  // IOS ����
    IOS_RECOVERY_RECORD = -7,               // IOS ͨ����¼
    IOS_RECOVERY_QQ = -8,                   // IOS QQ
    IOS_RECOVERY_WEIXIN = -9,               // IOS ΢��
    IOS_RECOVERY_WHATSAPP = -10,            // IOS WhatsApp
    IOS_RECOVERY_TELEGRAM = -11,             // IOS Telegram
    ANDROID_RECOVERY_WEIXIN = -12,          // Android ΢��

};

struct TargetTableInfo
{
    RecoveryAppType recoveryAppType;                            // �ָ�Ӧ������
    std::wstring mainColumnName;                                // �ؼ�������
    std::wstring uniqueColumnName;                              // Ψһ����������������Ψһ����Ϊ�ָ�����Ĺ�����
    std::wstring srcTableName;                                  // ԭ�������ڱ���
    std::wstring dstTableName;                                  // �ָ��������ڱ���
    std::string chatAccount;                                    // IOS ����/Ⱥ/������QQ�˺�
};


class SQLiteDataRecovery
{
public:
    virtual void StartRecovery(const std::wstring &dbPath, const std::list<TargetTableInfo> &targetTableInfos) = 0;
};


SQLITEDATARECOVERY_API SQLiteDataRecovery* CreateRecoveryClass();

SQLITEDATARECOVERY_API void DestoryRecoveryClass(SQLiteDataRecovery *obj);

#endif // SQLITEDATARECOVERYHEADER_H