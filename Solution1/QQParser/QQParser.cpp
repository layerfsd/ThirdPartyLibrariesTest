// QQParser.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <Shlwapi.h>
#include <map>
#include <vector>
#include <sstream>
#include <openssl\md5.h>
#include "QQParserInternal.h"
#include "../Utils/tinyxml/tinyxml.h"
#include "../Utils/StringConvertor.h"
#include <plist\plist.h>
#include <SQLiteDataRecoveryHeader.h>

#include "MessageForPic.h"
#include "MessageForVideo.h"
#include "MessageForPtt.h"
#include "MessageForMulti.h"


#pragma comment(lib, "ShlWapi.lib")

// openssl
#pragma comment(lib, "libcrypto")


std::wstring	g_qqDirPath;
SQLITE3_HELPER	Sqlite3Helper;
int				g_iType;
std::string		g_imei;
std::string		g_imei2;
wstring         _appDirPath;
bool			_isSync;

std::map<std::string, AccountInfo> g_accountInfo;
std::map<std::string, AccountInfoEx> g_accountInfoEx;

/*
wstring msg_body = "{
					"filename" : "3a97660cf97243cd93f3451f7707e7ae20160607092025.jpg",
					"hd" : false,
					"md5" : "c98541fcffc337076f1fd92bf6bffd6e",
					"size" : 126669,
					"uri" : "http://nos.netease.com/yixinpublic/pr_srbuqwcuphdjopnlnztwow==_1465262425_129986814",
					"url" : "http://nos.netease.com/yixinpublic/pr_srbuqwcuphdjopnlnztwow==_1465262425_129986814"
				   }"
*/
//功能: 解析以上格式内容的字符串
//Demo:
//		wstring fileName = MsgBodyParseA(msg_body, "filename");
//		运行后，fileName = "3a97660cf97243cd93f3451f7707e7ae20160607092025.jpg";
// 

void parseSystemEmoj(std::string& message)
{
	int pos = 0;
	int iLen = message.size();
	while (	message.find(20) != string::npos ||
		(pos = message.find(-16)) != string::npos && (pos < iLen - 1) &&
		message.at(pos + 1) == -97	)
	{
		string tail;
		if (  (pos = message.find(20)) != string::npos )
		{
			if ( message.length() >= pos + 2)
			{
				tail = message.substr(pos + 2);
			}
			else
			{
				tail = message.substr(pos + 1);
			}
			message = message.substr(0, pos) + StringConvertor::AcsiiToUtf8("[表情]") + tail;
		}
		if ( (pos = message.find(-16)) != string::npos && (pos < iLen - 1) &&
			message.at(pos + 1) == -97 )
		{
			if (pos < iLen - 4)
			{
				tail = message.substr(pos + 4);
			}
			message = message.substr(0, pos) + StringConvertor::AcsiiToUtf8("[表情]") + tail;
		}
	}
}

string MsgBodyParseA(const string& strBody, const string& strKey, int type = 0)
{
	string msg_body = strBody;
	string strValue = "";

	int pos = msg_body.find(strKey);
	if ( pos == string::npos)
	{
		return "";	
	}
	msg_body = msg_body.substr(pos + strKey.length() + 1);

	if (type == 0)
	{
		msg_body = msg_body.substr(3);
	}
	else if (type == 1)
	{
		msg_body = msg_body.substr(1);
	}

	pos = msg_body.find(',');
	if ( pos != string::npos )
	{
		strValue = msg_body.substr(0, pos);
	}
	else if ( (pos = msg_body.find('}')) != string::npos )
	{
		strValue = msg_body.substr(0, pos);
	}

	//去掉引号
    string value;
	for (string::iterator iter = strValue.begin(); iter != strValue.end(); ++iter)
	{
		if ( (*iter) == '\"' || (*iter) == '/')
		{
			continue;
		}
		else
		{
			value += *iter;
		}
	}

	return value;
}

inline void GetMD5(std::string src, std::string &dst)
{
    unsigned char hash[16] = {0};
    MD5_CTX c = {0};
    MD5_Init(&c);
    MD5_Update(&c, (byte*)src.c_str(), src.length());
    MD5_Final(hash, &c);

    char hex[128] = {0};
    for ( int i = 0; i < 16; ++i )
        StringCbPrintfA(hex, sizeof(hex), "%s%.2x", hex, hash[i]);

    dst = hex;
}

inline unsigned short Get16Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned short)(data[1] + (data[0] << 8)) :
    (unsigned short)(data[0] + (data[1] << 8));
}

inline unsigned int Get32Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned int)(data[3] + (data[2] << 8) + (data[1] << 16) + (data[0] << 24)) :
    (unsigned int)(data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24));
}

inline std::string ReadUTF(const char *&data)
{
    std::string str;
    unsigned short dataLen = Get16Bits(true, (const unsigned char*)data);
    data += 2;

    if (dataLen > 0)
    {
        char *buffer = new char[dataLen + 1]();
        if (buffer)
        {
            memcpy(buffer, data, dataLen);
            str = std::string(buffer);
            delete [] buffer;
        }
    }

    data += dataLen;

    return str;
}

// QQ转账
void AndroidQQWalletTransferMsgParse(const char *data, ChatHistory &dst)
{
    if (!data)
    {
        return;
    }

    std::string title;
    std::string subTitle;
    std::string content;
    std::string linkUrl;
    std::string notice;
    std::string walletType = StringConvertor::AcsiiToUtf8("QQ转账");

    // background
    data += 4;

    // icon
    data += 4;

    // title
    title = ReadUTF(data);

    // subTitle
    subTitle = ReadUTF(data);

    // content
    content = ReadUTF(data);

    // linkUrl
    linkUrl = ReadUTF(data);

    // blackStripe
    ReadUTF(data);

    // notice
    notice = ReadUTF(data);

    if (!title.empty() && !subTitle.empty())
    {
        // 当留言是默认的“QQ转账”时不再显示
        if (walletType == content)
        {
            dst.message = walletType +
                StringConvertor::AcsiiToUtf8("\n金额：") +
                title +
                "\n" +
                subTitle;
        }
        else
        {
            dst.message = walletType +
                StringConvertor::AcsiiToUtf8("\n金额：") +
                title +
                "\n" +
                subTitle +
                "\n" +
                content;
        }
    }
}

// QQ红包
void AndroidQQWalletRedPacketMsgParse(const char *data, ChatHistory &dst)
{
    if (!data)
    {
        return;
    }

    std::string title;
    std::string subTitle;
    std::string content;
    std::string linkUrl;
    std::string notice;
    std::string walletType = StringConvertor::AcsiiToUtf8("QQ红包");

    // background
    data += 4;

    // icon
    data += 4;

    // title
    title = ReadUTF(data);

    // subTitle
    subTitle = ReadUTF(data);

    // content
    content = ReadUTF(data);

    // blackStripe
    ReadUTF(data);

    // notice
    notice = ReadUTF(data);

    if (!title.empty() && !subTitle.empty())
    {
        // 当留言是默认的“QQ红包”时不再显示
        if (walletType == content)
        {
            dst.message = walletType +
                StringConvertor::AcsiiToUtf8("\n标题：") +
                title +
                "\n" +
                subTitle;
        }
        else
        {
            dst.message = walletType +
                StringConvertor::AcsiiToUtf8("\n标题：") +
                title +
                "\n" +
                subTitle +
                "\n" +
                content;
        }
    }
}

void AndroidQQWalletMsgParse(const char *data, int dataLen, ChatHistory &dst)
{
    if (!data || 0 >= dataLen)
    {
        return;
    }

    int version = 0;
    int flag = 0;

    // 跳过9字节头部
    data += 9;

    version = (int)Get32Bits(true, (const unsigned char*)data);
    data += 4;

    if (1 == version || 2 == version)
    {
        flag = (int)Get32Bits(true, (const unsigned char*)data);
        data += 4;

        if (1 == flag)
        {
            AndroidQQWalletTransferMsgParse(data, dst);
        }
    }
    else if (17 == version)
    {
        AndroidQQWalletRedPacketMsgParse(data, dst);
    }
    else if (32 <= version)
    {
        flag = (int)Get32Bits(true, (const unsigned char*)data);
        data += 4;

        // readInt2
        data += 4;

        // messageType
        data += 4;

        if (1 == flag)
        {
            AndroidQQWalletTransferMsgParse(data, dst);
        }
        else if (2 == flag)
        {
            AndroidQQWalletRedPacketMsgParse(data, dst);
        }
    }
}

void XorDecrypt(const char* src, int srcLen, const char* key, ChatHistory &dst, bool isNew=false, bool isTroop=false)
{
    int keyLen = strlen(key);
    int magic = 0;
    char buffer[1024 * 10] = {0};
    char buffer2[1024 * 10] = {0};
    bool ret = false;

    do 
    {
        if (!src ||
            srcLen <= 0 ||
            (srcLen > 1024 * 10) ||
            !key)
        {
            break;
        }

        keyLen = strlen(key);
        if (keyLen <= 0)
        {
            break;
        }

        dst.message.clear();

        ret = true;
    } while (false);

    if (!ret)
    {
        return;
    }

	for ( int i = 0, decode = 0; i < srcLen; ++i, ++decode )
	{
		if ( !isNew && (BYTE)src[i] >= 128 )
		{
			char curChar = key[decode%keyLen];

			// 根据分析得知，当遍历到某个单字时，
			// 如果 src 为汉字且 key 为字母，
			// 需要用此种异或方式来解密 :
			// 第一个字节原封不动，第二个字节和 1 异或，
			// 第三个字节和字母转换成的数字异或 ( A - 1， B - 2 ) ...
			if ( curChar >= 'A'
				&& curChar <= 'Z' )
			{
				buffer[i] = src[i];
				buffer[i+1] = src[i+1] ^ 0x01;
				buffer[i+2] = src[i+2] ^ (curChar-'A'+1);
				i += 2;
				continue;
			}

            if ( curChar >= 'a'
                && curChar <= 'z' )
            {
                buffer[i] = src[i];
                buffer[i+1] = src[i+1] ^ 0x01;
                buffer[i+2] = src[i+2] ^ (curChar-'A'+1);
                i += 2;
                continue;
            }

			buffer[i] = src[i];
			buffer[i+1] = src[i+1];
			i += 2;
		}
		buffer[i] = src[i] ^ key[decode%keyLen];
	}

	//parse msgData
	switch (dst.msgType)
	{
	case -1035:
		{
			//电脑端可以发送 混合类型消息
			MsgForMulti multiMsg(buffer, srcLen, !!dst.isSend);
			bool bRead = multiMsg.doParse();
			if (!bRead)
			{
				MultiMsg msg;
				msg.msgType = 0;
				char* szTmp = new char[srcLen + 1]();	
				if (szTmp)
				{	
					int iTmp = 0;
					while (iTmp < srcLen)
					{
						char ch = buffer[iTmp];
						if (ch != '\0')
						{
							szTmp[iTmp] = ch;
						}
						else
						{
							szTmp[iTmp] = ' ';
						}
						iTmp++;
					}
					msg.content = szTmp;
					delete [] szTmp;
				}
				dst.multiMsgLst.push_back(msg);
			}
			else
			{
				if (!multiMsg.msgLst.empty())
				{
					dst.multiMsgLst = multiMsg.msgLst;
					std::list<MultiMsg>::iterator it =  multiMsg.msgLst.begin();
					for (; it != multiMsg.msgLst.end(); it++)
					{
						if (it->msgType == 0)
						{
							string msg = StringConvertor::Utf8ToAscii(it->content);
							msg = "";
						}
						
					}
					
				}
			}
		}
		break;
	//图片
	case QQ_MSG_PIC:
		{
			MsgForPic picMsg(buffer, srcLen, !!dst.isSend);
			picMsg.doParse();
			dst.message = picMsg.strPath;
			int pos = 0;
			while ( (pos = dst.message.find('/')) != string::npos )
			{
				dst.message = dst.message.replace(pos, 1, "\\");
			}
		}
		break;
	case QQ_MSG_VIDEO:
		{
			MsgForVideo videoMsg(buffer, srcLen);
			videoMsg.doParse();
			//视频路径
			dst.message.append("Tencent\\MobileQQ\\shortvideo\\");
			//md5
			dst.message.append(videoMsg.strPath.c_str());
			dst.message.append("\\");

			//视频封面路径
			dst.thumbPath.append("Tencent\\MobileQQ\\shortvideo\\thumbs\\");
			dst.thumbPath.append(videoMsg.strThumbFile.c_str());
			dst.thumbPath.append(".jpg");
		}
		break;
	case QQ_MSG_AUDIO:
		{
			MsgForPtt pttMsg(buffer, srcLen);
			pttMsg.doParse();
			dst.message = pttMsg.strPath;
			int pos = 0;
			while ((pos = dst.message.find('/')) != string::npos)
			{
				dst.message = dst.message.replace(pos, 1, "\\");
			}
		}
		break;
	case -3008:
		{
			dst.msgType = QQ_MSG_FILE;
		}
	case QQ_MSG_FILE:
		{
			//群聊文件
			if ( isTroop )
			{
				char* pTemp = buffer;
				pTemp += 290;
				char* szFileName = pTemp;
				szFileName[strlen(szFileName)-1] = 0;
				dst.message = string("Tencent\\QQfile_recv\\") + szFileName;
			}
			//好友聊天文件
			else
			{
				//截取文件路0x016 + path + 0x7c 0x30 + 0x7c 0x30 + 0x7c + 0x31
				dst.message = buffer;
			}
			int ipos = dst.message.find("|");
			if (std::string::npos == ipos)
			{
				break;
			}
			dst.message = dst.message.substr(1, ipos - 1 );
		}
		break;
    case -2025:
        {
            dst.msgType = QQ_MSG_TXT;
            AndroidQQWalletMsgParse(buffer, srcLen, dst);
        }
        break;
	default:
		{
			char* szTmp = new char[srcLen + 1]();	
			if (szTmp)
			{	
				int iTmp = 0;
				while (iTmp < srcLen)
				{
					char ch = buffer[iTmp];
					if (ch != '\0')
					{
						szTmp[iTmp] = ch;
					}
					else
					{
						szTmp[iTmp] = ' ';
					}
					iTmp++;
				}
				dst.message = szTmp;
				delete [] szTmp;
			}
		}
		break;
	}	
}

void XorDecrypt(const char* src, int srcLen, const char* key, std::string &dst, bool isNew=false)
{
	int keyLen = strlen(key);
    int magic = 0;
    bool ret = false;

    do 
    {
        if (!src ||
            srcLen <= 0 ||
            !key ||
            keyLen <= 0)
        {
            break;
        }

        dst.clear();
        ret = true;
    } while (false);

    if (!ret)
    {
        return;
    }

	char* buffer = new char[srcLen+3]();
	if (!buffer)
	{
		return;
	}

	for ( int i = 0, decode = 0; i < srcLen; ++i, ++decode )
	{
		if ( !isNew && (BYTE)src[i] >= 128 )
		{
			char curChar = key[decode%keyLen];

			// 根据分析得知，当遍历到某个单字时，
			// 如果 src 为汉字且 key 为字母，
			// 需要用此种异或方式来解密 :
			// 第一个字节原封不动，第二个字节和 1 异或，
			// 第三个字节和字母转换成的数字异或 ( A - 1， B - 2 ) ...
			if ( curChar >= 'A'
				&& curChar <= 'Z' )
			{
				buffer[i] = src[i];
				buffer[i+1] = src[i+1] ^ 0x01;
				buffer[i+2] = src[i+2] ^ (curChar-'A'+1);
				i += 2;
				continue;
			}

            if ( curChar >= 'a'
                && curChar <= 'z' )
            {
                buffer[i] = src[i];
                buffer[i+1] = src[i+1] ^ 0x01;
                buffer[i+2] = src[i+2] ^ (curChar-'A'+1);
                i += 2;
                continue;
            }

			buffer[i] = src[i];
			
			if ((BYTE)src[i] != 0xc2 && ((BYTE)src[i] >= 0xe0 && (BYTE)src[i] <= 0xef))
			{
				buffer[i+1] = src[i+1];
				i += 2;
			}
			else
			{
				//
				//中文符号处理类型
				i += 1;
			}
			
		}
		buffer[i] = src[i] ^ key[decode%keyLen];
	}

	//是否存在\0截断情况
	dst = buffer;

	delete [] buffer;
}


bool GetImei(const char *lpszXml, char *lpszKey, std::string &imei)
{
	bool			bRet		= false;
	char			szXmlPath[MAX_FILE_PATH] = {0};
	TiXmlDocument	*qqDocument	= new TiXmlDocument;
    TiXmlElement    *root = NULL;
	TiXmlElement	*imeiEm = NULL;
    TiXmlAttribute  *attr = NULL;
    const char      *value = NULL;
    const char      *text = NULL;
	char			szImei[512]	= {0};
	LPSTR			lpszPointer	= NULL;
	std::string		xmlPath;

    do 
    {
        if (!lpszXml || !lpszKey)
        {
            break;
        }

        if (!qqDocument)
        {
            break;
        }

        if (!PathFileExistsA(lpszXml))
        {
            break;
        }

        if (!qqDocument->LoadFile(lpszXml))
        {
            break;
        }

        root = qqDocument->RootElement();
        if (!root)
        {
            break;
        }

        imeiEm = root->FirstChildElement();
        if (!imeiEm)
        {
            break;
        }

        while(imeiEm)
        {
            attr = NULL;
            value = NULL;
            text = NULL;

            attr = imeiEm->FirstAttribute();
            if (!attr)
            {
                continue;
            }

            value = attr->Value();
            if (!value)
            {
                continue;
            }

            if(0 == lstrcmpA(value, lpszKey))
            {
                text = imeiEm->GetText();
                if (!text)
                {
                    continue;
                }

                strncpy_s(szImei, sizeof(szImei) / sizeof(szImei[0]), text, _TRUNCATE);

                lpszPointer = strrchr(szImei, '|');
                if (lpszPointer)
                {
                    *lpszPointer = '\0';
                    imei = szImei;
                }
                else
                {
                    imei = szImei;
                }

                bRet = true;
                break;
            }

            imeiEm = imeiEm->NextSiblingElement();
        }

    } while (false);

	if (qqDocument)
	{
        delete qqDocument;
    }

	return bRet;
}

bool QQ_InitEntry(LPCWSTR lpwzCasePath, int iType, char *lpszImei, bool isDbPath/*直接是QQ数据库路径*/)
{
    g_qqDirPath.clear();
    g_imei.clear();
    g_imei2.clear();
    _appDirPath.clear();
    _isSync = false;

	g_iType = iType;
	_appDirPath = lpwzCasePath;

    if ( wstring::npos != _appDirPath.find(L"\\App\\Backup") || wstring::npos != _appDirPath.find(L"\\Backup\\Backup"))
    {
        _appDirPath = _appDirPath.substr(0, _appDirPath.find(L"\\Backup")) + _appDirPath.substr(_appDirPath.find(L"\\Backup")+7);
    }

	if (0 == iType)
	{
		return AndroidInitEntry(lpwzCasePath, lpszImei, isDbPath);
	}
	else if (1 == iType)
	{
		return IosInitEntry(lpwzCasePath, lpszImei, isDbPath);
	}
	else
	{
		return false;
	}
}

bool AndroidInitEntry(LPCWSTR lpwzCasePath, char *lpszImei, bool isDbPath)
{
	bool    bRet = false;
	WCHAR	wzQQDir[MAX_FILE_PATH] = {0};
	WCHAR	wzQQFile[MAX_FILE_PATH] = {0};
	std::wstring filepath;
	char	szQQDir[MAX_PATH] = {0};
	std::string			xmlPath;

	do 
	{
        if (!lpwzCasePath)
        {
            break;
        }

        if (!InitialSqlite3Helper())
        {
            break;
        }

        if (isDbPath)
        {
            g_qqDirPath = lpwzCasePath;
            bRet = true;
            break;
        }

        wcsncpy_s(wzQQDir, sizeof(wzQQDir) / sizeof(wzQQDir[0]), lpwzCasePath, _TRUNCATE);
		if (wzQQDir[lstrlenW(wzQQDir) - 1] != L'\\')
		{
            wcsncat_s(wzQQDir, sizeof(wzQQDir) / sizeof(wzQQDir[0]), L"\\", _TRUNCATE);
		}
		
        wcsncat_s(wzQQDir, sizeof(wzQQDir) / sizeof(wzQQDir[0]), L"com.tencent.mobileqq", _TRUNCATE);
        g_qqDirPath = wzQQDir;
		if (PathFileExistsW(wzQQDir))
		{
			bRet = true;
			break;
		}

		//.tar.gz
		//
		filepath = std::wstring(lpwzCasePath) + L"\\com.tencent.mobileqq.tar.gz";
		if (PathFileExistsW(filepath.c_str()))
		{
            bRet = UnzipAndMove(filepath, g_qqDirPath, L"com.tencent.mobileqq", 0);
            if (bRet)
            {
                break;
            }
		}

		//.ab
		//
		filepath = std::wstring(lpwzCasePath) + L"\\com.tencent.mobileqq.ab";
		if (PathFileExistsW(filepath.c_str()))
		{
            bRet = ExtractAbFile(filepath, g_qqDirPath, L"com.tencent.mobileqq");
            if (bRet)
            {
                break;
            }
		}

	} while (false);

	if (bRet)
	{
		WideCharToMultiByte(CP_ACP, 0, g_qqDirPath.c_str(), -1, szQQDir, MAX_FILE_PATH, NULL, NULL);

		xmlPath = std::string(szQQDir) + "\\shared_prefs\\mobileQQ.xml";
		GetImei(xmlPath.c_str(), "security_key", g_imei);

		xmlPath = std::string(szQQDir) + "\\shared_prefs\\DENGTA_META.xml";
		GetImei(xmlPath.c_str(), "QIMEI_DENGTA", g_imei2);
		if (g_imei2.empty())
		{
			xmlPath = std::string(szQQDir) + "\\shared_prefs\\appcenter_mobileinfo.xml";
			GetImei(xmlPath.c_str(), "imei", g_imei2);
		}
	}

	return bRet;
}


bool IosInitEntry(LPCWSTR lpwzCasePath, char *lpszImei, bool isDbPath)
{
	bool    bRet = false;
	std::wstring filepath;		//iphone压缩包
	std::wstring filepathIPad;	//ipad压缩包
	WCHAR	wzQQDir[MAX_FILE_PATH] = {0};	 //iphone路径
	WCHAR	wzQQDirIPad[MAX_FILE_PATH] = {0};//ipad路径
	WCHAR	wzQQFile[MAX_FILE_PATH] = {0};

	do 
	{
        if (!lpwzCasePath)
        {
            break;
        }

        if (!InitialSqlite3Helper())
        {
            break;
        }

        if (isDbPath)
        {
            g_qqDirPath = lpwzCasePath;
            bRet = true;
            break;
        }

        wcsncat_s(wzQQDir, sizeof(wzQQDir) / sizeof(wzQQDir[0]), lpwzCasePath, _TRUNCATE);
		if (wzQQDir[lstrlenW(wzQQDir) - 1] != L'\\')
		{
            wcsncat_s(wzQQDir, sizeof(wzQQDir) / sizeof(wzQQDir[0]), L"\\", _TRUNCATE);
		}
        wcsncat_s(wzQQDir, sizeof(wzQQDir) / sizeof(wzQQDir[0]), L"com.tencent.mqq", _TRUNCATE);

        wcsncat_s(wzQQDirIPad, sizeof(wzQQDirIPad) / sizeof(wzQQDirIPad[0]), lpwzCasePath, _TRUNCATE);
		if (wzQQDirIPad[lstrlenW(wzQQDirIPad) - 1] != L'\\')
		{
            wcsncat_s(wzQQDirIPad, sizeof(wzQQDirIPad) / sizeof(wzQQDirIPad[0]), L"\\", _TRUNCATE);
		}
        wcsncat_s(wzQQDirIPad, sizeof(wzQQDirIPad) / sizeof(wzQQDirIPad[0]), L"com.tencent.mipadqq", _TRUNCATE);

		if (PathFileExistsW(wzQQDir))
		{
			g_qqDirPath = wzQQDir;
			bRet = true;
			break;
		}
		else if ( PathFileExistsW(wzQQDirIPad) )
		{
			g_qqDirPath = wzQQDirIPad;
			bRet = true;
			break;
		}

		//.zip
		//
		filepath = std::wstring(lpwzCasePath) + L"\\com.tencent.mqq.zip";
		filepathIPad = std::wstring(lpwzCasePath) + L"\\com.tencent.mipadqq.zip";
		if (PathFileExistsW(filepath.c_str()))
		{
			g_qqDirPath = wzQQDir;
			bRet = UnzipAndMove(filepath, g_qqDirPath, L"com.tencent.mobileqq", 1);
			if (bRet)
			{
				break;
			}
		}
		else if ( PathFileExistsW(filepathIPad.c_str()) )
		{
			g_qqDirPath = wzQQDirIPad;
			bRet = UnzipAndMove(filepathIPad, g_qqDirPath, L"com.tencent.mobileqq", 1);
			if (bRet)
			{
				break;
			}
		}

	} while (false);

	return bRet;
}

void QQ_Free()
{
	if (g_accountInfo.size() > 0)
	{
		std::map<std::string, AccountInfo>::iterator accountItera;
		for (accountItera = g_accountInfo.begin(); accountItera != g_accountInfo.end(); accountItera++)
		{
			accountItera->second.friendList.clear();
			accountItera->second.groupList.clear();
			accountItera->second.troopList.clear();
			accountItera->second.discussList.clear();

			std::map<std::string,ChatInfo>::iterator chatItera;
			if (accountItera->second.chatMap.size() > 0)
			{
				for (chatItera = accountItera->second.chatMap.begin(); chatItera != accountItera->second.chatMap.end(); chatItera++)
				{
					chatItera->second.chatList.clear();
				}
				accountItera->second.chatMap.clear();
			}	
		}
		g_accountInfo.clear();
	}

    g_accountInfoEx.clear();
}

bool QQ_GetAccountList(char *lpszAccounts, int iSize)
{
	if (g_iType == 0)
	{
		return AndroidGetAccountList(lpszAccounts, iSize);
	}
	else if (1 == g_iType)
	{
		return IosGetAccountList(lpszAccounts, iSize);
	}
	else
	{
		return false;
	}
}

bool AndroidGetAccountList(char *lpszAccounts, int iSize)
{
	wstring syncPath = _appDirPath + L"\\AppSyncDir\\SyncDbFile\\qq\\isSync.txt";
	_isSync = PathFileExistsW(syncPath.c_str()) ? true :false;

	std::string			account;
	std::string			imei;
	std::string			xmlPath;
	std::wstring		findStr; 

	if (_isSync)
	{
		findStr = _appDirPath + L"\\AppSyncDir\\SyncDbFile\\qq\\*.db";

        //删除isSync.txt
        DeleteFileW(syncPath.c_str());
	}
	else
	{
		wstring pathFir = g_qqDirPath + L"\\databases";
		wstring pathSec = g_qqDirPath + L"\\db";
		if (PathFileExistsW(pathFir.c_str()))
		{
			findStr = pathFir + L"\\*.db";
		}
		else if (PathFileExistsW(pathSec.c_str()))
		{
			findStr = pathSec + L"\\*.db";
		}
	}
		
	HANDLE				hFind;
	WIN32_FIND_DATAW	w32Find = {0};
	char				szNumber[20+1] = {0};
	char				*lpszPoinetr;
	char				szQQDir[MAX_PATH+1] = {0};

	WideCharToMultiByte(CP_ACP, 0, g_qqDirPath.c_str(), -1, szQQDir, MAX_PATH, NULL, NULL);

	hFind = FindFirstFileW(findStr.c_str(), &w32Find);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		return false;
	}

	do 
	{
		if (w32Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			continue;
		}
		if (w32Find.cFileName[0] > L'0' && w32Find.cFileName[0] <= L'9')
		{
			if (0 != WideCharToMultiByte(CP_UTF8, 0, w32Find.cFileName, -1, szNumber, 20, NULL, NULL))
			{
				lpszPoinetr = StrRChrA(szNumber, NULL, '.');
				*lpszPoinetr = '\0';
				if (!account.empty())
				{
					account += "|";
				}
				account += szNumber;
				g_accountInfo[std::string(szNumber)].account = szNumber;

				if (_isSync)
				{
					g_accountInfo[std::string(szNumber)].dbPath = _appDirPath + L"\\AppSyncDir\\SyncDbFile\\qq\\" +  w32Find.cFileName;
				}
				else
				{
					wstring pathFir = g_qqDirPath + L"\\databases\\" + w32Find.cFileName;
					wstring pathSec = g_qqDirPath + L"\\db\\" + w32Find.cFileName;
					if (PathFileExistsW(pathFir.c_str()))
					{
						g_accountInfo[std::string(szNumber)].dbPath = pathFir;
					}
					else if (PathFileExistsW(pathSec.c_str()))
					{
						g_accountInfo[std::string(szNumber)].dbPath = pathSec;
					}
				}

				imei.clear();
				if (!g_imei.empty())
				{
					imei = g_imei;
				}
				else
				{
					xmlPath = std::string(szQQDir) + "\\shared_prefs\\contact_bind_info" + std::string(szNumber) + ".xml";
					GetImei(xmlPath.c_str(), "contact_bind_info_unique", imei);
					if (imei.empty())
					{
						GetImei(xmlPath.c_str(), "contact_bind_info_imei", imei);
					}
					if (imei.empty())
					{
						imei = g_imei2;
					}
				}
				g_accountInfo[std::string(szNumber)].key = imei;
				//GetImei(std::string(szNumber), g_accountInfo[std::string(szNumber)].key);
			}	
		}
	} while (FindNextFileW(hFind, &w32Find));

	FindClose(hFind);
	if (account.empty())
	{
		return false;
	}
	else
	{
		int iLen = account.length() > iSize-1 ? iSize-1 : account.length();
		account.copy(lpszAccounts, iLen);
		return true;
	}
}

bool IosGetAccountList(char *lpszAccounts, int iSize)
{
	std::string			account;
	std::wstring		findStr =  g_qqDirPath + L"\\Documents\\contents\\*";
	HANDLE				hFind;
	WIN32_FIND_DATAW	w32Find = {0};
	char				szNumber[20 + 1] = {0};

	bool bOld = false;
	hFind = FindFirstFileW(findStr.c_str(), &w32Find);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		//return false;

		findStr = g_qqDirPath + L"\\Library\\iPadQQData\\contents\\*";
		hFind = FindFirstFileW(findStr.c_str(), &w32Find);
		if (INVALID_HANDLE_VALUE == hFind)
		{
			return false;
		}
		bOld = true;
	}

	do 
	{
		if (0 == (w32Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			continue;
		}

		if (0 == lstrcmpiW(w32Find.cFileName, L".")
			|| 0 == lstrcmpiW(w32Find.cFileName, L".."))
		{
			continue;
		}

		if (w32Find.cFileName[0] > L'0' 
			&& w32Find.cFileName[0] <= L'9'
			&& lstrlenW(w32Find.cFileName) > 5)
		{
			if (0 != WideCharToMultiByte(CP_UTF8, 0, w32Find.cFileName, -1, szNumber, 20, NULL, NULL))
			{
				if (!account.empty())
				{
					account += "|";
				}
				account += szNumber;
				g_accountInfo[std::string(szNumber)].account = szNumber;
                
                g_accountInfoEx[std::string(szNumber)].account = szNumber;
                if (!bOld)
                {
                    std::wstring plistFile = g_qqDirPath + L"\\Documents\\contents\\" + w32Find.cFileName + L"\\profile\\" + w32Find.cFileName + L".plist";
                    IosGetAccountInfoEx(plistFile, g_accountInfoEx[std::string(szNumber)]);
                }

				if (!bOld)
				{
					g_accountInfo[std::string(szNumber)].dbPath  = g_qqDirPath + L"\\Documents\\contents\\" + w32Find.cFileName + L"\\QQ.db";
				}
				else
				{
					g_accountInfo[std::string(szNumber)].dbPath  = g_qqDirPath + L"\\Library\\iPadQQData\\contents\\" + w32Find.cFileName + L"\\QQ.db";
				}
				
			}	
		}
	} while (FindNextFileW(hFind, &w32Find));

	FindClose(hFind);
	if (account.empty())
	{
		return false;
	}
	else
	{
		int iLen = account.length() > iSize-1 ? iSize-1 : account.length();
		account.copy(lpszAccounts, iLen);
		return true;
	}
}

bool QQ_GetAccoutInfo(const std::string &account)
{
	if (0 == g_iType)
	{
		return AndroidGetAccountInfo(account);
	}
	else if (1 == g_iType)
	{
		return IosGetAccountInfo(account);
	}
	else
	{
		return false;
	}
}

bool AndroidGetAccountInfo(const std::string &account)
{
	sqlite3		*db = NULL;
	int			iRet;
	bool		bRet = false;

	if (!PathFileExistsW(g_accountInfo[account].dbPath.c_str()))
	{
		return bRet;
	}

	iRet = Sqlite3Helper.sqlite3_open16(g_accountInfo[account].dbPath.c_str(), &db);
	if (SQLITE_OK != iRet)
	{
		return bRet;
	}

	//get friend info
	//
	AndroidGetFriendInfo(db, account);

	//get group info
	//
	AndroidGetGroupInfo(db, account);

	//get troop info
	//
	AndroidGetTroopInfo(db, account);

	//get discussion info
	//
	AndroidGetDiscInfo(db, account);

	//get chat history
	//
	AndroidGetChatHistory(db, account);

	Sqlite3Helper.sqlite3_close(db);
	return true;
}

void AndroidGetFriendInfo(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	int				iRet = -1;
	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	sqlite3_stmt	*stmt = NULL;
    char	szSql1[] = "SELECT uin, name, remark, richBuffer, Friends.groupid, group_name FROM Friends, Groups WHERE Friends.groupid = Groups.group_id";
    char	szSql2[] = "SELECT uin, name, Friends.groupid, group_name FROM Friends, Groups WHERE Friends.groupid = Groups.group_id";
    std::vector<std::string> friendTableColumns;
    char sql[] = "PRAGMA table_info(\"Friends\")";
    bool isEncrypt = false;

    iRet = Sqlite3Helper.sqlite3_prepare(db, sql, lstrlenA(sql), &stmt, NULL);
    if (SQLITE_OK != iRet || !stmt)
    {
        return;
    }

    while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
    {
        const char *lpPointer = NULL;
        int  iSize = 0;

        iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
        lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
        if ( lpPointer != NULL )
        {
            friendTableColumns.push_back(lpPointer);
        }
    }

    Sqlite3Helper.sqlite3_finalize(stmt);

    for (std::size_t i = 0, j = 0; i < friendTableColumns.size(); ++i)
    {
        if ("remark" == friendTableColumns[i] ||
            "richBuffer" == friendTableColumns[i])
        {
            ++j;
        }

        if (2 == j)
        {
            isEncrypt = true;
            break;
        }
    }

    if (isEncrypt)
    {
        iRet = Sqlite3Helper.sqlite3_prepare(db, szSql1, lstrlenA(szSql1), &stmt, NULL);
        if (SQLITE_OK != iRet || !stmt)
        {
            return;
        }

        while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
        {
            const char *lpPointer = NULL;
            int  iSize = 0;
            FriendInfo		friendInfo;

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.account);
            }

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.nickName);
            }

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 2);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 2);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.remark);
            }

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 3);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 3);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.signature, true);
            }
            if (friendInfo.signature.size() > 2)
            {
                friendInfo.signature.erase(0,2);
            }

            friendInfo.groupId = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 4);

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 5);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 5);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.groupName);
            }

            pAccountInfo->friendList.push_back(friendInfo);
        }

        // 有的好友分组ID为-1，为防止遗漏此处也要处理
        Sqlite3Helper.sqlite3_finalize(stmt);
        std::string sql = "SELECT uin, name, remark, richBuffer FROM Friends where groupid = -1";
        iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.size(), &stmt, NULL);
        if (SQLITE_OK != iRet || !stmt)
        {
            return;
        }

        while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
        {
            const char *lpPointer = NULL;
            int  iSize = 0;
            FriendInfo		friendInfo;

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.account);
            }

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.nickName);
            }

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 2);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 2);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.remark);
            }

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 3);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 3);
            if ( lpPointer != NULL )
            {
                XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), friendInfo.signature, true);
            }
            if (friendInfo.signature.size() > 2)
            {
                friendInfo.signature.erase(0,2);
            }

            friendInfo.groupId = -1;
            friendInfo.groupName = StringConvertor::UnicodeToUtf8(L"无分组");

            pAccountInfo->friendList.push_back(friendInfo);
        }
    }
    else
    {
        iRet = Sqlite3Helper.sqlite3_prepare(db, szSql2, lstrlenA(szSql2), &stmt, NULL);
        if (SQLITE_OK != iRet || !stmt)
        {
            return;
        }

        while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
        {
            const char *lpPointer = NULL;
            int  iSize = 0;
            FriendInfo		friendInfo;

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
            if ( lpPointer != NULL )
            {
                friendInfo.account = lpPointer;
            }

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
            if ( lpPointer != NULL )
            {
                friendInfo.nickName = lpPointer;
            }

            friendInfo.groupId = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 2);

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 3);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 3);
            if ( lpPointer != NULL )
            {
                friendInfo.groupName = lpPointer;
            }

            pAccountInfo->friendList.push_back(friendInfo);
        }

        // 有的好友分组ID为-1，为防止遗漏此处也要处理
        Sqlite3Helper.sqlite3_finalize(stmt);
        std::string sql = "SELECT uin, name, remark, richBuffer FROM Friends where groupid = -1";
        iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.size(), &stmt, NULL);
        if (SQLITE_OK != iRet || !stmt)
        {
            return;
        }

        while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
        {
            const char *lpPointer = NULL;
            int  iSize = 0;
            FriendInfo		friendInfo;

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
            if ( lpPointer != NULL )
            {
                friendInfo.account = lpPointer;
            }

            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
            if ( lpPointer != NULL )
            {
                friendInfo.nickName = lpPointer;
            }

            friendInfo.groupId = -1;
            friendInfo.groupName = StringConvertor::UnicodeToUtf8(L"无分组");

            pAccountInfo->friendList.push_back(friendInfo);
        }
    }

	Sqlite3Helper.sqlite3_finalize(stmt);
}

void AndroidGetGroupInfo(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	int				iRet = -1;
	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	sqlite3_stmt	*stmt = NULL;
	char	szSql[] = "SELECT group_id,group_name,group_friend_count FROM Groups";

	iRet = Sqlite3Helper.sqlite3_prepare(db, szSql, lstrlenA(szSql), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		GroupInfo	groupInfo;
		const char *lpPointer = NULL;
		int  iSize = 0;

		groupInfo.groupId = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 0);

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
		if ( lpPointer != NULL )
		{
			XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), groupInfo.groupName);
		}

		groupInfo.memberCount = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 2);

		pAccountInfo->groupList.push_back(groupInfo);
	}

	Sqlite3Helper.sqlite3_finalize(stmt);
}

void AndroidGetTroopInfo(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	int				iRet = -1;
	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	sqlite3_stmt	*stmt = NULL;
	char	szSql[] = "SELECT troopuin, troopname, troopmemo FROM TroopInfo";
    std::string sql = "SELECT troopuin, troopname, troopmemo FROM TroopInfoV2";

	iRet = Sqlite3Helper.sqlite3_prepare(db, szSql, lstrlenA(szSql), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
        iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), -1, &stmt, NULL);
        if (SQLITE_OK != iRet || !stmt)
        {
            return;
        }
	}

	while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		TroopInfo	troopInfo;
		const char *lpPointer = NULL;
		int  iSize = 0;

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
		if ( lpPointer != NULL )
		{
			XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), troopInfo.troopUin);
		}

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
		if ( lpPointer != NULL )
		{
			XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), troopInfo.troopName);
		}

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 2);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 2);
		if ( lpPointer != NULL )
		{
			XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), troopInfo.troopMemo);
		}

		pAccountInfo->troopList.push_back(troopInfo);
	}

	Sqlite3Helper.sqlite3_finalize(stmt);
}

void AndroidGetDiscInfo(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	int				iRet = -1;
	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	sqlite3_stmt	*stmt = NULL;
	char	szSql[] = "SELECT uin, discussionName, datetime(createtime,'unixepoch','localtime') FROM DiscussionInfo";

	iRet = Sqlite3Helper.sqlite3_prepare(db, szSql, lstrlenA(szSql), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		DiscInfo	discInfo;
		const char *lpPointer = NULL;
		int  iSize = 0;

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
		if ( lpPointer != NULL )
		{
			XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), discInfo.discussUin);
		}

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
		if ( lpPointer != NULL )
		{
			XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), discInfo.discussName);
		}

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 2);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 2);
		if ( lpPointer != NULL )
		{
            discInfo.createTime = string(lpPointer);
		}

		pAccountInfo->discussList.push_back(discInfo);
	}

	Sqlite3Helper.sqlite3_finalize(stmt);
}

void AndroidGetChatHistory(sqlite3 *db, const std::string &account)
{
	std::string chatAccount;
	AccountInfo *pAccount = &g_accountInfo[account];

    if (!db || !pAccount)
    {
        return;
    }

	//get friend chat history
	//
	if (pAccount->friendList.size() > 0)
	{
		std::list<FriendInfo>::iterator friendItera;
		for (friendItera = pAccount->friendList.begin(); friendItera != pAccount->friendList.end(); friendItera++)
		{
			chatAccount = friendItera->account;
			AndroidGetSingleChatHistory(db, account, chatAccount, friendItera->remark, 0);
		}
	}
	

	//get troop chat history
	//
	if (pAccount->troopList.size() > 0)
	{
		std::list<TroopInfo>::iterator troopItera;
		for (troopItera = pAccount->troopList.begin(); troopItera != pAccount->troopList.end(); troopItera++)
		{
			chatAccount = troopItera->troopUin;
			AndroidGetSingleChatHistory(db, account, chatAccount, "", 1);
		}
	}
	

	//get discussion chat history
	//
	if (pAccount->discussList.size() > 0)
	{
		std::list<DiscInfo>::iterator discItera;
		for (discItera = pAccount->discussList.begin(); discItera != pAccount->discussList.end(); discItera++)
		{
			chatAccount = discItera->discussUin;
			AndroidGetSingleChatHistory(db, account, chatAccount, "", 2);
		}
	}
	
}

void AndroidGetSingleChatHistory(sqlite3 *db,
                                 const std::string &account,
                                 const std::string &chatAccount,
                                 const std::string &chatAccountRemark,
                                 int iType)
{
	std::string		hash;
	std::string     tableName;
	std::string     partName;
    std::string		sql;
    std::string		sql2;
    sqlite3_stmt	*stmt2 = NULL;

	int				iRet = -1;
	sqlite3_stmt	*stmt = NULL;
	bool	isTroop = false;
    bool    isNewDiscuss = false;

	GetMD5(chatAccount, hash);

	//check if table exists
	//
	switch (iType)
	{
	case  0:
		partName = "friend";
		break;
	case 1:
		partName = "troop";
		break;
	case 2:
		partName = "discussion";
		break;
	}
	tableName = "mr_" + partName + "_" + hash + "_New";
	sql = "SELECT * FROM sqlite_master WHERE type=\"table\" AND name = \"" + tableName + "\"";

	iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	iRet = Sqlite3Helper.sqlite3_step(stmt);
	if (SQLITE_ROW != iRet)
	{
		Sqlite3Helper.sqlite3_finalize(stmt);

        // 讨论组存在两种格式的表名
        if (2 == iType)
        {
            partName = "discusssion";
            tableName = "mr_" + partName + "_" + hash + "_New";
            sql = "SELECT * FROM sqlite_master WHERE type=\"table\" AND name = \"" + tableName + "\"";

            iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
            if (SQLITE_OK != iRet || !stmt)
            {
                return;
            }

            isNewDiscuss = true;
            iRet = Sqlite3Helper.sqlite3_step(stmt);
            if (SQLITE_ROW != iRet)
            {
                Sqlite3Helper.sqlite3_finalize(stmt);
                return;
            }
        }
        else
        {
            return;
        }
	}

	Sqlite3Helper.sqlite3_finalize(stmt);
    stmt = NULL;

	//
	//get account chat history
	ChatInfo		chatInfo;

	if (!_isSync)
	{
		switch(iType)
		{
		case 0:
			sql = "SELECT msgData, datetime(time,'unixepoch','localtime'), isSend, msgType, senderuin, uniseq, msgUid, hex(senderuin) From " + tableName + " ORDER BY time";
			break;
		case 1:
			sql = "SELECT msgData, datetime(time,'unixepoch','localtime'), isSend, msgType, senderuin, uniseq, msgUid, hex(senderuin) From " + tableName + " ORDER BY time";
			break;
		case 2:
            {
                if (isNewDiscuss)
                {
                    sql = "SELECT msgData, datetime(time,'unixepoch','localtime'), isSend, msgType, selfuin, uniseq, msgUid, hex(senderuin) From " + tableName + " ORDER BY time";
                }
                else
                {
                    sql = "SELECT msgData, datetime(time,'unixepoch','localtime'), isSend, msgType, senderuin, uniseq, msgUid, hex(senderuin) From " + tableName + " ORDER BY time";
                }
                break;
            }
		}
	}
	else
	{
		switch(iType)
		{
		case 0:
            sql = "SELECT msgData, datetime(time,'unixepoch','localtime'), isSend, msgType, senderuin, uniseq, msgUid, markType, hex(senderuin) From " + tableName + " ORDER BY time";
			break;
		case 1:
            sql = "SELECT msgData, datetime(time,'unixepoch','localtime'), isSend, msgType, senderuin, uniseq, msgUid, markType, hex(senderuin) From " + tableName + " ORDER BY time";
			break;
		case 2:
            {
                if (isNewDiscuss)
                {
                    sql = "SELECT msgData, datetime(time,'unixepoch','localtime'), isSend, msgType, selfuin, uniseq, msgUid, markType, hex(senderuin) From " + tableName + " ORDER BY time";
                }
                else
                {
                    sql = "SELECT msgData, datetime(time,'unixepoch','localtime'), isSend, msgType, senderuin, uniseq, msgUid, markType, hex(senderuin) From " + tableName + " ORDER BY time";
                }
                break;
            }
		}
	}

	iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	__int64 iPrevMsgUid = -1;
	while(SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
        ChatHistory	chatHistory;
		const char *lpPointer = NULL;
		int  iSize = 0;

		//msgUid
		//issend
		__int64 iMsgUid = (__int64)Sqlite3Helper.sqlite3_column_int64(stmt, 6);

		bool bOneMsg = ((iPrevMsgUid == iMsgUid) && (iMsgUid != 0));
		iPrevMsgUid = iMsgUid;
		if (!bOneMsg)
		{
            //time
            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
            if ( lpPointer != NULL )
            {
                chatHistory.time = string(lpPointer);
            }

			//issend
			chatHistory.isSend = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 2);
			if (chatHistory.isSend)
				chatHistory.isSend = 1;

			//msg type
			chatHistory.msgType = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 3);
			if ( chatHistory.msgType == -2017 )
			{
				chatHistory.msgType = QQ_MSG_FILE;
				isTroop = true;
			}
			if ( chatHistory.msgType == -2006 )
			{
				chatHistory.msgType = QQ_MSG_FILE;
			}

			//sender uin
			iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 4);
			lpPointer = (const char*)Sqlite3Helper.sqlite3_column_blob(stmt, 4);
			if ( lpPointer != NULL )
			{
				XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), chatHistory.senderUin);
			}

			//sender name
            if (!chatHistory.isSend)
            {
                chatHistory.senderName = chatAccountRemark;
            }
            else
            {
                if (g_accountInfoEx.end() != g_accountInfoEx.find(account))
                {
                    chatHistory.senderName = g_accountInfoEx[account].nickName;
                }
            }

            if (chatHistory.senderName.empty())
            {
                iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, _isSync ? 8 : 7);
                lpPointer = (const char*)Sqlite3Helper.sqlite3_column_blob(stmt, _isSync ? 8 : 7);
                std::string chatUin = lpPointer != NULL ? string(lpPointer) : "";
                std::string chatName;

                switch (iType)
                {
                case 0:
                    sql2 = "select name from Friends where hex(uin) = '" + chatUin + "'";
                    break;
                case 1:
                    sql2 = "select troopnick from TroopMemberInfo where hex(memberuin) = '" + chatUin + "'";
                    break;
                case 2:
                    sql2 = "select inteRemark from DiscussionMemberInfo where hex(memberuin) = '" + chatUin + "'";
                    break;
                default:
                    break;
                }

                if (SQLITE_OK == Sqlite3Helper.sqlite3_prepare(db, sql2.c_str(), sql2.length(), &stmt2, NULL))
                {
                    while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt2))
                    {
                        chatName.clear();
                        int iSize = Sqlite3Helper.sqlite3_column_bytes(stmt2, 0);
                        const char *lpPointer = (const char*)Sqlite3Helper.sqlite3_column_blob(stmt2, 0);
                        if ( lpPointer != NULL )
                        {
                            XorDecrypt(lpPointer, iSize, g_accountInfo[account].key.c_str(), chatName);
                        }

                        if (!chatName.empty())
                        {
                            break;
                        }
                    }

                    Sqlite3Helper.sqlite3_finalize(stmt2);
                    stmt2 = NULL;
                }

                if (chatHistory.senderName.empty())
                {
                    chatHistory.senderName = chatName;
                }

                if (g_accountInfoEx.end() == g_accountInfoEx.find(account))
                {
                    if (chatHistory.isSend)
                    {
                        g_accountInfoEx[account].nickName = chatHistory.senderName;
                    }
                }
            }
		}

		//message
		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		//直接传iSize不存在截断问题
		lpPointer = (const char*)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
		if ( lpPointer != NULL )
		{
			XorDecrypt(lpPointer,iSize, g_accountInfo[account].key.c_str(), chatHistory, true, isTroop);
		}

		//系统表情
		if (chatHistory.msgType == -1035)
		{
			std::list<MultiMsg>::iterator msgIt = chatHistory.multiMsgLst.begin();
			for (; msgIt != chatHistory.multiMsgLst.end(); msgIt++)
			{
				if (msgIt->msgType == 0)
				{
					//文本消息中 系统表情处理
					parseSystemEmoj(msgIt->content);
				}
			}
		}
		parseSystemEmoj(chatHistory.message);


		//unisep
		if (chatHistory.msgType == QQ_MSG_VIDEO)
		{
			//md5+frienduin+uniseq
			long long llUniseq = Sqlite3Helper.sqlite3_column_int64(stmt, 5);

			if (chatHistory.isSend)
			{
				chatHistory.message.append(chatAccount.c_str());
			}
			else
			{
				chatHistory.message.append(account.c_str());
			}

			char szUniseq[100] = {0};
            StringCbPrintfA(szUniseq, sizeof(szUniseq), "%lld", llUniseq);
			//sprintf(szUniseq, "%lld", llUniseq);
			chatHistory.message.append(szUniseq);
			chatHistory.message.append(".mp4");
		}

		//add by renj 20160506
		chatHistory.filePath = "";
		if (chatHistory.msgType == QQ_MSG_PIC || chatHistory.msgType == QQ_MSG_AUDIO || chatHistory.msgType == QQ_MSG_FILE || chatHistory.msgType == QQ_MSG_VIDEO)
		{
			chatHistory.filePath = chatHistory.message;
		}
		
		chatHistory.markType = _isSync ? (int)Sqlite3Helper.sqlite3_column_int64(stmt, 7) : 0;

		int pos = 0;
		if ( QQ_MSG_CALL == chatHistory.msgType || QQ_START_CALL == chatHistory.msgType)
		{
			pos = chatHistory.message.find("|");
			if ( pos != string::npos )
			{
				chatHistory.message = chatHistory.message.substr(0, pos);
			}
		}
        else if (QQ_SIGNATURE == chatHistory.msgType)
        {
            /************************************************************************/
            /* 签名 格式：
            *{"ver":"1.0","time":"1484045267","loctextpos":"0","plaintext":["11533"],"feedid":"929304761d3bb7458d02a0d00d0","tplid":0,"count":0,"zanfalg":0}
            /************************************************************************/
            pos = chatHistory.message.find("plaintext");
            if (pos != string::npos)
            {
                pos += 11;
                string messageInfo = chatHistory.message.substr(pos, chatHistory.message.find(",", pos));
                int messagePos = messageInfo.find("\"");
                int messageEndPos = messageInfo.find("\"", messagePos+1);
                if ((messagePos != string::npos) && (messageEndPos != string::npos))
                {
                    chatHistory.message = chatHistory.senderName + StringConvertor::AcsiiToUtf8("更新了签名\n") + messageInfo.substr(messagePos+1, messageEndPos-messagePos-1);
                } 
                else
                {
                    chatHistory.message = chatHistory.senderName + StringConvertor::AcsiiToUtf8("更新了签名\n") + messageInfo;
                }
            }
        }
        else if (QQ_POKE == chatHistory.msgType)
        {
            /************************************************************************/
            /* 戳了一下 格式：
            *{"msg":"浣犳埑浜嗗彾涓夊瞾(鈼?鈼?鈼?涓€涓?,"isPlayed":true}
            /************************************************************************/
            std::wstring msg2 = StringConvertor::Utf8ToUnicode(chatHistory.message);
            pos = chatHistory.message.find("msg");
            if (pos != string::npos)
            {
                pos += 6;
                chatHistory.message = chatHistory.message.substr(pos, chatHistory.message.find("\"", pos)-pos);
            }
        }
        else if (QQ_MSG_MAP == chatHistory.msgType)
        {
            int messageEndPos = -1;
            int pos0 = chatHistory.message.find("lat=");
            if (pos0 != string::npos)
            {
                pos0 += 4;
                messageEndPos = chatHistory.message.find("&", pos0);
                if (messageEndPos != -1)
                {
                    chatHistory.latitude = chatHistory.message.substr(pos0, messageEndPos-pos0);
                }
                else
                {
                    chatHistory.latitude = chatHistory.message.substr(pos0);
                }
            }
            int pos1 = chatHistory.message.find("lon=");
            if (pos1 != string::npos)
            {
                pos1 += 4;
                messageEndPos = chatHistory.message.find("&", pos1);
                if (messageEndPos != -1)
                {
                    chatHistory.longitude = chatHistory.message.substr(pos1, messageEndPos-pos1);
                }
                else
                {
                    chatHistory.longitude = chatHistory.message.substr(pos1);
                }
            }
            int pos2 = chatHistory.message.find("loc=");
            if (pos2 != string::npos)
            {
                pos2 += 4;
                messageEndPos = chatHistory.message.find("&", pos2);
                if (messageEndPos != -1)
                {
                    chatHistory.location = chatHistory.message.substr(pos2, messageEndPos-pos2);
                }
                else
                {
                    chatHistory.location = chatHistory.message.substr(pos2);
                }
            }

            //群里 有道云笔记分享、红包等也被解析成地图信息了
            if ((pos0 == -1) && (pos1 == -1) && (pos2 == -1))
            {
                chatHistory.msgType = QQ_MSG_OTHER;
            }
        }
        
        if ((QQ_RCMD_CONTACT == chatHistory.msgType) || (QQ_MSG_OTHER == chatHistory.msgType)
            ||(QQ_SIGNIN == chatHistory.msgType) || (QQ_TOPIC == chatHistory.msgType))
        {
            bool        bEnter = true;
            std::wstring msgWorkedUnicode = L"";
            std::wstring msgUnicode = StringConvertor::Utf8ToUnicode(chatHistory.message);

            //红包消息 //xxx成功领取了x元红包 剩x元
            int iIndex = 0;
            int iMsgEnd = msgUnicode.length();
            bool bShowChineseOnly = false;
            /*if (msgUnicode.find(L"[QQ红包]") != std::wstring::npos)
            {
                msgWorkedUnicode = L"【QQ红包】";
                iIndex += msgUnicode.find(L"[QQ红包]")+6;
                bShowChineseOnly = true;
            }
            else if (msgUnicode.find(L"[转账]") != std::wstring::npos)
            {
                msgWorkedUnicode = L"【QQ转账】";
                if (msgUnicode.find(L"元") != std::wstring::npos)
                {
                    iMsgEnd = msgUnicode.find(L"元")+1;
                }
                bShowChineseOnly = true;
            }
            else */if (msgUnicode.find(L"推荐好友") != std::wstring::npos)
            {
                msgWorkedUnicode = L"【推荐好友】";
                iIndex += msgUnicode.find(L"推荐好友") + 4;
                bShowChineseOnly = true;
            }
//             else if (msgTemp.find("[分享]") != std::string::npos)
//             {
//                 msgWorked = "【分享】";
//                 iIndex += msgTemp.find("分享") + 6;
//             }

            if (bShowChineseOnly)
            {
                while(iIndex < iMsgEnd)
                {
                    if (!bEnter && (msgUnicode.find(L"  ", iIndex) == iIndex))
                    {
                        bEnter = true;
                        msgWorkedUnicode.append(L"\n");
                    }

                    if (((msgUnicode.at(iIndex) >= 0x4E00) && (msgUnicode.at(iIndex) <= 0x9FA5)) || !bEnter)
                    {
                        msgWorkedUnicode.push_back(msgUnicode.at(iIndex));
                        bEnter = false;
                    }
                    ++iIndex;
                }
                if (msgWorkedUnicode.at(msgWorkedUnicode.length()-1) == L'\n')
                {
                    msgWorkedUnicode[msgWorkedUnicode.length()-1] = L'\0';
                }
            }
            else
            {
                
                while(iIndex < iMsgEnd)
                {
                    if (((msgUnicode.at(iIndex) > 31) && (msgUnicode.at(iIndex) < 127)) 
                        ||((msgUnicode.at(iIndex) >= 0x4E00) && (msgUnicode.at(iIndex) <= 0x9FA5)))
                    {
                        msgWorkedUnicode.push_back(msgUnicode.at(iIndex));
                    }
                    ++iIndex;
                }
            }
            chatHistory.message = StringConvertor::UnicodeToUtf8(msgWorkedUnicode);
        }

		if (bOneMsg)
		{
			
			std::list<ChatHistory>::reverse_iterator it = (chatInfo.chatList).rbegin();
			if (it != chatInfo.chatList.rend())
			{
				if (chatHistory.msgType == -1035)
				{
					std::list<MultiMsg>::iterator msgIt = chatHistory.multiMsgLst.begin();
					for (; msgIt != chatHistory.multiMsgLst.end(); msgIt++)
					{
						MultiMsg msg = *msgIt;
						it->multiMsgLst.push_back(msg);
					}
				}
				else
				{
					//同一条消息拆分了再次拼接
					it->message += chatHistory.message;
				}
				
			}
			
		}
		else
		{
			chatInfo.chatList.push_back(chatHistory);
		}
		
	}

	g_accountInfo[account].chatMap[chatAccount + "-" + partName] = chatInfo;
	Sqlite3Helper.sqlite3_finalize(stmt);
	return;
}

void IosGetAccountInfoEx(const std::wstring &plistFile, AccountInfoEx &accountInfo)
{
    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    char *plistData = NULL;
    LARGE_INTEGER plistDataSize = {0};
    DWORD readBytes = 0;
    plist_t plist = NULL;
    plist_t plistNode = NULL;
    plist_t plistNode2 = NULL;
    plist_dict_iter iter = NULL;
    int plistArraySize = 0;
    char *stringValue = NULL;
    int index = 0;

    do 
    {
        if (-1 == GetFileAttributesW(plistFile.c_str()))
        {
            break;
        }

        fileHandle = CreateFileW(plistFile.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        if (INVALID_HANDLE_VALUE == fileHandle)
        {
            break;
        }

        if (!GetFileSizeEx(fileHandle, &plistDataSize))
        {
            break;
        }

        if (0 == plistDataSize.QuadPart)
        {
            break;
        }

        if (0 != plistDataSize.HighPart)
        {
            break;
        }

        plistData = new char[plistDataSize.LowPart + 1]();
        if (!plistData)
        {
            break;
        }

        if (!ReadFile(fileHandle, plistData, plistDataSize.LowPart, &readBytes, 0) ||
            readBytes != plistDataSize.LowPart)
        {
            break;
        }

        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;

        plist_from_bin(plistData, plistDataSize.LowPart, &plist);
        if (!plist)
        {
            break;
        }

        plist_dict_new_iter(plist, &iter);
        if (!iter)
        {
            break;
        }

        plist_dict_next_item(plist, iter, NULL, &plistNode);
        while (plistNode)
        {
            if (PLIST_ARRAY == plist_get_node_type(plistNode))
            {
                plistArraySize = plist_array_get_size(plistNode);
                if (4 > plistArraySize)
                {
                    break;
                }

                plistNode2 = plist_array_get_item(plistNode, 2);
                if (PLIST_STRING == plist_get_node_type(plistNode2))
                {
                    plist_get_string_val(plistNode2, &stringValue);
                    accountInfo.nickName = (stringValue != NULL) ? stringValue : "";
                }

                if (stringValue)
                {
                    free(stringValue);
                    stringValue = NULL;
                    break;
                }
            }

            plistNode = NULL;
            plist_dict_next_item(plist, iter, NULL, &plistNode);
        }

        plist_free(plist);
        plist = NULL;

    } while (0);

    if (INVALID_HANDLE_VALUE != fileHandle)
    {
        CloseHandle(fileHandle);
    }

    if (plistData)
    {
        delete plistData;
    }

    if (plist)
    {
        plist_free(plist);
    }
}

bool IosGetAccountInfo(const std::string &account)
{
	sqlite3		*db = NULL;
	int			iRet;
	bool		bRet = false;
	char		szDbPath[MAX_FILE_PATH] = {0};

	if (!PathFileExistsW(g_accountInfo[account].dbPath.c_str()))
	{
		return bRet;
	}

	iRet = Sqlite3Helper.sqlite3_open16(g_accountInfo[account].dbPath.c_str(), &db);
	if (SQLITE_OK != iRet)
	{
		return bRet;
	}

	//get friend info
	//
	IosGetFriendInfo(db, account);

	//get group info
	//
	IosGetGroupInfo(db, account);

	//get troop info
	//
	IosGetTroopInfo(db, account);

	//get discussion info
	//
	IosGetDiscInfo(db, account);

	//get chat history
	//
	IosGetChatHistory(db, account);

	Sqlite3Helper.sqlite3_close(db);

    IosGetRecoveryChatHistory(account);

    Sqlite3Helper.sqlite3_close(db);

    return true;
}

void IosGetFriendNickName(sqlite3 *db, AccountInfo* pAccountInfo)
{
    if (NULL == pAccountInfo && NULL != db)
        return;

	if (pAccountInfo->friendList.size() == 0)
	{
		return;
	}
    list<FriendInfo>::iterator itor;
    for (itor = pAccountInfo->friendList.begin(); itor != pAccountInfo->friendList.end(); ++itor)
    {
        sqlite3_stmt* stmt = NULL;
        string sql;
        sql.append("SELECT nick, remark FROM tb_userSummary WHERE uin = '");
        sql.append((*itor).account);
        sql.append("'");

        int iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
        if (SQLITE_OK != iRet || !stmt)
        {
            return;
        }

        if(SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
        {
            const char * lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
            if (lpPointer != NULL )
            {
                (*itor).nickName = string(lpPointer);
            }

            const char* ptr = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
            if (ptr != NULL )
            {
                (*itor).remark = string(ptr);
            }
        }
        Sqlite3Helper.sqlite3_finalize(stmt);
    }
}

void IosGetFriendInfo(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	int				iRet = -1;
	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	sqlite3_stmt	*stmt = NULL;
	char	szSql[] = "SELECT DISTINCT tbl_name FROM sqlite_master WHERE tbl_name LIKE 'tb_c2cMsg_%'";

	iRet = Sqlite3Helper.sqlite3_prepare(db, szSql, lstrlenA(szSql), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		FriendInfo		friendInfo;
		const char *lpPointer = NULL;
		int  iSize = 0;

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
		lpPointer += 10;
		if ( lpPointer != NULL )
		{
            friendInfo.account = string(lpPointer);
		}
		friendInfo.groupId = 0;

        if (std::string::npos != friendInfo.account.find("_694F69B9_93B9_480a_B23C_F6D221EEAF39"))
        {
            continue;
        }
		pAccountInfo->friendList.push_back(friendInfo);
	}

	Sqlite3Helper.sqlite3_finalize(stmt);

    IosGetFriendNickName(db, pAccountInfo);
}

void IosGetGroupInfo(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	GroupInfo		groupInfo;
	char			szName[20] = {0};

    groupInfo.groupName = StringConvertor::AcsiiToUtf8("好友列表");
	groupInfo.groupId = 0;
	groupInfo.memberCount = pAccountInfo->friendList.size();

	pAccountInfo->groupList.push_back(groupInfo);
}

void IosGetTroopInfo(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	int				iRet = -1;
	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	sqlite3_stmt	*stmt = NULL;
	char	szSql[] = "SELECT groupcode, groupName FROM tb_troop";

    if (!pAccountInfo)
    {
        return;
    }

	iRet = Sqlite3Helper.sqlite3_prepare(db, szSql, lstrlenA(szSql), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		TroopInfo		troopInfo;
		const char *lpPointer = NULL;
		int  iSize = 0;

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
		if ( lpPointer != NULL )
		{
            troopInfo.troopUin = string(lpPointer);
		}

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
		if (lpPointer)
		{
            troopInfo.troopName = string(lpPointer);
		}

		pAccountInfo->troopList.push_back(troopInfo);
	}

	Sqlite3Helper.sqlite3_finalize(stmt);
}

void IosGetDiscInfo(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	int				iRet = -1;
	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	sqlite3_stmt	*stmt = NULL;
	char	szSql[] = "SELECT DiscussUin FROM tb_discussMaxSeq";

	iRet = Sqlite3Helper.sqlite3_prepare(db, szSql, lstrlenA(szSql), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		DiscInfo	discInfo;
		const char *lpPointer = NULL;
		int  iSize = 0;

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
		if ( lpPointer != NULL )
		{
            discInfo.discussUin = string(lpPointer);
		}

		pAccountInfo->discussList.push_back(discInfo);
	}

	Sqlite3Helper.sqlite3_finalize(stmt);
}

void IosGetChatHistory(sqlite3 *db, const std::string &account)
{
    if (!db)
    {
        return;
    }

    std::string tableName;
	std::string chatAccount;
	AccountInfo *pAccount = &g_accountInfo[account];

	//get friend chat history
	//
	
	if (pAccount->friendList.size() > 0)
	{
		std::list<FriendInfo>::iterator friendItera;
		for (friendItera = pAccount->friendList.begin(); friendItera != pAccount->friendList.end(); friendItera++)
		{
			chatAccount = friendItera->account;
            tableName = "tb_c2cMsg_" + chatAccount;
            IosGetSingleChatHistory(db, account, chatAccount, tableName, 0);
		}
	}


	//get troop chat history
	//
	if (pAccount->troopList.size() > 0)
	{
		std::list<TroopInfo>::iterator troopItera;
		for (troopItera = pAccount->troopList.begin(); troopItera != pAccount->troopList.end(); troopItera++)
		{
			chatAccount = troopItera->troopUin;
            tableName = "tb_TroopMsg_" + chatAccount;
            IosGetSingleChatHistory(db, account, chatAccount, tableName, 1);
		}
	}
	

	//get discussion chat history
	//
	if (pAccount->discussList.size() > 0)
	{
		std::list<DiscInfo>::iterator discItera;
		for (discItera = pAccount->discussList.begin(); discItera != pAccount->discussList.end(); discItera++)
		{
			chatAccount = discItera->discussUin;
            tableName = "tb_discussGrp_" + chatAccount;
            IosGetSingleChatHistory(db, account, chatAccount, tableName, 2);
		}
	}
}

void IosGetRecoveryChatHistory(const std::string &account)
{
    SQLiteDataRecovery *recoveryHandler = CreateRecoveryClass();
    if (!recoveryHandler)
    {
        return;
    }

    std::list<TargetTableInfo> targetTableInfos;

    std::string tableName;
    std::string chatAccount;
    AccountInfo *pAccount = &g_accountInfo[account];

    std::wstring dbBackPath;
    std::wstring srcWalFile;
    std::wstring dstWalFile;
    std::wstring srcShmFile;
    std::wstring dstShmFile;

    dbBackPath = pAccount->dbPath + L"_back";
    if (!CopyFileW(pAccount->dbPath.c_str(), dbBackPath.c_str(), FALSE))
    {
        return;
    }

    srcWalFile = pAccount->dbPath + L"-wal";
    dstWalFile = dbBackPath + L"-wal";
    if (PathFileExistsW(srcWalFile.c_str()))
    {
        if (!CopyFileW(srcWalFile.c_str(), dstWalFile.c_str(), FALSE))
        {
            return;
        }
    }

    srcShmFile = pAccount->dbPath + L"-shm";
    dstShmFile = dbBackPath + L"-shm";
    if (PathFileExistsW(srcShmFile.c_str()))
    {
        if (!CopyFileW(srcShmFile.c_str(), dstShmFile.c_str(), FALSE))
        {
            return;
        }
    }

    //get friend chat history
    //
    if (pAccount->friendList.size() > 0)
    {
        TargetTableInfo tableInfo;
        tableInfo.recoveryAppType = IOS_RECOVERY_QQ;

        std::list<FriendInfo>::iterator friendItera;
        for (friendItera = pAccount->friendList.begin(); friendItera != pAccount->friendList.end(); friendItera++)
        {
            chatAccount = friendItera->account;
            tableInfo.srcTableName = L"tb_c2cMsg_" + StringConvertor::AcsiiToUnicode(chatAccount);
            tableInfo.dstTableName = tableInfo.srcTableName + L"_694F69B9_93B9_480a_B23C_F6D221EEAF39";
            tableInfo.mainColumnName = L"content";
            tableInfo.uniqueColumnName = L"time";
            tableInfo.chatAccount = chatAccount;
            targetTableInfos.push_back(tableInfo);
        }
    }


    //get troop chat history
    //
    if (pAccount->troopList.size() > 0)
    {
        TargetTableInfo tableInfo;
        tableInfo.recoveryAppType = IOS_RECOVERY_QQ;

        std::list<TroopInfo>::iterator troopItera;
        for (troopItera = pAccount->troopList.begin(); troopItera != pAccount->troopList.end(); troopItera++)
        {
            chatAccount = troopItera->troopUin;
            tableInfo.srcTableName = L"tb_TroopMsg_" + StringConvertor::AcsiiToUnicode(chatAccount);
            tableInfo.dstTableName = tableInfo.srcTableName + L"_694F69B9_93B9_480a_B23C_F6D221EEAF39";
            tableInfo.mainColumnName = L"strMsg";
            tableInfo.uniqueColumnName = L"MsgTime";
            tableInfo.chatAccount = chatAccount;
            targetTableInfos.push_back(tableInfo);
        }
    }


    //get discussion chat history
    //
    if (pAccount->discussList.size() > 0)
    {
        TargetTableInfo tableInfo;
        tableInfo.recoveryAppType = IOS_RECOVERY_QQ;

        std::list<DiscInfo>::iterator discItera;
        for (discItera = pAccount->discussList.begin(); discItera != pAccount->discussList.end(); discItera++)
        {
            chatAccount = discItera->discussUin;
            tableInfo.srcTableName = L"tb_discussGrp_" + StringConvertor::AcsiiToUnicode(chatAccount);
            tableInfo.dstTableName = tableInfo.srcTableName + L"_694F69B9_93B9_480a_B23C_F6D221EEAF39";
            tableInfo.mainColumnName = L"Msg";
            tableInfo.uniqueColumnName = L"MsgTime";
            tableInfo.chatAccount = chatAccount;
            targetTableInfos.push_back(tableInfo);
        }
    }

    recoveryHandler->StartRecovery(dbBackPath, targetTableInfos);
    DestoryRecoveryClass(recoveryHandler);

    sqlite3 *db = NULL;
    if (SQLITE_OK != Sqlite3Helper.sqlite3_open16(dbBackPath.c_str(), &db))
    {
        return;
    }

    //get friend chat history
    //
    if (pAccount->friendList.size() > 0)
    {
        std::list<FriendInfo>::iterator friendItera;
        for (friendItera = pAccount->friendList.begin(); friendItera != pAccount->friendList.end(); friendItera++)
        {
            chatAccount = friendItera->account;
            tableName = "tb_c2cMsg_" + chatAccount + "_694F69B9_93B9_480a_B23C_F6D221EEAF39";
            IosGetSingleChatHistory(db, account, chatAccount, tableName, 0, true);
        }
    }


    //get troop chat history
    //
    if (pAccount->troopList.size() > 0)
    {
        std::list<TroopInfo>::iterator troopItera;
        for (troopItera = pAccount->troopList.begin(); troopItera != pAccount->troopList.end(); troopItera++)
        {
            chatAccount = troopItera->troopUin;
            tableName = "tb_TroopMsg_" + chatAccount + "_694F69B9_93B9_480a_B23C_F6D221EEAF39";
            IosGetSingleChatHistory(db, account, chatAccount, tableName, 0, true);
        }
    }


    //get discussion chat history
    //
    if (pAccount->discussList.size() > 0)
    {
        std::list<DiscInfo>::iterator discItera;
        for (discItera = pAccount->discussList.begin(); discItera != pAccount->discussList.end(); discItera++)
        {
            chatAccount = discItera->discussUin;
            tableName = "tb_discussGrp_" + chatAccount + "_694F69B9_93B9_480a_B23C_F6D221EEAF39";
            IosGetSingleChatHistory(db, account, chatAccount, tableName, 0, true);
        }
    }

    DeleteFileW(dbBackPath.c_str());
    DeleteFileW(dstWalFile.c_str());
    DeleteFileW(dstShmFile.c_str());
}

void IosGetSingleChatHistory(sqlite3 *db,
                             const std::string &account,
                             const std::string &chatAccount,
                             const std::string &tableName,
                             int iType,
                             bool isRecovery)
{
    if (!db)
    {
        return;
    }

	std::string		hash;
	std::string		sql;
	std::string		partname;

	int				iRet = -1;
	sqlite3_stmt	*stmt = NULL;

	//check if table exists
	//
	switch (iType)
	{
	case  0:
		partname = "friend";
		break;
	case 1:
		partname = "troop";
		break;
	case 2:
		partname = "discussion";
		break;
	}
	sql = "SELECT * FROM sqlite_master WHERE type=\"table\" AND name = \"" + tableName + "\"";

	iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	iRet = Sqlite3Helper.sqlite3_step(stmt);
	if (SQLITE_ROW != iRet)
	{
		Sqlite3Helper.sqlite3_finalize(stmt);
		return;
	}

	Sqlite3Helper.sqlite3_finalize(stmt);
    stmt = NULL;

	//
	//get account chat history
	ChatInfo		chatInfo;

	switch(iType)
	{
	case 0:
		sql = "SELECT content, datetime(time,'unixepoch','localtime'), flag, type, picUrl, fileMsgType FROM " + tableName;
		break;
	case 1:
		sql = "SELECT strMsg, datetime(MsgTime,'unixepoch','localtime'), sMsgType, SendUin, nickName, picUrl From " + tableName;
		break;
	case 2:
		sql = "SELECT Msg, datetime(MsgTime,'unixepoch','localtime'), MsgType, SendUin, NickName, picUrl From " + tableName;
		break;
	}

	iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
	if (SQLITE_OK != iRet || !stmt)
	{
		return;
	}

	while(SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		ChatHistory		chatHistory;
		const char *lpPointer = NULL;
		int  iSize = 0;
		int  msgType;

		//message
		//
		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		if (iSize > 0)
		{
			lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
			if (lpPointer)
			{
				char* szTmp = new char[iSize + 1]();	
				if (szTmp)
				{	
					int iTmp = 0;
					while (iTmp < iSize)
					{
						char ch = lpPointer[iTmp];
						if (ch != '\0')
						{
							szTmp[iTmp] = ch;
						}
						else
						{
							szTmp[iTmp] = '\n';
						}
						iTmp++;
					}
                    chatHistory.message = string(szTmp);
					delete [] szTmp;
				}
			}
		}

		//time
		//
		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
		if ( lpPointer != NULL )
		{
            chatHistory.time = string(lpPointer);
		}

		//type
		//
		msgType = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 3);

		if (0 == iType)
		{
			//issend
			//
			chatHistory.isSend = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 2);
			if (chatHistory.isSend)
			{
				chatHistory.isSend = 1;
			}

			//type
			//
			msgType = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 3);

            if (!chatHistory.isSend)
            {
                if (g_accountInfoEx.find(account) != g_accountInfoEx.end())
                {
                    if (!g_accountInfoEx[account].nickName.empty())
                    {
                        chatHistory.senderName = g_accountInfoEx[account].nickName;
                    }
                }
                
                chatHistory.senderUin = account;
            }
            else
            {
                chatHistory.senderUin = chatAccount;
            }
		}
		else
		{
			////msg type
			////
			msgType = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 2);

			chatHistory.isSend = 0;
			//sender uin
			//
			if (1 == iType)
			{
				iSize = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 3);
				if (iSize != 0)
				{
					std::stringstream ss;
					ss << iSize;
					ss >> chatHistory.senderUin;
				}
				else
				{
					chatHistory.senderUin = account;
				}
			}
			else
			{
				iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 3);
				lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 3);
				if ( lpPointer != NULL )
				{
                    chatHistory.senderUin = string(lpPointer);
				}
			}

			//nick name
			//
			iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 4);
			lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 4);
			if ( lpPointer != NULL )
			{
                chatHistory.senderName = string(lpPointer);
			}
		}

		//fileType
		//
		__int64 fileType = Sqlite3Helper.sqlite3_column_int64(stmt, 5);

		//0:文字，系统表情  1:图片  3:语音  4:文件   133:动态表情   141:地图、提醒事件、推荐联系人
		//12:视频通话 147: 语音通话 150:语音通话取消
		switch (msgType)
		{
		case 0:
			chatHistory.msgType = QQ_MSG_TXT;
			break;

		case 1:
			{
				chatHistory.msgType = QQ_MSG_PIC;
				string picUrl;
				string filePath;
                const char *data = NULL;
				if ( iType == 0 )
				{
					data = (const char*)Sqlite3Helper.sqlite3_column_blob(stmt, 4);
                    picUrl = data ? data : "";
				}
				else
				{
					data = (const char*)Sqlite3Helper.sqlite3_column_blob(stmt, 5);
                    picUrl = data ? data : "";
                }

                string isOriginal  = MsgBodyParseA(picUrl, "isOriginal");
                string filename	   = MsgBodyParseA(picUrl, "md5");
                filename += ".png";
                if ( isOriginal == "1" )
                {
                    filePath = StringConvertor::UnicodeToUtf8(g_qqDirPath) + "\\Documents\\" + account + "\\image_original\\" + filename;
                }
                else
                {
                    filePath = StringConvertor::UnicodeToUtf8(g_qqDirPath) + "\\Documents\\" + account + "\\image_big\\" + filename;
                }
                string thumbPath = StringConvertor::UnicodeToUtf8(g_qqDirPath) + "\\Documents\\" + account + "\\image_thumbnail\\" + filename;
				chatHistory.filePath  = filePath;
				chatHistory.thumbPath = thumbPath;
				break;
			}

		case 3:
			{
                chatHistory.msgType = QQ_MSG_AUDIO;
                string filePath;
                string filename	= chatHistory.message;
                int pos = filename.find("+");
                if ( pos != string::npos )
                {
                    filename = filename.substr(0, pos);
                }
                filename += ".amr";
                filePath = StringConvertor::UnicodeToUtf8(g_qqDirPath) + "\\Documents\\" + account + "\\Audio\\" + filename;
                chatHistory.filePath = filePath;
				break;
			}

		case 4:
			{
				chatHistory.msgType = QQ_MSG_FILE;
				string content = chatHistory.message;
				string filePath;
				int pos = 0;

				if ( (pos = content.find("|")) != string::npos )
				{
					content = content.substr(0, pos);
					while ( (pos = content.find('/')) != string::npos )
					{
						content = content.replace(pos, 1, "\\");
					}
				}

                //文件在本地
				if ( (pos = chatHistory.message.find("Documents")) != string::npos )
				{
					//图片文件
					if ( fileType == 1 )
					{
                        chatHistory.filePath = StringConvertor::UnicodeToUtf8(g_qqDirPath)+"\\Documents\\"+account+"\\Image\\"+content;
					}
					else if ( fileType == 0 )
					{
                        chatHistory.filePath = StringConvertor::UnicodeToUtf8(g_qqDirPath)+"\\Documents\\"+account+"\\MyFolder\\"+content;
					}	
					else
					{
                        chatHistory.filePath = StringConvertor::UnicodeToUtf8(g_qqDirPath)+"\\Documents\\"+account+"\\FileRecv\\"+content;
					}
				}
				//文件不在本地
				else
				{
					chatHistory.filePath = content;
				}

				break;
			}

		case 133:
			chatHistory.msgType = QQ_DYNAMIC_FACE;
			break;

		case 141:
			{
				chatHistory.msgType = QQ_EVENT_REMID;
				//解析 xml格式的内容
				const char *szXml = chatHistory.message.c_str();
				TiXmlDocument doc; 
				doc.Parse(szXml); 
				TiXmlHandle docHandle( &doc ); 
				TiXmlHandle xmlHandle = docHandle.FirstChildElement("msg");
				TiXmlElement *element = xmlHandle.ToElement();
				if ( element != NULL )
				{
					const char* szTmp = element->Attribute("brief");
					if (!szTmp)
					{
						break;
					}
					string brief( szTmp );
                    if ( brief.find(StringConvertor::AcsiiToUtf8("位置")) != string::npos)
					{
						chatHistory.msgType = QQ_MSG_MAP;
                        const char *attr = element->Attribute("actionData");
                        if (attr)
                        {
                            string actionData = attr;
                            actionData = actionData.substr(actionData.find("lat"));
                            chatHistory.message = StringConvertor::AcsiiToUtf8("[分享位置]") + actionData;
                        }
					}
					else if ( brief.find(StringConvertor::AcsiiToUtf8("提醒")) != string::npos )
					{
						chatHistory.msgType  = QQ_EVENT_REMID;
                        const char *attr = element->Attribute("url");
                        if (attr)
                        {
                            chatHistory.filePath = attr;
                            if ( (element = element->FirstChildElement("item")) != NULL )
                            {
                                if ( (element = element->FirstChildElement("summary")) != NULL )
                                {
                                    const char *text = element->GetText();
                                    if (text)
                                    {
                                        chatHistory.message = "[" + brief + "] " + StringConvertor::AcsiiToUtf8("事件内容: ") + string(text);
                                    }
                                }
                            }
                        }
					}
					else if ( brief.find(StringConvertor::AcsiiToUtf8("推荐")) != string::npos )
					{
						chatHistory.msgType = QQ_RCMD_CONTACT;
						if ( (element = element->FirstChildElement("item")) != NULL )
						{
							string summary;
							string nickname;
							string uin;
                            const char *text = NULL;
							TiXmlElement* sumElement = element->FirstChildElement("summary");
							if ( sumElement != NULL )
							{
                                text = sumElement->GetText();
								summary = text;
							}
							if ( (element = element->NextSiblingElement("item")) != NULL  )
							{
								TiXmlElement* nicknameEle = element->FirstChildElement("title");
								if ( nicknameEle != NULL )
								{
                                    text = nicknameEle->GetText();
									nickname = text;
								}
								TiXmlElement* uinEle = element->FirstChildElement("summary");
								if ( uinEle != NULL )
								{
                                    text = uinEle->GetText();
									uin = text;
								}
							}

							chatHistory.message = "[" + summary + "] " + StringConvertor::AcsiiToUtf8("昵称: ") + nickname + ", " + uin;
						}
					}		
				}
				break;
			}

        case 181:
            {
                chatHistory.msgType = QQ_MSG_VIDEO;
            }
            break;
		case 12:
		case 147:
		case 150:
			chatHistory.msgType = QQ_MSG_CALL;
			break;

		default:
			{
				chatHistory.msgType = QQ_MSG_OTHER;
			}
			break;
		}

		chatInfo.chatList.push_back(chatHistory);
	}

    if (!isRecovery)
    {
        g_accountInfo[account].chatMap[chatAccount + "-" + partname] = chatInfo;
    }
    else
    {
        g_accountInfo[account].recoveryChatMap[chatAccount + "-" + partname] = chatInfo;
    }

	Sqlite3Helper.sqlite3_finalize(stmt);
	return;
}

FriendInfo* QQ_GetFirstFriend(std::string &account)
{
	if (g_accountInfo.size() == 0)
	{
		return NULL;
	}
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
	if (pAccountInfo->friendList.size() == 0)
	{
		return NULL;
	}
	pAccountInfo->friendItera = pAccountInfo->friendList.begin();
	if (pAccountInfo->friendItera == pAccountInfo->friendList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pAccountInfo->friendItera);
	}
}

FriendInfo* QQ_GetNextFriend(std::string &account)
{
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
	if (pAccountInfo->friendItera == pAccountInfo->friendList.end())
	{
		return NULL;
	}

	pAccountInfo->friendItera++;
	if (pAccountInfo->friendItera == pAccountInfo->friendList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pAccountInfo->friendItera);
	}
}


GroupInfo* QQ_GetFirstGroup(std::string &account)
{
	if (g_accountInfo.size() == 0)
	{
		return NULL;
	}
	
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
	if (pAccountInfo->groupList.size() == 0)
	{
		return NULL;
	}
	pAccountInfo->groupItera = pAccountInfo->groupList.begin();
	if (pAccountInfo->groupItera == pAccountInfo->groupList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pAccountInfo->groupItera);
	}
}

GroupInfo* QQ_GetNextGroup(std::string &account)
{
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
	if (pAccountInfo->groupItera == pAccountInfo->groupList.end())
	{
		return NULL;
	}

	pAccountInfo->groupItera++;
	if (pAccountInfo->groupItera == pAccountInfo->groupList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pAccountInfo->groupItera);
	}
}

TroopInfo* QQ_GetFirstTroop(std::string &account)
{
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
	pAccountInfo->troopItera = pAccountInfo->troopList.begin();
	if (pAccountInfo->troopItera == pAccountInfo->troopList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pAccountInfo->troopItera);
	}
}

TroopInfo* QQ_GetNextTroop(std::string &account)
{
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
	if (pAccountInfo->troopItera == pAccountInfo->troopList.end())
	{
		return NULL;
	}

	pAccountInfo->troopItera++;
	if (pAccountInfo->troopItera == pAccountInfo->troopList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pAccountInfo->troopItera);
	}
}

DiscInfo* QQ_GetFirstDisc(std::string &account)
{
	if (g_accountInfo.size() == 0)
	{
		return NULL;
	}
	
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
	if (pAccountInfo->discussList.size() == 0)
	{
		return NULL;
	}
	pAccountInfo->discussItera = pAccountInfo->discussList.begin();
	if (pAccountInfo->discussItera == pAccountInfo->discussList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pAccountInfo->discussItera);
	}
}

DiscInfo* QQ_GetNextDisc(std::string &account)
{
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
	if (pAccountInfo->discussItera == pAccountInfo->discussList.end())
	{
		return NULL;
	}

	pAccountInfo->discussItera++;
	if (pAccountInfo->discussItera == pAccountInfo->discussList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pAccountInfo->discussItera);
	}
}

ChatHistory* QQ_GetFirstChatHistory(std::string &account, std::string &chatAccount, int iType, bool isRecovery)
{
	std::string partName;
	std::string ttlName;

	switch(iType)
	{
	case 0:
		partName = "friend";
		break;
	case 1:
		partName = "troop";
		break;
	case 2:
		partName = "discussion";
		break;
	default:
		;
	}

	if (partName.empty())
	{
		return NULL;
	}

	if (g_accountInfo[account].chatMap.size() == 0)
	{
		return NULL;
	}
	ttlName = chatAccount + "-" + partName;
	std::map<std::string,ChatInfo>::iterator itera;
    
    if (!isRecovery)
    {
        itera = g_accountInfo[account].chatMap.find(ttlName);
        if (itera == g_accountInfo[account].chatMap.end())
        {
            if (2 == iType)
            {
                partName = "discusssion";
                ttlName = chatAccount + "-" + partName;
                itera = g_accountInfo[account].chatMap.find(ttlName);
                if (itera == g_accountInfo[account].chatMap.end())
                {
                    return NULL;
                }
            }
            else
            {
                return NULL;
            }
        }
    }
    else
    {
        itera = g_accountInfo[account].recoveryChatMap.find(ttlName);
        if (itera == g_accountInfo[account].recoveryChatMap.end())
        {
            if (2 == iType)
            {
                partName = "discusssion";
                ttlName = chatAccount + "-" + partName;
                itera = g_accountInfo[account].recoveryChatMap.find(ttlName);
                if (itera == g_accountInfo[account].recoveryChatMap.end())
                {
                    return NULL;
                }
            }
            else
            {
                return NULL;
            }
        }
    }

	ChatInfo *pChatInfo = &itera->second;

	if (pChatInfo->chatList.size() == 0)
	{
		return NULL;
	}

	pChatInfo->chatItera = pChatInfo->chatList.begin();
	if (pChatInfo->chatItera == pChatInfo->chatList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pChatInfo->chatItera);
	}
}

ChatHistory* QQ_GetNextChatHistory(std::string &account, std::string &chatAccount, int iType, bool isRecovery)
{
	std::string partName;
	std::string ttlName;

	switch(iType)
	{
	case 0:
		partName = "friend";
		break;
	case 1:
		partName = "troop";
		break;
	case 2:
		partName = "discussion";
		break;
	default:
		;
	}

	if (partName.empty())
	{
		return NULL;
	}

	ttlName = chatAccount + "-" + partName;
	std::map<std::string,ChatInfo>::iterator itera;
    
    if (!isRecovery)
    {
        itera = g_accountInfo[account].chatMap.find(ttlName);
        if (itera == g_accountInfo[account].chatMap.end())
        {
            return NULL;
        }
    }
    else
    {
        itera = g_accountInfo[account].recoveryChatMap.find(ttlName);
        if (itera == g_accountInfo[account].recoveryChatMap.end())
        {
            return NULL;
        }
    }

	ChatInfo *pChatInfo = &itera->second;

	if (pChatInfo->chatItera == pChatInfo->chatList.end())
	{
		return NULL;
	}

	pChatInfo->chatItera++;
	if (pChatInfo->chatItera == pChatInfo->chatList.end())
	{
		return NULL;
	}
	else
	{
		return &(*pChatInfo->chatItera);
	}
}

AccountInfoEx* QQ_GetAccoutInfoEx(const std::string &account)
{
    return (g_accountInfoEx.find(account) != g_accountInfoEx.end()) ? &g_accountInfoEx[account] : NULL;
}