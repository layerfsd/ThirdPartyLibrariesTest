#include <tchar.h>
#include <Windows.h>
#include <strsafe.h>
#include <stdio.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <algorithm>
#include <assert.h>
#include <process.h>
#include "..\Utils\StringConvertor.h"
#include "MTPInfo.h"
#include <DebugLog.h>


#include <SQLiteDataRecoveryHeader.h>
#pragma comment(lib, "SQLiteDataRecovery")


void cb_ObjectInfo(const ObjectInfo *objectInfo)
{
    
}


#include <openssl/md5.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#ifdef _DEBUG
    #pragma comment(lib, "libcryptoMTd")
#else
    #pragma comment(lib, "libcryptoMT")
#endif

#pragma comment(lib, "crypt32")

std::wstring GetMD5(const std::string &src)
{
    unsigned char hash[16] = {0};
    MD5_CTX ctx = {0};
    MD5_Init(&ctx);
    MD5_Update(&ctx, src.c_str(), src.size());
    MD5_Final(hash, &ctx);

    wchar_t hex[128] = {0};
    for ( int i = 0; i < 16; ++i )
    {
        StringCbPrintfW(hex, sizeof(hex), L"%s%.2x", hex, hash[i]);
    }

    return hex;
}



int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    //MTPInfo mtp(cb_ObjectInfo);
    //if (MTP_MODE == mtp.CheckDeviceMode())
    //{
    //    mtp.StartTask(L"F:\\phonePicture\\", L"DCIM");
    //}

    
    /*SYSTEMTIME sysTime = {0};
    FILETIME fileTime = {0};
    FILETIME localFileTime = {0};
    LARGE_INTEGER offset = {0};
    offset.QuadPart = 131568352328397184;
    fileTime.dwHighDateTime = offset.HighPart;
    fileTime.dwLowDateTime = offset.LowPart;


    FileTimeToSystemTime(&fileTime, &sysTime);

    int a = 0;*/



    // SQlite反删除
    /*std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\sms.db";
    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    tableInfo.recoveryAppType = IOS_RECOVERY_SMS;
    tableInfo.srcTableName = L"message";
    tableInfo.dstTableName = L"message_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"text";
    tableInfo.uniqueColumnName = L"guid";
    targetTableInfos.push_back(tableInfo);

    tableInfo.recoveryAppType = IOS_RECOVERY_SMS;
    tableInfo.srcTableName = L"handle";
    tableInfo.dstTableName = L"handle_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"service";
    tableInfo.uniqueColumnName = L"id";
    targetTableInfos.push_back(tableInfo);*/

    /*std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\CallHistory.storedata";
    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    tableInfo.recoveryAppType = IOS_RECOVERY_RECORD;
    tableInfo.srcTableName = L"ZCALLRECORD";
    tableInfo.dstTableName = L"ZCALLRECORD_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"ZADDRESS";
    tableInfo.uniqueColumnName = L"ZUNIQUE_ID";
    targetTableInfos.push_back(tableInfo);*/


    //std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\AddressBook.sqlitedb";
    //std::list<TargetTableInfo> targetTableInfos;
    //TargetTableInfo tableInfo;
    //tableInfo.recoveryAppType = IOS_RECOVERY_CONTACT;
    //tableInfo.srcTableName = L"ABPersonFullTextSearch_content";
    //tableInfo.dstTableName = L"ABPersonFullTextSearch_content_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    //tableInfo.mainColumnName = L"c16Phone";
    //tableInfo.uniqueColumnName = L"docid";
    ////tableInfo.mainColumnName = L"c15Phone";
    //targetTableInfos.push_back(tableInfo);

    //tableInfo.recoveryAppType = IOS_RECOVERY_CONTACT;
    //tableInfo.srcTableName = L"ABMultiValue";
    //tableInfo.dstTableName = L"ABMultiValue_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    //tableInfo.mainColumnName = L"guid";
    //tableInfo.uniqueColumnName = L"guid";
    //targetTableInfos.push_back(tableInfo);

    /*std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\contacts2.db";
    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    tableInfo.recoveryAppType = ANDROID_RECOVERY_CONTACT;
    tableInfo.srcTableName = L"raw_contacts";
    tableInfo.dstTableName = L"raw_contacts_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"display_name";
    tableInfo.uniqueColumnName = L"_id";
    targetTableInfos.push_back(tableInfo);

    tableInfo.recoveryAppType = ANDROID_RECOVERY_CONTACT;
    tableInfo.srcTableName = L"data";
    tableInfo.dstTableName = L"data_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"data1";
    tableInfo.uniqueColumnName.clear();
    targetTableInfos.push_back(tableInfo);*/


    /*std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\contacts2.db";
    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    tableInfo.recoveryAppType = ANDROID_RECOVERY_RECORD;
    tableInfo.srcTableName = L"calls";
    tableInfo.dstTableName = L"calls_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"duration";
    tableInfo.uniqueColumnName = L"date";
    targetTableInfos.push_back(tableInfo);*/

    /*std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\mmssms.db";
    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    tableInfo.recoveryAppType = ANDROID_RECOVERY_SMS;
    tableInfo.srcTableName = L"sms";
    tableInfo.dstTableName = L"sms_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"body";
    tableInfo.uniqueColumnName = L"date";
    targetTableInfos.push_back(tableInfo);*/

    /*std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\QQ.db";
    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    tableInfo.recoveryAppType = IOS_RECOVERY_QQ;
    tableInfo.chatAccount = "2010741172";
    tableInfo.srcTableName = L"tb_c2cMsg_2010741172";
    tableInfo.dstTableName = L"tb_c2cMsg_2010741172_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"content";
    tableInfo.uniqueColumnName = L"time";
    targetTableInfos.push_back(tableInfo);*/

    /*std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\MM.sqlite";
    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    tableInfo.recoveryAppType = IOS_RECOVERY_WEIXIN;
    tableInfo.mainColumnName = L"Message";
    tableInfo.uniqueColumnName = L"CreateTime";
    tableInfo.srcTableName = L"Chat_75b941e0218e9fc88aebc0ae5058e17f";
    tableInfo.dstTableName = L"Chat_75b941e0218e9fc88aebc0ae5058e17f_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    targetTableInfos.push_back(tableInfo);*/

    //std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\FTS5IndexMicroMsg.db";
    //std::list<TargetTableInfo> targetTableInfos;
    //TargetTableInfo tableInfo;
    //tableInfo.recoveryAppType = ANDROID_RECOVERY_WEIXIN;
    //tableInfo.srcTableName = L"FTS5MetaMessage";
    //tableInfo.dstTableName = L"FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    //tableInfo.mainColumnName = L"talker";
    //tableInfo.uniqueColumnName = L"timestamp";
    //targetTableInfos.push_back(tableInfo);

    //tableInfo.recoveryAppType = ANDROID_RECOVERY_WEIXIN;
    //tableInfo.srcTableName = L"FTS5IndexMessage_content";
    //tableInfo.dstTableName = L"FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    //tableInfo.mainColumnName = L"c0";
    //tableInfo.uniqueColumnName = L"c0";
    //targetTableInfos.push_back(tableInfo);

    //SQLiteDataRecovery *dbRecoveryer = CreateRecoveryClass();
    //if (dbRecoveryer)
    //{
    //    dbRecoveryer->StartRecovery(dbPath, targetTableInfos);

    //    /*std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\EnMicroMsg-Descrypt.db";
    //    targetTableInfos.clear();
    //    TargetTableInfo tableInfo;
    //    tableInfo.recoveryAppType = ANDROID_RECOVERY_WEIXIN;
    //    tableInfo.srcTableName = L"message";
    //    tableInfo.dstTableName = L"message_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    //    tableInfo.mainColumnName = L"imgPath";
    //    tableInfo.uniqueColumnName = L"createTime";
    //    targetTableInfos.push_back(tableInfo);
    //    
    //    dbRecoveryer->StartRecovery(dbPath, targetTableInfos);*/

    //    DestoryRecoveryClass(dbRecoveryer);
    //}


    std::wstring dbPath = L"E:\\Project\\test\\ConsoleApplication2\\fts\\fts_message.db";
    wchar_t buffer[16] = {0};
    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;

    tableInfo.recoveryAppType = IOS_RECOVERY_WEIXIN;
    tableInfo.mainColumnName = L"UsrName";
    tableInfo.uniqueColumnName = L"UsrName";
    tableInfo.srcTableName = L"fts_username_id";
    tableInfo.dstTableName = L"fts_username_id_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    targetTableInfos.push_back(tableInfo);

    
    tableInfo.mainColumnName = L"c3Message";
    tableInfo.uniqueColumnName = L"c2CreateTime";

    for (int i = 1; i < 10; ++i)
    {
        memset(buffer, 0, sizeof(buffer));
        StringCbPrintfW(buffer, sizeof(buffer), L"%d", i);
        tableInfo.srcTableName = L"fts_message_table_" + std::wstring(buffer) + L"_content";
        tableInfo.dstTableName = L"fts_message_table_" + std::wstring(buffer) + L"_content_694F69B9_93B9_480a_B23C_F6D221EEAF39";

        targetTableInfos.push_back(tableInfo);
    }

    std::wstring dbBackPath;
    std::wstring srcWalFile;
    std::wstring dstWalFile;
    std::wstring srcShmFile;
    std::wstring dstShmFile;

    dbBackPath = dbPath + L"_back";
    if (!CopyFileW(dbPath.c_str(), dbBackPath.c_str(), FALSE))
    {
        return 0;
    }

    srcWalFile = dbPath + L"-wal";
    dstWalFile = dbBackPath + L"-wal";
    if (PathFileExistsW(srcWalFile.c_str()))
    {
        if (!CopyFileW(srcWalFile.c_str(), dstWalFile.c_str(), FALSE))
        {
            return 0;
        }
    }

    srcShmFile = dbPath + L"-shm";
    dstShmFile = dbBackPath + L"-shm";
    if (PathFileExistsW(srcShmFile.c_str()))
    {
        if (!CopyFileW(srcShmFile.c_str(), dstShmFile.c_str(), FALSE))
        {
            return 0;
        }
    }

    SQLiteDataRecovery *dbRecoveryer = CreateRecoveryClass();
    if (dbRecoveryer)
    {
        dbRecoveryer->StartRecovery(dbPath, targetTableInfos);
        DestoryRecoveryClass(dbRecoveryer);
    }

    DeleteFileW(dbBackPath.c_str());
    DeleteFileW(dstWalFile.c_str());
    DeleteFileW(dstShmFile.c_str());

    return 0;
}