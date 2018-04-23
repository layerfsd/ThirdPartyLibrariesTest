#ifndef WEIXIN_PARSER_INTERNAL_H
#define WEIXIN_PARSER_INTERNAL_H

#include <QQ&WeixinCommon.h>
#include <Parser.h>
#include <WeixinParser.h>
#include <CommonDef.h>
#include "Sqlite3Helper.h"

bool AndroidInitEntry(LPCWSTR lpwzCasePath, char *lpszImei, bool isDbPath = false);
bool IosInitEntry(LPCWSTR lpwzCasePath, char *lpszImei, bool isDbPath = false);

bool AndroidGetAccountList(char *lpszAccounts, int iSize);
bool IosGetAccountList(char *lpszAccounts, int iSize);

bool AndroidGetAccountInfo(const std::string &account);

void AndroidGetFriendInfo(sqlite3 *db, const std::string &account, bool isRecovery = false);
void AndroidGetRecoveryFriendInfo(sqlite3 *db, const std::string &account);
void AndroidGetChatHistory(sqlite3 *db, const std::string &account); 
void AndroidGetRecoveryChatHistory(sqlite3 *db, const std::string &account); 
void AndroidGetSingleChatHistory(sqlite3 *db,
                                 const std::string &account,
                                 const FriendInfo &friendInfo,
                                 int iType,
                                 bool isRecovery = false);

void IosGetAccountInfoEx(const std::wstring &plistFile, AccountInfoEx &accountInfo);
bool IosGetAccountInfo(const std::string &account);
void IosDecryptFriendData(const char *data, int dataSize, FriendInfo &friendInfo);
void IosGetFriendInfoEx(sqlite3 *db, const std::string &account, const std::string &tableName, bool isRecovery = false);
void IosGetFriendInfo(sqlite3 *db, const std::string &account, int flag);
void IosGetChatHistory(sqlite3 *db, const std::string &account); 

void IosGetRecoveryFriendInfo(const std::string &account);
void IosGetRecoveryChatHistory(const std::string &account); 

void IosGetSingleChatHistory(sqlite3 *db, const std::string &account, const std::string &chatAccount, int iType);
void IosGetSingleChatHistoryWithoutFriendAccountEx(sqlite3 *db,
                                                   const std::string &account,
                                                   const std::string &chatAccountMd5,
                                                   const std::string &tableName,
                                                   int iType,
                                                   bool isRecovery = false);
void IosGetSingleChatHistoryWithoutFriendAccount(sqlite3 *db, const std::string &account, int iType);

#endif // WEIXIN_PARSER_INTERNAL_H