#ifndef QQ_PARSER_INTERNAL_H
#define QQ_PARSER_INTERNAL_H

#include <QQ&WeixinCommon.h>
#include <Parser.h>
#include <QQParser.h>
#include <CommonDef.h>
#include "Sqlite3Helper.h"


bool AndroidInitEntry(LPCWSTR lpwzCasePath, char *lpszImei, bool isDbPath = false);
bool IosInitEntry(LPCWSTR lpwzCasePath, char *lpszImei, bool isDbPath = false);

bool AndroidGetAccountList(char *lpszAccounts, int iSize);
bool IosGetAccountList(char *lpszAccounts, int iSize);

bool AndroidGetAccountInfo(const std::string &account);
bool IosGetAccountInfo(const std::string &account);
void IosGetAccountInfoEx(const std::wstring &plistFile, AccountInfoEx &accountInfo);

void AndroidGetFriendInfo(sqlite3 *db, const std::string &account);
void AndroidGetGroupInfo(sqlite3 *db, const std::string &account);
void AndroidGetTroopInfo(sqlite3 *db, const std::string &account);
void AndroidGetDiscInfo(sqlite3 *db, const std::string &account);

void IosGetFriendInfo(sqlite3 *db, const std::string &account);
void IosGetGroupInfo(sqlite3 *db, const std::string &account);
void IosGetTroopInfo(sqlite3 *db, const std::string &account);
void IosGetDiscInfo(sqlite3 *db, const std::string &account);

void AndroidGetChatHistory(sqlite3 *db, const std::string &account);
void AndroidGetSingleChatHistory(sqlite3 *db, const std::string &account,
                                 const std::string &chatAccount,
                                 const std::string &chatAccountRemark,
                                 int iType);

void IosGetChatHistory(sqlite3 *db, const std::string &account);
void IosGetRecoveryChatHistory(const std::string &account);
void IosGetSingleChatHistory(sqlite3 *db,
                             const std::string &account,
                             const std::string &chatAccount,
                             const std::string &tableName,
                             int iType,
                             bool isRecovery = false);


#endif // QQ_PARSER_INTERNAL_H