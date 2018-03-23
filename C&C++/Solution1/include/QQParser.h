#ifndef _QQ_PARSER_H
#define _QQ_PARSER_H

#ifdef QQPARSER_EXPORTS
#define QQ_API  __declspec(dllexport)
#else
#define QQ_API  __declspec(dllimport)
#endif

#include <Windows.h>

//iType: 0-android 1-ios
//
QQ_API bool QQ_InitEntry(LPCWSTR lpwzCasePath, int iType, char *lpszImei = NULL, bool isDbPath = false/*ֱ����QQ���ݿ�·��?*/);   

QQ_API void QQ_Free(); 

//�����˺��б���"|"����
//
QQ_API bool QQ_GetAccountList(char *lpszAccounts, int iSize);

//��ȡ�˺���Ϣ
//
QQ_API bool QQ_GetAccoutInfo(const std::string &account);
QQ_API AccountInfoEx* QQ_GetAccoutInfoEx(const std::string &account);

//��ȡָ���˺ŵĺ�����Ϣ
//
QQ_API FriendInfo* QQ_GetFirstFriend(std::string &account);
QQ_API FriendInfo* QQ_GetNextFriend(std::string &account);

//��ȡָ���˺ŵķ�����Ϣ
//
QQ_API GroupInfo* QQ_GetFirstGroup(std::string &account); 
QQ_API GroupInfo* QQ_GetNextGroup(std::string &account);

//��ȡָ���˺ŵ�Ⱥ��Ϣ
//
QQ_API TroopInfo* QQ_GetFirstTroop(std::string &account);
QQ_API TroopInfo* QQ_GetNextTroop(std::string &account);

//��ȡָ���˺ŵ���������Ϣ
//
QQ_API DiscInfo* QQ_GetFirstDisc(std::string &account);
QQ_API DiscInfo* QQ_GetNextDisc(std::string &account);

//��ȡָ�����ˡ�QQȺ��������������¼
//account:Ҫ��ȡ���˺� chatAccount:Ҫ��ȡ�����¼��QQ�š�Ⱥ�Ż���������� iType��Ҫ��ȡ�������¼������ 0-���� 1-Ⱥ 2-������
//
QQ_API ChatHistory* QQ_GetFirstChatHistory(std::string &account, std::string &chatAccount, int iType, bool isRecovery = false);
QQ_API ChatHistory* QQ_GetNextChatHistory(std::string &account, std::string &chatAccount, int iType, bool isRecovery = false);

#endif