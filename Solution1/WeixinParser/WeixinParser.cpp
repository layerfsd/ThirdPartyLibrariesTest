// WeixinParser.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "WeixinParserInternal.h"
#include <Shlwapi.h>
#include <map>
#include <vector>
#include <sstream>
#include <openssl\evp.h>
#include <openssl\rand.h>
#include <openssl\ossl_typ.h>
#include <openssl\hmac.h>
#include <openssl\md5.h>
#include "..\Utils\tinyxml2\tinyxml2.h"
#include "..\Utils\StringConvertor.h"
#include <plist\plist.h>
#include <SQLiteDataRecoveryHeader.h>

#define PAGESIZE 1024
#define PBKDF2_ITER 4000
#define FILE_HEADER_SZ 16

using std::stringstream;


#pragma comment(lib, "ShlWapi.lib")

// openssl
#pragma comment(lib, "libcrypto")


std::wstring	g_wsDirPath;
std::string		g_sKey;
std::string		g_imei;
SQLITE3_HELPER	Sqlite3Helper;
SQLITE3_HELPER	sqliteTmp;
int	 g_iType;
bool isHaveFriendInfos = true;

std::map<std::string, AccountInfo> g_accountInfo;
std::map<std::string, AccountInfoEx> g_accountInfoEx;

bool DecryptDatabaseFile( const std::wstring& dbFile, const std::wstring& outputFile, const char* password)
{
	int i, csz, tmp_csz, key_sz, iv_sz;
	FILE *infh, *outfh;
	int read, written;
	unsigned char *inbuffer, *outbuffer, *salt, *out, *key, *iv;
	EVP_CIPHER *evp_cipher;

	// 详见：https://github.com/openssl/openssl/issues/962
	// OpenSSL 1.1.0之后的改变
	//EVP_CIPHER_CTX ectx;
	EVP_CIPHER_CTX *ectx = EVP_CIPHER_CTX_new();
	if (!ectx)
	{
		return false;
	}

	OpenSSL_add_all_algorithms();

	evp_cipher = (EVP_CIPHER *) EVP_get_cipherbyname("aes-256-cbc");

	key_sz = EVP_CIPHER_key_length(evp_cipher);
	key = (unsigned char*)malloc(key_sz);

	iv_sz = EVP_CIPHER_iv_length(evp_cipher);
	iv = (unsigned char*)malloc(iv_sz);

	inbuffer = (unsigned char*) malloc(PAGESIZE);
	outbuffer = (unsigned char*) malloc(PAGESIZE);
	salt = (unsigned char*)malloc(FILE_HEADER_SZ);

	infh = _wfopen(dbFile.c_str(), L"rb");
	outfh = _wfopen(outputFile.c_str(), L"wb");

	if ( !infh )
	{
		//__TRACE(L"[SQLiteHelper] Open File Failed. [%s]\r\n", dbFile.c_str());
		return false;
	}

	if ( !outfh )
	{
		//__TRACE(L"[SQLiteHelper] Open File Failed. [%s]\r\n", outputFile.c_str());
		return false;
	}

	read = fread(inbuffer, 1, PAGESIZE, infh);  /* read the first page */
	memcpy(salt, inbuffer, FILE_HEADER_SZ); /* first 16 bytes are the random database salt */

	PKCS5_PBKDF2_HMAC_SHA1(password, strlen(password), salt, FILE_HEADER_SZ, PBKDF2_ITER, key_sz, key);

	memset(outbuffer, 0, PAGESIZE);
	out = outbuffer;

	memcpy(iv, inbuffer + PAGESIZE - iv_sz, iv_sz); /* last iv_sz bytes are the initialization vector */

    // EVP_CipherInit/EVP_CipherFinal函数在OpenSSL 1.1.0版本中已经不适用，
    // 应使用EVP_CipherInit_ex/EVP_CipherFinal_ex代替

    //EVP_CipherInit(ectx, evp_cipher, NULL, NULL, 0);
    EVP_CipherInit_ex(ectx, evp_cipher, NULL, NULL, NULL, 0);
	EVP_CIPHER_CTX_set_padding(ectx, 0);
    //EVP_CipherInit(ectx, NULL, key, iv, 0);
    EVP_CipherInit_ex(ectx, NULL, NULL, key, iv, 0);
	EVP_CipherUpdate(ectx, out, &tmp_csz, inbuffer + FILE_HEADER_SZ, PAGESIZE - iv_sz - FILE_HEADER_SZ);
	csz = tmp_csz;  
	out += tmp_csz;
    //EVP_CipherFinal(ectx, out, &tmp_csz);
    EVP_CipherFinal_ex(ectx, out, &tmp_csz);
	csz += tmp_csz;
	EVP_CIPHER_CTX_cleanup(ectx);

	fwrite("SQLite format 3\0", 1, FILE_HEADER_SZ, outfh);
	fwrite(outbuffer, 1, PAGESIZE - FILE_HEADER_SZ, outfh);

	for(i = 1; (read = fread(inbuffer, 1, PAGESIZE, infh)) > 0 ;i++) {
		memcpy(iv, inbuffer + PAGESIZE - iv_sz, iv_sz); /* last iv_sz bytes are the initialization vector */
		memset(outbuffer, 0, PAGESIZE);
		out = outbuffer;

        //EVP_CipherInit(ectx, evp_cipher, NULL, NULL, 0);
        EVP_CipherInit_ex(ectx, evp_cipher, NULL, NULL, NULL, 0);
		EVP_CIPHER_CTX_set_padding(ectx, 0);
        //EVP_CipherInit(ectx, NULL, key, iv, 0);
        EVP_CipherInit_ex(ectx, NULL, NULL, key, iv, 0);
		EVP_CipherUpdate(ectx, out, &tmp_csz, inbuffer, PAGESIZE - iv_sz);
		csz = tmp_csz;  
		out += tmp_csz;
        //EVP_CipherFinal(ectx, out, &tmp_csz);
        EVP_CipherFinal_ex(ectx, out, &tmp_csz);
		csz += tmp_csz;
		EVP_CIPHER_CTX_cleanup(ectx);

		fwrite(outbuffer, 1, PAGESIZE, outfh);
	}

	EVP_CIPHER_CTX_free(ectx);

	fclose(infh);
	fclose(outfh);

	free(inbuffer);
	free(outbuffer);
	free(key);
	free(salt);
	free(iv);

	return true;
}

bool GetFileUin(std::string &uin, std::string parsePath, std::string parseField)
{
	do 
	{
		uin.clear();
		if (parsePath.empty() || parseField.empty())
		{
			break;
		}

		char	szXmlPath[MAX_FILE_PATH + 1] = {0};
		tinyxml2::XMLDocument 	doc	;
		tinyxml2::XMLElement	*xmlEm;
		int				iUin = 0;
		std::string		xmlPath;

		WideCharToMultiByte(CP_ACP, 0, g_wsDirPath.c_str(), -1, szXmlPath, MAX_FILE_PATH, NULL, NULL);
		xmlPath = std::string(szXmlPath) + parsePath; //"\\shared_prefs\\system_config_prefs.xml"
		if (!PathFileExistsA(xmlPath.c_str()))
		{
			break;
		}

		if (doc.LoadFile(xmlPath.c_str()) == 0)
		{
			xmlEm = doc.RootElement()->FirstChildElement();
			while(xmlEm)
			{
				const tinyxml2::XMLAttribute* attribute = xmlEm->FirstAttribute();
				if (attribute)
				{
					const char* tmp = attribute->Value();
					if (tmp)
					{
						if (0 == lstrcmpA(tmp, parseField.c_str()))//"default_uin"))
						{
							xmlEm->QueryIntAttribute("value", &iUin);
							std::stringstream ss;
							ss << iUin;
							ss >> uin;

							break;
						}
					}
				}
				xmlEm = xmlEm->NextSiblingElement();
			}

		}
	} while (false);

	return !(uin.empty() || "0" == uin);
}

bool GetPreUin(std::string &uin)
{
	bool bGet = true;
	do 
	{
		if (GetFileUin(uin, "\\shared_prefs\\system_config_prefs.xml", "default_uin"))
		{
			break;
		}

		if (GetFileUin(uin, "\\sp\\system_config_prefs.xml", "default_uin"))
		{
			break;
		}

		if (GetFileUin(uin, "\\shared_prefs\\auth_info_key_prefs.xml", "_auth_uin"))
		{
			break;
		}

		if (GetFileUin(uin, "\\sp\\auth_info_key_prefs.xml", "_auth_uin"))
		{
			break;
		}

		bGet = false;
	} while (false);

	return bGet;
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

bool GetImeiFromPrefs(std::string &imei, const std::string& filePath, const std::string& xmlField)
{
	bool bRet = false;
	do 
	{
		if (filePath.empty() || xmlField.empty())
		{
			break;
		}

		std::string		xmlPath;
		char			szXmlPath[MAX_FILE_PATH + 1] = {0};
		WideCharToMultiByte(CP_ACP, 0, g_wsDirPath.c_str(), -1, szXmlPath, MAX_FILE_PATH, NULL, NULL);
		xmlPath = std::string(szXmlPath) + filePath;;
		if (!PathFileExistsA(xmlPath.c_str()))
		{
			break;
		}

		tinyxml2::XMLDocument 	qqDocument ;
		tinyxml2::XMLElement 	*xmlEm;
		char			szImei[100]	= {0};
		LPSTR			lpszPointer	= NULL;
		if (0 != qqDocument.LoadFile(xmlPath.c_str()))
		{
			break;
		}

		xmlEm = qqDocument.RootElement()->FirstChildElement();
		while(xmlEm)
		{
			const tinyxml2::XMLAttribute* attribute = xmlEm->FirstAttribute();
			if (attribute)
			{
				const char* value = attribute->Value();
				if (value)
				{
					if (0 == lstrcmpA(value, xmlField.c_str()))
					{
						const char* szImei = xmlEm->GetText();
						if (szImei)
						{
							imei = szImei;
							int iPos = imei.find('_');
							if (-1 != iPos)
							{
								imei = imei.substr(0, iPos);
							}
							bRet = true;
						}
						break;
					}
				}
			}
			xmlEm = xmlEm->NextSiblingElement();
		}

	} while (false);

	return bRet;
}

bool GetImeiFromXml(std::string &imei)
{
	bool			bRet		= false;
	do 
	{
		std::string filePath = "\\shared_prefs\\Access_Preferences.xml";
		std::string xmlFiled = "test_uuid";
		bRet = GetImeiFromPrefs(imei, filePath, xmlFiled);
		if (bRet)
		{
			break;
		}

		filePath = "\\shared_prefs\\DENGTA_META.xml";
		xmlFiled = "IMEI_DENGTA";
		bRet = GetImeiFromPrefs(imei, filePath, xmlFiled);
		if (bRet)
		{
			break;
		}

		filePath = "\\shared_prefs\\appcenter_mobileinfo.xml";
		xmlFiled = "IMEI";
		bRet = GetImeiFromPrefs(imei, filePath, xmlFiled);

	} while (false);

	return bRet;
}

bool GetImeiFromJavaMap0(std::wstring cfgPath, std::string& output)
{
	BOOL		        bBackup = FALSE;
	HANDLE				hRead = NULL;
	HANDLE				hWrite = NULL;
	//DWORD				dwExitCode;
	SECURITY_ATTRIBUTES sa = {0};
	DWORD dwRead = 0;

	output.clear();

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	PROCESS_INFORMATION piProcInfo = {0};
	STARTUPINFOW siStartInfo = {0};

	ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
	ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );

	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdOutput = hWrite;
	/*siStartInfo.hStdInput = hRead;*/
	siStartInfo.hStdError = hWrite;
	siStartInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	siStartInfo.wShowWindow = SW_HIDE;

	WCHAR path[MAX_PATH] = {0};
	GetModuleFileNameW(NULL, path, MAX_PATH);
	*wcsrchr(path, L'\\') = L'\0';

	wchar_t cmdLine[MAX_FILE_PATH] = {0};
	StringCchPrintfW(cmdLine, MAX_FILE_PATH, L"\"%s\\cfgParser.exe\" \"%s\"", path, cfgPath.c_str());

	if (!CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &siStartInfo, &piProcInfo))
	{
		CloseHandle(hRead);
		CloseHandle(hWrite);
		return false;
	}

	//
	//等5分钟
	WaitForSingleObject(piProcInfo.hProcess, 1000 * 60 * 5);
	
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	CloseHandle(hRead);
	CloseHandle(hWrite);

	

	std::wstring curPath = path;
	curPath += L"\\IMEI\\";
	wstring imeiPath = curPath + L"*.*";

	WIN32_FIND_DATAW fd = {0};
	HANDLE hf = FindFirstFileW(imeiPath.c_str(), &fd);
	do 
	{
		if ( std::wstring(fd.cFileName) == L".."
			|| std::wstring(fd.cFileName) == L"." )
			continue;

		std::wstring wImei = fd.cFileName;
		/*std::string tmp(wImei.length(), ' ');
		std::copy(wImei.begin(), wImei.end(), tmp.begin());
		output = tmp;*/
        output = StringConvertor::UnicodeToAcsii(wImei);

		//
		//避免影响下一次
		DeleteFileW((curPath + wImei).c_str());
		break;

	} while ( FindNextFile(hf, &fd) );
	FindClose(hf);

	if (output.empty())
	{
		int iPos = imeiPath.find(L"DataAnalysis");
		wstring imeiPath = path;
		if (-1 != iPos)
		{
			imeiPath = imeiPath.substr(0, iPos);
			curPath = imeiPath + L"FastCopy\\IMEI\\";
			imeiPath += L"FastCopy\\IMEI\\*.*";
			
			HANDLE hf = FindFirstFileW(imeiPath.c_str(), &fd);
			do 
			{
				if ( std::wstring(fd.cFileName) == L".."
					|| std::wstring(fd.cFileName) == L"." )
					continue;

				std::wstring wImei = fd.cFileName;
				/*std::string tmp(wImei.length(), ' ');
				std::copy(wImei.begin(), wImei.end(), tmp.begin());
				output = tmp;*/
                output = StringConvertor::UnicodeToAcsii(wImei);

				//
				//避免影响下一次
				DeleteFileW((curPath + wImei).c_str());
				break;

			} while ( FindNextFile(hf, &fd) );
			FindClose(hf);
		}	
	}

	return !output.empty();
}

bool GetImeiFromJavaMap(std::string &imei, int iImeiLen)
{
	int		iLen = 15;
	std::wstring filePath = g_wsDirPath + L"\\MicroMsg\\CompatibleInfo.cfg";

	GetImeiFromJavaMap0(filePath, imei);
	if (!imei.empty())
	{
		return true;
	}

	HANDLE hf = CreateFileW(filePath.c_str(), FILE_GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if ( INVALID_HANDLE_VALUE == hf )
	{
		return false;
	}

	if (iImeiLen > 0)
	{
		iLen = iImeiLen;
	}

	int fileSize = GetFileSize(hf, NULL);
	char* buffer = new char[fileSize + 1]();
	DWORD dump = 0;
	ReadFile(hf, buffer, fileSize, &dump, NULL);

	if (dump > 0x91)
	{
		char szImei[20] = {0};
		if (iLen > 19)
		{
			iLen = 19;
		}
		memcpy(szImei, buffer + 0x91, iLen);
		imei = szImei;

		//去掉英文字符
		string::iterator iter;
		for ( iter = imei.begin(); iter != imei.end(); )
		{
			if ((*iter) < L'0' || (*iter) > L'9')
				iter = imei.erase(iter);
			else
				iter++;
		}
	}

    //在整个文件中查找连续15个数字
    if (imei.length() < iLen)
    {
        int iNum = 0;
        char szImei[20] = {0};
        for(int i=0; i<fileSize; ++i)
        {
			
			if ((buffer[i] >= L'0' && buffer[i] <= L'9')
				|| (buffer[i] >= L'a' && buffer[i] <= L'z')
				|| (buffer[i] >= L'A' && buffer[i] <= L'Z'))
			{
				iNum++;
			}
			else
			{
				iNum = 0;
			}

            if (iNum == iLen)
            {
                memcpy(szImei, buffer + i - iLen + 1, iLen);
                imei = szImei;
				//break;
            }
        }
    }

	delete [] buffer;
	CloseHandle(hf);
	return (imei.size() >= iLen);
}


bool WEIXIN_InitEntry(LPCWSTR lpwzCasePath, int iType, char *lpszImei, bool isDbPath)
{
    g_imei.clear();
	g_iType = iType;
	if (lpszImei)
	{
		g_imei = lpszImei;
	}
	
	if (0 == g_iType)
	{
		return AndroidInitEntry(lpwzCasePath, lpszImei, isDbPath);
	}
	else if (1 == g_iType)
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
    bool	bRet = false;
    WCHAR	wzWeixinDir[MAX_FILE_PATH] = {0};
    std::wstring  filepath;

    do 
    {
        if (!lpwzCasePath)
        {
            break;
        }

        if (!PathFileExistsW(lpwzCasePath))
        {
            break;
        }

        if (!InitialSqlite3Helper())
        {
            return false;
        }

        if (isDbPath)
        {
            g_wsDirPath = lpwzCasePath;
            bRet = true;
            break;
        }

        StringCbCatW(wzWeixinDir, sizeof(wzWeixinDir), lpwzCasePath);
        if (wzWeixinDir[wcslen(wzWeixinDir) - 1] != L'\\')
        {
            StringCbCatW(wzWeixinDir, sizeof(wzWeixinDir), L"\\");
        }
        StringCbCatW(wzWeixinDir, sizeof(wzWeixinDir), L"com.tencent.mm");

        g_wsDirPath = wzWeixinDir;

        if (PathFileExistsW(wzWeixinDir))
        {
            bRet = true;
            break;
        }

        //.tar.gz
        //
        filepath = std::wstring(lpwzCasePath) + L"\\com.tencent.mm.tar.gz";
        if (PathFileExistsW(filepath.c_str()))
        {
            bRet = UnzipAndMove(filepath, g_wsDirPath, L"com.tencent.mm", 0);
        }
        
    } while (false);

	if (bRet)
	{
		//get the key to decrypt db file
		std::string  imei;
		std::string  uin;
		std::string  hash;
		std::string  key;

		GetPreUin(uin);

        /*
          javamap中记录的imei是正确的
        */
		GetImeiFromJavaMap(imei, 0);
		string imei2;
		GetImeiFromXml(imei2);
		if (!imei2.empty())
		{
			if ((imei.length() < 15 ) || (-1 != imei.find(imei2)))
			{
				imei = imei2;
			}
		}
		
		/*
		if(!GetImeiFromJavaMap(imei, 0))
		{
			if (imei.empty())
			{
				GetImeiFromXml(imei);
			}
			if (NULL == lpszImei || 0 == lstrcmpA(lpszImei, ""))
			{
				GetImeiFromJavaMap(imei, 0);
			}
		}*/
	

		if (imei.empty() && lpszImei)
		{
			imei = lpszImei;
		}

		if (uin.empty() || imei.empty())
		{
			return false;
		}

		GetMD5(imei + uin, hash);
		key = hash.substr(0, 7);
		g_sKey = key;
        if (g_imei.empty())
        {
            g_imei = imei;
        }
	}
	else
	{
		return false;
	}

	wstring configHeadFile = L"C:\\head.txt";
	wstring audioHeadFile  = g_wsDirPath + L"\\head.txt";
	if ( !PathFileExistsW(audioHeadFile.c_str()) )
	{
		CopyFile(configHeadFile.c_str(), audioHeadFile.c_str(), TRUE);
	}

	return bRet;
}

bool IosInitEntry(LPCWSTR lpwzCasePath, char *lpszImei, bool isDbPath)
{
	WCHAR	wzWeiXinDir[MAX_FILE_PATH] = {0};
	bool    bRet = false;
	std::wstring filepath;

	do 
	{
        if (!lpwzCasePath)
        {
            break;
        }

        if (!PathFileExistsW(lpwzCasePath))
        {
            break;
        }

        if (!InitialSqlite3Helper())
        {
            break;
        }

        sqliteTmp = Sqlite3Helper;

        if (isDbPath)
        {
            g_wsDirPath = lpwzCasePath;
            bRet = true;
            break;
        }

		StringCbCatW(wzWeiXinDir, sizeof(wzWeiXinDir), lpwzCasePath);
		if (wzWeiXinDir[wcslen(wzWeiXinDir) - 1] != L'\\')
		{
			StringCbCatW(wzWeiXinDir, sizeof(wzWeiXinDir), L"\\");
		}
		StringCbCatW(wzWeiXinDir, sizeof(wzWeiXinDir), L"com.tencent.xin");
		
        g_wsDirPath = wzWeiXinDir;
		if (PathFileExistsW(wzWeiXinDir))
		{
			bRet = true;
			break;
		}

		//.zip
		filepath = std::wstring(lpwzCasePath) + L"\\com.tencent.xin.zip";
		if (PathFileExistsW(filepath.c_str()))
		{
			bRet = UnzipAndMove(filepath, g_wsDirPath, L"com.tencent.xin", 1);
		}
	} while (false);

	wstring configHeadFile = L"C:\\head.txt";
	wstring audioHeadFile  = g_wsDirPath + L"\\Documents\\head.txt";
	if ( !PathFileExistsW(audioHeadFile.c_str()) )
	{
		CopyFile(configHeadFile.c_str(), audioHeadFile.c_str(), TRUE);
	}

	return bRet;
}

bool WEIXIN_GetAccountList(char *lpszAccounts, int iSize)
{
	if (0 == g_iType)
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

void WEIXIN_Free()
{
	std::map<std::string, AccountInfo>::iterator accountItera;
	for (accountItera = g_accountInfo.begin(); accountItera != g_accountInfo.end(); accountItera++)
	{
		accountItera->second.friendList.clear();
		accountItera->second.groupList.clear();
		accountItera->second.troopList.clear();
		accountItera->second.discussList.clear();

		std::map<std::string,ChatInfo>::iterator chatItera;
		for (chatItera = accountItera->second.chatMap.begin(); chatItera != accountItera->second.chatMap.end(); chatItera++)
		{
			chatItera->second.chatList.clear();
		}
		accountItera->second.chatMap.clear();
	}
	g_accountInfo.clear();

    g_accountInfoEx.clear();
}

string GetAccountDirName(string uin)
{
	if (uin.empty() || "0" == uin)
		return "";

	string preAccountDirName;
	GetMD5("mm" + uin, preAccountDirName);

	return preAccountDirName;
}

std::vector<string> GetAllAccountUin()
{
	std::vector<string> allUin;

	std::wstring findStr =  g_wsDirPath + L"\\MicroMsg\\ClickFlow\\*";
	WIN32_FIND_DATAW w32Find = {0};

	HANDLE hFind = FindFirstFileW(findStr.c_str(), &w32Find);
	if (INVALID_HANDLE_VALUE != hFind)
	{
		while(true) 
		{
			if (!(w32Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				string fileName = StringConvertor::UnicodeToUtf8(wstring(w32Find.cFileName));
				int postfixPos = fileName.find(".cfg");
				if (string::npos != postfixPos)
				{
					int startPos = fileName.find_last_of("_");
					if (-1 != startPos)
					{
						string uin = fileName.substr(startPos + 1, fileName.length() - startPos - 5);
						if (!uin.empty() && uin.length() > 5)
						{
							allUin.push_back(uin);
						}
					}		
				}
			}
			if (!FindNextFileW(hFind, &w32Find))
				break;
		} 
		FindClose(hFind);
	}

	return allUin;
}

bool AndroidGetAccountList(char *lpszAccounts, int iSize)
{
	string preUin;
	GetPreUin(preUin);
	string preAccountDirName = GetAccountDirName(preUin);

    std::vector<string> allUin = GetAllAccountUin();

    bool shouldUsePreUin = true;
    if (allUin.empty())
    {
        allUin.push_back(preUin);
        shouldUsePreUin = false;
    }
    else
    {
        for (std::vector<string>::iterator iter = allUin.begin();
            iter != allUin.end();
            ++iter)
        {
            if (preUin == *iter)
            {
                shouldUsePreUin = false;
                break;
            }
        }
    }

	std::string account;
	for(unsigned int i = 0; i < allUin.size(); ++i)
	{
		string accountDirName = GetAccountDirName(allUin[i]);
		string hash;
		GetMD5(g_imei + allUin[i], hash);
		string key = hash.substr(0, 7);

		HANDLE				hFind;
		WIN32_FIND_DATAW	w32Find = {0};
		std::wstring		findStr =  g_wsDirPath + L"\\MicroMsg\\*";

		hFind = FindFirstFileW(findStr.c_str(), &w32Find);
		if (INVALID_HANDLE_VALUE == hFind)
			return false;

		do 
		{
			if ((w32Find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				std::string fileName = StringConvertor::UnicodeToUtf8(wstring(w32Find.cFileName));
				if ((fileName == accountDirName) && (string::npos == account.find(fileName)))
				{
					if (!account.empty())
					{
						account += "|";
					}
					account += fileName;
					g_accountInfo[fileName].key = key;
					g_accountInfo[fileName].account = fileName;
					g_accountInfo[fileName].dbPath  = g_wsDirPath + L"\\MicroMsg\\" + w32Find.cFileName + L"\\EnMicroMsg.db";
					break;
				}
				else if (shouldUsePreUin && (fileName == preAccountDirName) && (string::npos == account.find(fileName)))
				{
                    shouldUsePreUin = false;
					if (!account.empty())
					{
						account += "|";
					}
					
					account += fileName;
					g_accountInfo[fileName].key = g_sKey;
					g_accountInfo[fileName].account = fileName;
					g_accountInfo[fileName].dbPath  = g_wsDirPath + L"\\MicroMsg\\" + w32Find.cFileName + L"\\EnMicroMsg.db";
				}
			}
		} while (FindNextFileW(hFind, &w32Find));

		FindClose(hFind);
	}

	if (!account.empty())
	{
		int iLen = account.length();
		account.copy(lpszAccounts, iLen);
	}

	return  true;
}

bool IosGetAccountList(char *lpszAccounts, int iSize)
{

	if (!lpszAccounts || iSize <= 0)
	{
		return false;
	}

	std::string			account;
	std::wstring		findStr =  g_wsDirPath + L"\\Documents\\*";
	HANDLE				hFind;
	WIN32_FIND_DATAW	w32Find = {0};
	char				szNumber[100] = {0};

	hFind = FindFirstFileW(findStr.c_str(), &w32Find);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		return false;
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

		if (wcslen(w32Find.cFileName) == 32
			&& w32Find.cFileName != wstring(L"00000000000000000000000000000000"))
		{
			if (0 != WideCharToMultiByte(CP_UTF8, 0, w32Find.cFileName, -1, szNumber, 100, NULL, NULL))
			{
				if (!account.empty())
				{
					account += "|";
				}
				account += szNumber;
				g_accountInfo[std::string(szNumber)].account = szNumber;
                g_accountInfo[std::string(szNumber)].dbPath  = g_wsDirPath + L"\\Documents\\" + w32Find.cFileName + L"\\DB\\MM.sqlite";
                g_accountInfo[std::string(szNumber)].firentDbPath  = g_wsDirPath + L"\\Documents\\" + w32Find.cFileName + L"\\DB\\WCDB_Contact.sqlite";
                std::wstring plistFile = g_wsDirPath + L"\\Documents\\" + w32Find.cFileName + L"\\mmsetting.archive";
                g_accountInfoEx[std::string(szNumber)].account = szNumber;
                IosGetAccountInfoEx(plistFile, g_accountInfoEx[std::string(szNumber)]);
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



bool WEIXIN_GetAccoutInfo(const std::string &account)
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

AccountInfoEx* WEIXIN_GetAccoutInfoEx(const std::string &account)
{
    return (g_accountInfoEx.find(account) != g_accountInfoEx.end()) ? &g_accountInfoEx[account] : NULL;
}

bool AndroidGetAccountInfo(const std::string &account)
{
	sqlite3		 *db = NULL;
	int			 iRet;
	bool		 bRet = false;
	char		 szDbPath[MAX_FILE_PATH] = {0};

    std::wstring dbBackPath;
    std::wstring srcWalFile;
    std::wstring dstWalFile;
    std::wstring srcShmFile;
    std::wstring dstShmFile;
    std::wstring ftsDbPath;
    std::wstring ftsDbBackPath;
    std::wstring ftsSrcWalFile;
    std::wstring ftsDstWalFile;
    std::wstring ftsSrcShmFile;
    std::wstring ftsDstShmFile;

    dbBackPath = g_accountInfo[account].dbPath + L"_back";
    if (!CopyFileW(g_accountInfo[account].dbPath.c_str(), dbBackPath.c_str(), FALSE))
    {
        return false;
    }

    srcWalFile = g_accountInfo[account].dbPath + L"-wal";
    dstWalFile = dbBackPath + L"-wal";
    if (PathFileExistsW(srcWalFile.c_str()))
    {
        if (!CopyFileW(srcWalFile.c_str(), dstWalFile.c_str(), FALSE))
        {
            return false;
        }
    }

    srcShmFile = g_accountInfo[account].dbPath + L"-shm";
    dstShmFile = dbBackPath + L"-shm";
    if (PathFileExistsW(srcShmFile.c_str()))
    {
        if (!CopyFileW(srcShmFile.c_str(), dstShmFile.c_str(), FALSE))
        {
            return false;
        }
    }

    size_t pos = g_accountInfo[account].dbPath.rfind(L'\\');
    if (std::wstring::npos != pos)
    {
        ftsDbPath = g_accountInfo[account].dbPath.substr(0, pos) + L"\\FTS5IndexMicroMsg.db";
        ftsDbBackPath = ftsDbPath + L"_back";
        
        if (PathFileExistsW(ftsDbPath.c_str()))
        {
            if (!CopyFileW(ftsDbPath.c_str(), ftsDbBackPath.c_str(), FALSE))
            {
                return false;
            }

            ftsSrcWalFile = ftsDbPath + L"-wal";
            ftsDstWalFile = ftsDbBackPath + L"-wal";
            if (PathFileExistsW(ftsSrcWalFile.c_str()))
            {
                if (!CopyFileW(ftsSrcWalFile.c_str(), ftsDstWalFile.c_str(), FALSE))
                {
                    return false;
                }
            }

            ftsSrcShmFile = ftsDbPath + L"-shm";
            ftsDstShmFile = ftsDbBackPath + L"-shm";
            if (PathFileExistsW(ftsSrcShmFile.c_str()))
            {
                if (!CopyFileW(ftsSrcShmFile.c_str(), ftsDstShmFile.c_str(), FALSE))
                {
                    return false;
                }
            }
        }
    }

	std::wstring wsDstFile = dbBackPath + L"-Descrypt";
    
    if (g_accountInfo.find(account) == g_accountInfo.end())
    {
        return false;
    }

	if (!DecryptDatabaseFile(dbBackPath, wsDstFile, g_accountInfo[account].key.c_str()))
	{
		return false;
	}

    if (PathFileExistsW(ftsDbBackPath.c_str()))
    {
        std::list<TargetTableInfo> targetTableInfos;
        TargetTableInfo tableInfo;
        tableInfo.recoveryAppType = ANDROID_RECOVERY_WEIXIN;
        tableInfo.srcTableName = L"FTS5MetaMessage";
        tableInfo.dstTableName = L"FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39";
        tableInfo.mainColumnName = L"talker";
        tableInfo.uniqueColumnName = L"timestamp";
        targetTableInfos.push_back(tableInfo);

        tableInfo.recoveryAppType = ANDROID_RECOVERY_WEIXIN;
        tableInfo.srcTableName = L"FTS5IndexMessage_content";
        tableInfo.dstTableName = L"FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39";
        tableInfo.mainColumnName = L"c0";
        tableInfo.uniqueColumnName = L"c0";
        targetTableInfos.push_back(tableInfo);

        SQLiteDataRecovery *dbRecoveryer = CreateRecoveryClass();
        if (dbRecoveryer)
        {
            dbRecoveryer->StartRecovery(ftsDbBackPath, targetTableInfos);

            targetTableInfos.clear();
            TargetTableInfo tableInfo;
            tableInfo.recoveryAppType = ANDROID_RECOVERY_WEIXIN;
            tableInfo.srcTableName = L"message";
            tableInfo.dstTableName = L"message_694F69B9_93B9_480a_B23C_F6D221EEAF39";
            tableInfo.mainColumnName = L"imgPath";
            tableInfo.uniqueColumnName = L"createTime";
            targetTableInfos.push_back(tableInfo);

            dbRecoveryer->StartRecovery(wsDstFile, targetTableInfos);

            DestoryRecoveryClass(dbRecoveryer);
        }
    }
    
	iRet = Sqlite3Helper.sqlite3_open16(wsDstFile.c_str(), &db);
	if (SQLITE_OK != iRet)
	{
		DeleteFileW(wsDstFile.c_str());
		return bRet;
	}

    std::string sql = "select value from userinfo where id = 4";
    sqlite3_stmt	*stmt = NULL;
    iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.size(), &stmt, NULL);
    if (SQLITE_OK == iRet)
    {
        if (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
        {
            std::string nickName = (const char*)Sqlite3Helper.sqlite3_column_text(stmt, 0) ? (const char*)Sqlite3Helper.sqlite3_column_text(stmt, 0) : "";
            if (!nickName.empty())
            {
                g_accountInfoEx[account].nickName = nickName;
            }
        }

        Sqlite3Helper.sqlite3_finalize(stmt);
    }

    sql = "select value from userinfo where id = 42";
    iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.size(), &stmt, NULL);
    if (SQLITE_OK == iRet)
    {
        if (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
        {
            std::string weixinNumber = (const char*)Sqlite3Helper.sqlite3_column_text(stmt, 0) ? (const char*)Sqlite3Helper.sqlite3_column_text(stmt, 0) : "";
            if (!weixinNumber.empty())
            {
                g_accountInfoEx[account].weixinNumber = weixinNumber;
            }
        }

        Sqlite3Helper.sqlite3_finalize(stmt);
    }

    if (g_accountInfoEx[account].weixinNumber.empty())
    {
        sql = "select value from userinfo where id = 2";
        iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.size(), &stmt, NULL);
        if (SQLITE_OK == iRet)
        {
            if (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
            {
                std::string weixinNumber = (const char*)Sqlite3Helper.sqlite3_column_text(stmt, 0) ? (const char*)Sqlite3Helper.sqlite3_column_text(stmt, 0) : "";
                if (!weixinNumber.empty())
                {
                    g_accountInfoEx[account].weixinNumber = weixinNumber;
                }
            }

            Sqlite3Helper.sqlite3_finalize(stmt);
        }
    }

    if (g_accountInfoEx[account].weixinNumber.empty())
    {
        g_accountInfoEx[account].weixinNumber = account;
    }

    //get friend info

	AndroidGetFriendInfo(db, account);
    //AndroidGetRecoveryFriendInfo(db, account);

	//get chat history
	//
	AndroidGetChatHistory(db, account);
    AndroidGetRecoveryChatHistory(db, account);

	Sqlite3Helper.sqlite3_close(db);

    DeleteFileW(ftsDbBackPath.c_str());
    DeleteFileW(ftsDstWalFile.c_str());
    DeleteFileW(ftsDstShmFile.c_str());
    DeleteFileW(wsDstFile.c_str());
    DeleteFileW(dbBackPath.c_str());
    DeleteFileW(dstWalFile.c_str());
    DeleteFileW(dstShmFile.c_str());

	return true;
}

void AndroidGetFriendInfo(sqlite3 *db, const std::string &account, bool isRecovery)
{
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return;
	}

	int				iRet;
	AccountInfo		*pAccountInfo = &g_accountInfo[account];
	sqlite3_stmt	*stmt;

    std::string sql;
    if (!isRecovery)
    {
        sql = "select userName, alias, nickName, conRemark, type from rcontact where verifyFlag = 0 and type != 4 and type != 33 and username != ''";
    }
    else
    {
        sql = "select userName, alias, nickName, conRemark, type from rcontact_694F69B9_93B9_480a_B23C_F6D221EEAF39 where verifyFlag = 0 and type != 4 and type != 33 and username != ''";
    }

	iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), -1, &stmt, NULL);
	if (SQLITE_OK != iRet)
	{
		return;
	}

    // 2017.10.28 
    // friendInfo.alias - 系统分配微信号
    // friendInfo.account - 真实微信号
    // 真实微信号可能为空，此时使用系统分配微信号

	while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		const char		*lpPointer;
		int				iSize;
		FriendInfo		friendInfo;

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
		if (lpPointer)
		{
            friendInfo.alias = lpPointer;
		}

        iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
        lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
        if (lpPointer)
        {
            friendInfo.account = lpPointer;
        }
        if (friendInfo.account.empty())
        {
            friendInfo.account = friendInfo.alias;
        }

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 2);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 2);
		if (lpPointer)
		{
            friendInfo.nickName = lpPointer;
		}

		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 3);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 3);
		if (lpPointer)
		{
            friendInfo.remark = lpPointer;
		}

		friendInfo.groupId = 0;
		friendInfo.iType = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 4);

        // 群聊
        if (std::string::npos != friendInfo.alias.rfind("@chatroom"))
        {
            friendInfo.groupName = friendInfo.alias;

            if (friendInfo.nickName.empty() &&
                friendInfo.remark.empty() &&
                !friendInfo.account.empty())
            {
                sqlite3_stmt *stmt2 = NULL;
                std::string sql = "select displayname from chatroom where chatroomname = '" + friendInfo.account + "'";
                
                if (SQLITE_OK == Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), -1, &stmt2, NULL))
                {
                    if (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt2))
                    {
                        iSize = Sqlite3Helper.sqlite3_column_bytes(stmt2, 0);
                        lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt2, 0);
                        if (lpPointer)
                        {
                            friendInfo.nickName = lpPointer;
                        }
                    }
                    Sqlite3Helper.sqlite3_finalize(stmt2);
                }

            }
        }

        if (!isRecovery)
        {
            pAccountInfo->friendList.push_back(friendInfo);
        }
        else
        {
            pAccountInfo->recoveryFriendList.push_back(friendInfo);
        }
	}

	Sqlite3Helper.sqlite3_finalize(stmt);
}

void AndroidGetRecoveryFriendInfo(sqlite3 *db, const std::string &account)
{
    AndroidGetFriendInfo(db, account, true);
}

void AndroidGetChatHistory(sqlite3 *db, const std::string &account)
{
	std::string chatAccount;
	AccountInfo *pAccount = &g_accountInfo[account];

	//get friend chat history
	//
	std::list<FriendInfo>::iterator friendItera;
	for (friendItera = pAccount->friendList.begin(); friendItera != pAccount->friendList.end(); friendItera++)
	{
		AndroidGetSingleChatHistory(db, account, *friendItera, 0);
	}
}

void AndroidGetRecoveryChatHistory(sqlite3 *db, const std::string &account)
{
    std::string chatAccount;
    AccountInfo *pAccount = &g_accountInfo[account];

    //get friend chat history
    //
    std::list<FriendInfo>::iterator friendItera;
    for (friendItera = pAccount->friendList.begin(); friendItera != pAccount->friendList.end(); friendItera++)
    {
        AndroidGetSingleChatHistory(db, account, *friendItera, 0, true);
    }
}

void AndroidGetSingleChatHistory(sqlite3 *db,
                                 const std::string &account,
                                 const FriendInfo &friendInfo,
                                 int iType,
                                 bool isRecovery)
{
	std::string		hash;
	std::string     tableName;
	std::string		sql;
	std::string		partname;

	int				iRet;
	sqlite3_stmt	*stmt;

	//check if table exists
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

	//get account chat history
	ChatInfo		chatInfo;

	switch(iType)
	{
	case 0:
        {
            if (!isRecovery)
            {
                sql = "SELECT content, datetime(createTime/1000,'unixepoch','localtime'), type, isSend, imgPath FROM message WHERE talker = '"\
                    + friendInfo.alias  + "' ORDER BY createTime";
            }
            else
            {
                sql = "SELECT content, datetime(createTime/1000,'unixepoch','localtime'), type, isSend, imgPath FROM message_694F69B9_93B9_480a_B23C_F6D221EEAF39 WHERE talker = '"\
                    + friendInfo.alias  + "' ORDER BY createTime";
            }
        }
		break;
	default:
		break;
	}

	iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
	if (SQLITE_OK != iRet)
		return; 

	while(SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		ChatHistory	chatHistory;
		const char *lpPointer;
		int  iSize;

		//message
		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		if (iSize > 0)
		{
			char* szTmp = new char[iSize + 1]();	
			if (szTmp)
			{
				lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
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
                chatHistory.message = szTmp;
				delete [] szTmp;
			}
		}
		

		//time
		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
		if (lpPointer)
		{
            chatHistory.time = lpPointer;
		}

		//type
		int msgType = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 2);


		//issend
		chatHistory.isSend = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 3);
        chatHistory.senderUin = chatHistory.isSend ? account : friendInfo.account;
        if (chatHistory.isSend)
        {
            chatHistory.senderName = account;
            if (g_accountInfoEx.end() != g_accountInfoEx.find(account))
            {
                if (!g_accountInfoEx[account].nickName.empty())
                {
                    chatHistory.senderName = g_accountInfoEx[account].nickName;
                }
            }
        }
        else
        {
            chatHistory.senderName = friendInfo.remark;
            if (chatHistory.senderName.empty())
            {
                chatHistory.senderName = friendInfo.nickName;
            }
        }

        if (1 != chatHistory.isSend && (10000 == msgType || 570425393 == msgType))
        {
            chatHistory.senderName = StringConvertor::AcsiiToUtf8("系统消息");
        }

        ChatHistory chatHistoryBack = chatHistory;

        // 群聊
        if (1 != chatHistory.isSend && !friendInfo.groupName.empty())
        {
            chatHistory.senderUin.clear();
            chatHistory.senderName.clear();

            std::string str = ":";
            size_t pos = chatHistory.message.find(str);
            if (std::string::npos != pos && (chatHistory.message.size() - 1) != pos)
            {
                std::string realChatAccount = chatHistory.message.substr(0, pos);
                if ('\n' == chatHistory.message[pos + 1])
                {
                    chatHistory.message = chatHistory.message.substr(pos + str.size() + 1);
                }
                else
                {
                    chatHistory.message = chatHistory.message.substr(pos + str.size());
                }

                chatHistory.senderUin = realChatAccount;

                if (10000 == msgType || 570425393 == msgType)
                {
                    chatHistory.senderName = StringConvertor::AcsiiToUtf8("系统消息");
                }
                else
                {
                    sqlite3_stmt *stmt = NULL;
                    std::string sql = "select alias, conRemark, nickName from rcontact where username == '" + realChatAccount +"'";

                    if (SQLITE_OK == Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.size(), &stmt, NULL))
                    {
                        if (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
                        {
                            const char		*lpPointer;
                            int				iSize;
                            FriendInfo		friendInfo;

                            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
                            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
                            if (lpPointer)
                            {
                                if (!std::string(lpPointer).empty())
                                {
                                    chatHistory.senderUin = lpPointer;
                                }
                            }

                            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
                            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
                            if (lpPointer)
                            {
                                chatHistory.senderName = lpPointer;
                            }

                            iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 2);
                            lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 2);
                            if (lpPointer)
                            {
                                if (chatHistory.senderName.empty())
                                {
                                    chatHistory.senderName = lpPointer;
                                }
                            }
                        }

                        Sqlite3Helper.sqlite3_finalize(stmt);
                    }
                }
            }
        }

        if (chatHistory.senderUin.empty())
        {
            chatHistory = chatHistoryBack;
        }

		//file path
		string	filePath;
		int		pos = 0;
		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 4);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 4);
		if (lpPointer)
            filePath =  lpPointer;

		// 需要解析 XML 格式内容
		string  strXml  = chatHistory.message;
		const char *szXml = strXml.c_str();
		tinyxml2::XMLElement  *element = NULL;
		tinyxml2::XMLDocument  doc;
		if ( msgType == 42 || msgType == 48 || msgType == 49 || msgType == 419430449 )
		{
			doc.Parse(szXml);													
			tinyxml2::XMLHandle docHandle( &doc ); 
			tinyxml2::XMLHandle xmlHandle = docHandle.FirstChildElement("msg");
			element = xmlHandle.ToElement();
		}

		// 微信数据在手机本地存放的子目录
		string microMsgDir = "\\Tencent\\MicroMsg\\";
		switch (msgType)
		{
		case 1:
			{
				chatHistory.msgType = WEIXIN_MSG_TEXT;//文字消息
				break;
			}

		// 经分析发现，图片附件按如下规律存储:
		// 例如：imgPath  = "THUMBNAIL_DIRPATH://th_b0d74651da0af63ba85b7a8fec1b9bf1"
		// 则，  filePath =  "b0\\d7\\th_b0d74651da0af63ba85b7a8fec1b9bf1"
		// filePath 存储于 microMsgDir + account + "\\image2\\" 目录下
		case 3:
			{
				chatHistory.msgType = WEIXIN_MSG_PIC;
				chatHistory.message = "[图片]";
				if ( (pos = filePath.find("th_")) != string::npos )
				{
					string thumbPath = filePath.substr(pos);
					filePath = filePath.substr(pos);
					filePath = filePath.substr(3);
					string temp = filePath;
					string fileDir1 = temp.substr(0, 2);
					temp = temp.substr(2);
					string fileDir2 = temp.substr(0, 2);
                    thumbPath = microMsgDir + account + "\\image2\\" + fileDir1 + "\\" + fileDir2 + "\\" +  thumbPath;
                    chatHistory.thumbPath = StringConvertor::AcsiiToUtf8(thumbPath);
					//如果是发送，则只保存缩略图
					if ( chatHistory.isSend == 0)
					{
						filePath += ".jpg";
                        chatHistory.filePath = microMsgDir + account + "\\image2\\" + fileDir1 + "\\" + fileDir2 + "\\" + filePath;
                        chatHistory.filePath = StringConvertor::AcsiiToUtf8(chatHistory.filePath);
                    }
				}
				break;
			}
		
		// 经分析发现，语音附件按如下规律存储:
		// 例如: imgPath  =  "3716530617160e48b872676104"，则先取imgPath的MD5值hash
		//       hash     =  "4EF9334E7CCAA36236E37892EF831171"
		// 得出: filePath =  "\\4E\\F9\\msg_3716530617160e48b872676104.amr;
		// filePath 存储于 microMsgDir + account + "\\voice2\\" 目录下
		case 34:
			{
				chatHistory.msgType = WEIXIN_MSG_AUDIO;
				string duration = chatHistory.message;
				GetMD5(filePath, hash);
				string fileDir1 = hash.substr(0, 2);
				hash = hash.substr(2);
				string fileDir2 = hash.substr(0, 2);
				string fileName = "msg_" + filePath + ".amr";

                filePath = microMsgDir + account + "\\voice2\\" + fileDir1 + "\\" + fileDir2 + "\\" + fileName;

				//语音文件不能直接播放，需要添加文件头
                chatHistory.filePath = StringConvertor::AcsiiToUtf8(filePath);
                int pos = 0;
				if ( (pos = duration.find(':')) != string::npos )
				{
					duration = duration.substr(pos + 1);
					if ( (pos = duration.find(':')) != string::npos )
					{
						duration = duration.substr(0, pos);
						//将毫秒转换成秒
						if ( duration.length() > 3 )
						{
							duration = duration.insert(duration.length() - 3, 1, '.');
						}
						else 
						{
							while (duration.length() < 4)
							{
								duration = "0" + duration;
							}
							duration = duration.insert(duration.length() - 3, 1, '.');
						}
						chatHistory.duration = duration;
					}
				}
				chatHistory.message = "[语音] 时长:" + duration + "秒";
                chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
                break;
			}
		case 42:
			{
				chatHistory.msgType = WEIXIN_MSG_CARD;//名片
				if ( element != NULL )
				{
					string userName;
					const char* elementTmp = element->Attribute("username");
					if (elementTmp)
					{
						userName.append(elementTmp);
					}
					string nickName;
					elementTmp = element->Attribute("nickname");
					if (elementTmp)
					{
						nickName.append(elementTmp);
					}
					string alias;
					elementTmp = element->Attribute("alias");
					if (elementTmp)
					{
						alias.append(elementTmp);
					}
					string province;
					elementTmp = element->Attribute("province");
					if (elementTmp)
					{
						province.append(elementTmp);
					}
					
					string city;
					elementTmp = element->Attribute("city");
					if (elementTmp)
					{
						city.append(elementTmp);
					}
					string sex;
					elementTmp = element->Attribute("sex");
					if (elementTmp)
					{
						sex.append(elementTmp);
						if (sex == string("1"))
						{
							sex = "男";
						}
						else
						{
							sex = "女";
						}
					}

                    string str = "[名片] ";
                    chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                    str = "账号:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += userName;
                    str = ",别名:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += alias;
                    str = ",昵称:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += nickName;
                    str = ",所在地:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += province;
                    chatHistory.message += city;
                    str = ",性别:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += StringConvertor::AcsiiToUtf8(sex);

					chatHistory.contactAccount	= userName;
					chatHistory.contactNickName = nickName;
					chatHistory.contactCity		= province + city;
					chatHistory.contactSex		= StringConvertor::AcsiiToUtf8(sex);
				}
				break;
			}
		case 43:
			{
				chatHistory.msgType = WEIXIN_MSG_VIDEO;
				string duration = chatHistory.message;
				filePath += ".mp4";
				chatHistory.filePath = microMsgDir + account + "\\video\\" + filePath;
				int pos = 0;
				if ( (pos = duration.find(':')) != string::npos )
				{
					duration = duration.substr(pos + 1);
					if ( (pos = duration.find(':')) != string::npos )
					{
						duration = duration.substr(0, pos);
						chatHistory.duration = duration;
					}
				}

				chatHistory.message = "[视频] 时长:" + duration + "秒";
                chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
				break;
			}
		case 47:
			{
				chatHistory.msgType = WEIXIN_DYNAMIC_FACE;
				chatHistory.message = "[动态表情]";
                chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
                break;
			}
		case 48:
			{
				chatHistory.msgType = WEIXIN_MSG_MAP;//地理位置
				if ( element != NULL )
				{
					element = element->FirstChildElement("location");
					if (element)
					{
						string latitude;
						const char* elementTmp = element->Attribute("x");
						if (elementTmp)
						{
							latitude.append(elementTmp);
						}
						string longitude;
						elementTmp = element->Attribute("y");
						if (elementTmp)
						{
							longitude.append(elementTmp);
						}
						string label;
						elementTmp = element->Attribute("label");
						if (elementTmp)
						{
                            label.append(elementTmp);
						}

                        string str = "[地图位置] ";
                        chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += label;
                        str = " 纬度:";
                        chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += latitude;
                        str = ", 经度:";
                        chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += longitude;

                        chatHistory.location  = label;
						chatHistory.latitude  = latitude;
						chatHistory.longitude = longitude;
					}
					
				}
				break;
			}
        case 49:
            {
                chatHistory.msgType = WEIXIN_MSG_TEXT;
                if ( element != NULL )
                {
                    tinyxml2::XMLElement *parentElement = element->FirstChildElement("appmsg");
                    if (parentElement)
                    {
                        tinyxml2::XMLElement *childElement = parentElement->FirstChildElement("title");
                        if (childElement && childElement->GetText())
                        {
                            chatHistory.message = childElement->GetText();
                        }

                        childElement = parentElement->FirstChildElement("des");
                        if (childElement && childElement->GetText())
                        {
                            chatHistory.message += std::string("\n") + childElement->GetText();
                        }

                        childElement = parentElement->FirstChildElement("url");
                        if (childElement && childElement->GetText())
                        {
                            chatHistory.message += std::string("\n") + childElement->GetText();
                        }
                    }
                }
            break;
            }
		case 50:
			{
				chatHistory.msgType = WEIXIN_MSG_CALL;
				chatHistory.message = "[语音通话、视频通话]";
                chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
                break;
			}
		case 62:
			{
				chatHistory.msgType = WEIXIN_MSG_PREVIDEO;
				string duration = chatHistory.message;
				filePath += ".mp4";
				chatHistory.filePath = microMsgDir + account + "\\video\\" + filePath;
                int pos = 0;
				if ( (pos = duration.find(':')) != string::npos )
				{
					duration = duration.substr(pos + 1);
					if ( (pos = duration.find(':')) != string::npos )
					{
						duration = duration.substr(0, pos);
						chatHistory.duration = duration;
					}
				}
				chatHistory.message = "[即时拍摄视频] 时长:" + duration + "秒";
                chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
                break;
			}
        case 419430449:
            {
                chatHistory.msgType = WEIXIN_MSG_TEXT;
                int paysubtype = 0;
                std::string pay_memo;
                std::string money;
                std::string str;
                const char *data = NULL;
                size_t pos = std::string::npos;
                chatHistory.message.clear();

                if (element != NULL)
                {
                    element = element->FirstChildElement("appmsg");
                    if (element)
                    {
                        element = element->FirstChildElement("wcpayinfo");
                        if (element)
                        {
                            tinyxml2::XMLElement *childElement = element->FirstChildElement("paysubtype");
                            if (childElement)
                            {
                                childElement->QueryIntText(&paysubtype);
                            }

                            childElement = element->FirstChildElement("feedesc");
                            if (childElement)
                            {
                                data = childElement->GetText();
                            }
                            
                            // 两种格式的人民币符号
                            // ¥ 0xC2 0xA5
                            // ￥0xEF 0xBF 0xA5
                            if (data && strlen(data) > 2)
                            {
                                if (0xC2 == (unsigned char)data[0] && 0xA5 == (unsigned char)data[1])
                                {
                                    str = std::string(data + 2);

                                    pos = str.find(']', 0);
                                    if (std::string::npos != pos)
                                    {
                                        money = str.substr(0, pos);
                                    }
                                    else
                                    {
                                        money = str;
                                    }
                                }
                            }

                            if (money.empty())
                            {
                                if (data && strlen(data) > 3)
                                {
                                    if (0xEF == (unsigned char)data[0] && 0xBF == (unsigned char)data[1] && 0xA5 == (unsigned char)data[2])
                                    {
                                        str = std::string(data + 3);

                                        pos = str.find(']', 0);
                                        if (std::string::npos != pos)
                                        {
                                            money = str.substr(0, pos);
                                        }
                                        else
                                        {
                                            money = str;
                                        }
                                    }
                                }
                            }

                            childElement = element->FirstChildElement("pay_memo");
                            if (childElement)
                            {
                                data = childElement->GetText();
                                pay_memo = data ? std::string(data) : "";
                            }
                        }
                    }
                }

                if (!money.empty())
                {
                    std::string sender;

                    if (chatHistory.isSend)
                    {
                        sender = friendInfo.remark;
                        if (sender.empty())
                        {
                            sender = friendInfo.nickName;
                        }

                        if (sender.empty())
                        {
                            sender = friendInfo.account;
                        }
                    }
                    else
                    {
                        sender = account;
                        if (g_accountInfoEx.end() != g_accountInfoEx.find(account))
                        {
                            if (!g_accountInfoEx[account].nickName.empty())
                            {
                                sender = g_accountInfoEx[account].nickName;
                            }
                        }
                    }

                    if (1 == paysubtype)
                    {
                        chatHistory.message = StringConvertor::AcsiiToUtf8("向") +
                            sender +
                            StringConvertor::AcsiiToUtf8("转账\n金额：") +
                            money +
                            StringConvertor::AcsiiToUtf8("元");
                    }
                    else if (3 == paysubtype)
                    {
                        chatHistory.message = StringConvertor::AcsiiToUtf8("已收钱\n金额：") +
                            money +
                            StringConvertor::AcsiiToUtf8("元");
                    }

                    if (!pay_memo.empty())
                    {
                        chatHistory.message += StringConvertor::AcsiiToUtf8("\n备注：") + pay_memo;
                    }
                }
                break;
            }
            case 10000:
                {
                    chatHistory.msgType = WEIXIN_MSG_TEXT;

                    // 微信红包
                    // 1、发给别人的，别人已领取
                    // 2、别人发给自己的，自己已领取

                    size_t pos1 = std::string::npos;
                    size_t pos2 = std::string::npos;
                    std::string sendid;
                    std::string sign;
                    std::string ver;
                    std::string content;
                    std::string findStr1 = "<img src=\"SystemMessages_HongbaoIcon.png\"/>  ";
                    std::string findStr2 = "<_wc_custom_link_";

                    pos1 = chatHistory.message.find(findStr1);
                    if (std::string::npos != pos1)
                    {
                        pos2 = chatHistory.message.find(findStr2, findStr1.size());
                        if (std::string::npos != pos2)
                        {
                            content = chatHistory.message.substr(findStr1.size(), pos2 - findStr1.size());
                        }

                        pos1 = chatHistory.message.find("sendid=", pos2 + findStr2.size());
                        if (std::string::npos != pos1)
                        {
                            pos2 = chatHistory.message.find('&', pos1 + 6);
                            if (std::string::npos != pos2)
                            {
                                sendid = chatHistory.message.substr(pos1, pos2 - pos1);
                                
                                pos1 = chatHistory.message.find("sign=", pos2 + 1);
                                if (std::string::npos != pos1)
                                {
                                    pos2 = chatHistory.message.find('&', pos1 + 5);
                                    if (std::string::npos != pos2)
                                    {
                                        sign = chatHistory.message.substr(pos1, pos2 - pos1);

                                        pos1 = chatHistory.message.find("ver=", pos2 + 1);
                                        if (std::string::npos != pos1)
                                        {
                                            pos2 = chatHistory.message.find("\">", pos1 + 4);
                                            if (std::string::npos != pos2)
                                            {
                                                ver = chatHistory.message.substr(pos1, pos2 - pos1);

                                                pos1 = chatHistory.message.find("</_wc_custom_link_>", pos2 + 2);
                                                if (std::string::npos != pos1)
                                                {
                                                    content += chatHistory.message.substr(pos2 + 2, pos1 - pos2 - 2);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if (!chatHistory.isSend)
                        {
                            if (!sendid.empty() && !sign.empty() && !ver.empty())
                            {
                                std::string receiveTime;
                                char moneyBuffer[128] = {0};
                                double receiveMoney = 0.0;

                                std::string sql = "select receiveAmount, datetime(receiveTime / 1000, 'unixepoch', 'localtime') from WalletLuckyMoney where "
                                                  "mNativeUrl = 'wxpay://c2cbizmessagehandler/hongbao/receivehongbao?msgtype=1&channelid=1&" + sendid +
                                                  "&sendusername=" + friendInfo.alias + "&" + ver + "&" + sign + "'";
                                sqlite3_stmt *stmt = NULL;

                                if (SQLITE_OK == Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.size(), &stmt, NULL))
                                {
                                    if (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
                                    {
                                        const char		*lpPointer;
                                        int				iSize;

                                        receiveMoney = Sqlite3Helper.sqlite3_column_double(stmt, 0) / 100;
                                        iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
                                        lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
                                        receiveTime = lpPointer != NULL ? std::string(lpPointer) : "";
                                        StringCbPrintfA(moneyBuffer, sizeof(moneyBuffer), "%.2f", receiveMoney);

                                        chatHistory.message = content + StringConvertor::AcsiiToUtf8("\n金额：") +
                                            std::string(moneyBuffer) + StringConvertor::AcsiiToUtf8("元\n领取时间：") + receiveTime;
                                    }

                                    Sqlite3Helper.sqlite3_finalize(stmt);
                                }
                            }
                        }
                        
                    }
                }
                break;
		default:
			{
				chatHistory.msgType = WEIXIN_MSG_OTHER;
				break;
			}
		}

		chatInfo.chatList.push_back(chatHistory);
	}

    if (!isRecovery)
    {
        g_accountInfo[account].chatMap[friendInfo.account + "-" + partname] = chatInfo;
    }
    else
    {
        g_accountInfo[account].recoveryChatMap[friendInfo.account + "-" + partname] = chatInfo;
    }

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
                    accountInfo.weixinNumber = (stringValue != NULL) ? stringValue : "";
                }

                if (stringValue)
                {
                    free(stringValue);
                    stringValue = NULL;
                }

                plistNode2 = plist_array_get_item(plistNode, 3);
                if (PLIST_STRING == plist_get_node_type(plistNode2))
                {
                    plist_get_string_val(plistNode2, &stringValue);
                    accountInfo.nickName = (stringValue != NULL) ? stringValue : "";
                }

                if (stringValue)
                {
                    free(stringValue);
                    stringValue = NULL;
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
	int flag = -1;

	if (!PathFileExistsW(g_accountInfo[account].dbPath.c_str()))
	{
		return bRet;
	}

	if (PathFileExistsW(g_accountInfo[account].firentDbPath.c_str()))
	{
		iRet = sqliteTmp.sqlite3_open16(g_accountInfo[account].firentDbPath.c_str(), &db);
		if (SQLITE_OK != iRet)
		{
			iRet = Sqlite3Helper.sqlite3_open16(g_accountInfo[account].dbPath.c_str(), &db);
			if (SQLITE_OK != iRet)
			{
				return bRet;
			}
			else
			{
				flag = 2;
			}
		}
		else
		{
			flag = 1;
		}
	}
	else
	{
		iRet = Sqlite3Helper.sqlite3_open16(g_accountInfo[account].dbPath.c_str(), &db);
		if (SQLITE_OK != iRet)
		{
			return bRet;
		}
		else
		{
			flag = 2;
		}
	}

    //get friend info
    IosGetFriendInfo(db, account, flag);

    if (1 == flag)
    {
        Sqlite3Helper.sqlite3_close(db);
        db = NULL;
        iRet = Sqlite3Helper.sqlite3_open16(g_accountInfo[account].dbPath.c_str(), &db);
        if (SQLITE_OK != iRet)
        {
            return bRet;
        }
    }

	//get chat history
	//
	//IosGetChatHistory(db, account);
    IosGetSingleChatHistoryWithoutFriendAccount(db, account, 0);
    Sqlite3Helper.sqlite3_close(db);

    IosGetRecoveryFriendInfo(account);
    IosGetRecoveryChatHistory(account);
    
	return true;
}

void IosGetRecoveryFriendInfo(const std::string &account)
{
    if (g_accountInfo.end() == g_accountInfo.find(account))
    {
        return;
    }

    if (g_accountInfo[account].firentDbPath.empty())
    {
        return;
    }

    std::wstring dbBackPath;
    std::wstring srcWalFile;
    std::wstring dstWalFile;
    std::wstring srcShmFile;
    std::wstring dstShmFile;

    dbBackPath = g_accountInfo[account].firentDbPath + L"_back";
    if (!CopyFileW(g_accountInfo[account].firentDbPath.c_str(), dbBackPath.c_str(), FALSE))
    {
        return;
    }

    srcWalFile = g_accountInfo[account].firentDbPath + L"-wal";
    dstWalFile = dbBackPath + L"-wal";
    if (PathFileExistsW(srcWalFile.c_str()))
    {
        if (!CopyFileW(srcWalFile.c_str(), dstWalFile.c_str(), FALSE))
        {
            return;
        }
    }

    srcShmFile = g_accountInfo[account].firentDbPath + L"-shm";
    dstShmFile = dbBackPath + L"-shm";
    if (PathFileExistsW(srcShmFile.c_str()))
    {
        if (!CopyFileW(srcShmFile.c_str(), dstShmFile.c_str(), FALSE))
        {
            return;
        }
    }

    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    tableInfo.recoveryAppType = IOS_RECOVERY_WEIXIN;
    tableInfo.srcTableName = L"Friend";
    tableInfo.dstTableName = L"Friend_694F69B9_93B9_480a_B23C_F6D221EEAF39";
    tableInfo.mainColumnName = L"dbContactRemark";
    tableInfo.uniqueColumnName = L"userName";
    targetTableInfos.push_back(tableInfo);
    sqlite3 *db = NULL;

    SQLiteDataRecovery *recoveryHandler = CreateRecoveryClass();
    if (recoveryHandler)
    {
        recoveryHandler->StartRecovery(dbBackPath, targetTableInfos);
        DestoryRecoveryClass(recoveryHandler);
        recoveryHandler = NULL;
    }

    if (SQLITE_OK == Sqlite3Helper.sqlite3_open16(dbBackPath.c_str(), &db))
    {
        IosGetFriendInfoEx(db, account, StringConvertor::UnicodeToAcsii(tableInfo.dstTableName), true);
        Sqlite3Helper.sqlite3_close(db);
    }

    DeleteFileW(dbBackPath.c_str());
    DeleteFileW(dstWalFile.c_str());
    DeleteFileW(dstShmFile.c_str());
}

void IosGetRecoveryChatHistory(const std::string &account)
{
    if (g_accountInfo.end() == g_accountInfo.find(account))
    {
        return;
    }

    if (g_accountInfo[account].dbPath.empty())
    {
        return;
    }

    std::wstring dbBackPath;
    std::wstring srcWalFile;
    std::wstring dstWalFile;
    std::wstring srcShmFile;
    std::wstring dstShmFile;
    std::wstring ftsDbPath;
    std::wstring ftsDbBackPath;
    std::wstring ftsSrcWalFile;
    std::wstring ftsDstWalFile;
    std::wstring ftsSrcShmFile;
    std::wstring ftsDstShmFile;

    std::list<TargetTableInfo> targetTableInfos;
    TargetTableInfo tableInfo;
    std::string sql = "select tbl_name from sqlite_master where tbl_name like 'fts_message_table_%_content'";
    sqlite3_stmt *stmt = NULL;
    sqlite3 *ftsDb = NULL;

    dbBackPath = g_accountInfo[account].dbPath + L"_back";
    if (!CopyFileW(g_accountInfo[account].dbPath.c_str(), dbBackPath.c_str(), FALSE))
    {
        return;
    }

    srcWalFile = g_accountInfo[account].dbPath + L"-wal";
    dstWalFile = dbBackPath + L"-wal";
    if (PathFileExistsW(srcWalFile.c_str()))
    {
        if (!CopyFileW(srcWalFile.c_str(), dstWalFile.c_str(), FALSE))
        {
            return;
        }
    }

    srcShmFile = g_accountInfo[account].dbPath + L"-shm";
    dstShmFile = dbBackPath + L"-shm";
    if (PathFileExistsW(srcShmFile.c_str()))
    {
        if (!CopyFileW(srcShmFile.c_str(), dstShmFile.c_str(), FALSE))
        {
            return;
        }
    }

    size_t pos = g_accountInfo[account].dbPath.rfind(L'\\');
    if (std::wstring::npos != pos)
    {
        ftsDbPath = g_accountInfo[account].dbPath.substr(0, pos);
        if (ftsDbPath.size() > 3)
        {
            if (L'B' == ftsDbPath[ftsDbPath.size() - 1] &&
                L'D' == ftsDbPath[ftsDbPath.size() - 2] &&
                L'\\' == ftsDbPath[ftsDbPath.size() - 3])
            {
                ftsDbPath =  ftsDbPath.substr(0, ftsDbPath.size() - 3) + L"\\fts\\fts_message.db";
                ftsDbBackPath = ftsDbPath + L"_back";

                if (PathFileExistsW(ftsDbPath.c_str()))
                {
                    if (!CopyFileW(ftsDbPath.c_str(), ftsDbBackPath.c_str(), FALSE))
                    {
                        return;
                    }

                    ftsSrcWalFile = ftsDbPath + L"-wal";
                    ftsDstWalFile = ftsDbBackPath + L"-wal";
                    if (PathFileExistsW(ftsSrcWalFile.c_str()))
                    {
                        if (!CopyFileW(ftsSrcWalFile.c_str(), ftsDstWalFile.c_str(), FALSE))
                        {
                            return;
                        }
                    }

                    ftsSrcShmFile = ftsDbPath + L"-shm";
                    ftsDstShmFile = ftsDbBackPath + L"-shm";
                    if (PathFileExistsW(ftsSrcShmFile.c_str()))
                    {
                        if (!CopyFileW(ftsSrcShmFile.c_str(), ftsDstShmFile.c_str(), FALSE))
                        {
                            return;
                        }
                    }

                    if (SQLITE_OK == Sqlite3Helper.sqlite3_open16(ftsDbBackPath.c_str(), &ftsDb))
                    {
                        if (SQLITE_OK == Sqlite3Helper.sqlite3_prepare(ftsDb, sql.c_str(), sql.length(), &stmt, NULL))
                        {
                            tableInfo.recoveryAppType = IOS_RECOVERY_WEIXIN;
                            tableInfo.mainColumnName = L"UsrName";
                            tableInfo.uniqueColumnName = L"UsrName";
                            tableInfo.srcTableName = L"fts_username_id";
                            tableInfo.dstTableName = L"fts_username_id_694F69B9_93B9_480a_B23C_F6D221EEAF39";
                            targetTableInfos.push_back(tableInfo);

                            while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
                            {
                                const wchar_t *data = (const wchar_t*)Sqlite3Helper.sqlite3_column_text16(stmt, 0);
                                if (data)
                                {
                                    tableInfo.recoveryAppType = IOS_RECOVERY_WEIXIN;
                                    tableInfo.srcTableName = std::wstring(data);
                                    tableInfo.dstTableName = std::wstring(data) + L"_694F69B9_93B9_480a_B23C_F6D221EEAF39";
                                    tableInfo.mainColumnName = L"c3Message";
                                    tableInfo.uniqueColumnName = L"c2CreateTime";
                                    targetTableInfos.push_back(tableInfo);
                                }
                            }

                            Sqlite3Helper.sqlite3_finalize(stmt);
                        }
                        Sqlite3Helper.sqlite3_close(ftsDb);
                    }

                    SQLiteDataRecovery *recoveryHandler = CreateRecoveryClass();
                    if (recoveryHandler)
                    {
                        recoveryHandler->StartRecovery(ftsDbBackPath, targetTableInfos);
                        DestoryRecoveryClass(recoveryHandler);
                        recoveryHandler = NULL;
                    }
                }
            }
        }
    }

    std::list<FriendInfo> &friendList = g_accountInfo[account].friendList;
    std::list<FriendInfo> &recoveryFriendList = g_accountInfo[account].recoveryFriendList;
    
    struct ChatAccountInfo
    {
        std::string md5;
        std::string tableName;
    };

    std::list<ChatAccountInfo> chatAccountInfoList;

    targetTableInfos.clear();
    tableInfo.recoveryAppType = IOS_RECOVERY_WEIXIN;
    tableInfo.mainColumnName = L"Type";
    tableInfo.uniqueColumnName = L"CreateTime";

    std::string md5String;
    std::wstring md5Wstring;
    for (std::list<FriendInfo>::iterator iter = friendList.begin();
        iter != friendList.end();
        ++iter)
    {
        GetMD5(iter->alias, md5String);
        md5Wstring = StringConvertor::AcsiiToUnicode(md5String);
        tableInfo.chatAccount = iter->alias;
        tableInfo.srcTableName = L"Chat_" + md5Wstring;
        tableInfo.dstTableName = L"Chat_" + md5Wstring + L"_694F69B9_93B9_480a_B23C_F6D221EEAF39";
        targetTableInfos.push_back(tableInfo);

        ChatAccountInfo chatAccountInfo;
        chatAccountInfo.md5 = md5String;
        chatAccountInfo.tableName = StringConvertor::UnicodeToAcsii(tableInfo.dstTableName);
        chatAccountInfoList.push_back(chatAccountInfo);
    }

    for (std::list<FriendInfo>::iterator iter = recoveryFriendList.begin();
        iter != recoveryFriendList.end();
        ++iter)
    {
        GetMD5(iter->alias, md5String);
        md5Wstring = StringConvertor::AcsiiToUnicode(md5String);
        tableInfo.srcTableName = L"Chat_" + md5Wstring;
        tableInfo.dstTableName = L"Chat_" + md5Wstring + L"_694F69B9_93B9_480a_B23C_F6D221EEAF39";
        targetTableInfos.push_back(tableInfo);

        ChatAccountInfo chatAccountInfo;
        chatAccountInfo.md5 = md5String;
        chatAccountInfo.tableName = StringConvertor::UnicodeToAcsii(tableInfo.dstTableName);
        chatAccountInfoList.push_back(chatAccountInfo);
    }

    SQLiteDataRecovery *recoveryHandler = CreateRecoveryClass();
    if (recoveryHandler)
    {
        recoveryHandler->StartRecovery(dbBackPath, targetTableInfos);
        DestoryRecoveryClass(recoveryHandler);
        recoveryHandler = NULL;
    }

    sqlite3 *db = NULL;
    if (SQLITE_OK != Sqlite3Helper.sqlite3_open16(dbBackPath.c_str(), &db))
    {
        return;
    }

    for (std::list<ChatAccountInfo>::iterator iter = chatAccountInfoList.begin();
        iter != chatAccountInfoList.end();
        ++iter)
    {
        IosGetSingleChatHistoryWithoutFriendAccountEx(db, account, iter->md5, iter->tableName, 0, true);
    }

    Sqlite3Helper.sqlite3_close(db);

    DeleteFileW(ftsDbBackPath.c_str());
    DeleteFileW(ftsDstWalFile.c_str());
    DeleteFileW(ftsDstShmFile.c_str());
    DeleteFileW(dbBackPath.c_str());
    DeleteFileW(dstWalFile.c_str());
    DeleteFileW(dstShmFile.c_str());
}

void IosDecryptFriendData(const char *data, int dataSize, FriendInfo &friendInfo)
{
    // 数据type定义
    // 0x0a-昵称
    // 0x12-微信号
    // 0x1a-备注
    // 0x22-备注拼音
    // 0x2a-备注拼音首字母

    do 
    {
        if (!data || dataSize < 0)
        {
            break;
        }

        int type = 0;
        const char *p = data;
        char *buffer = NULL;
        int bufferSize = 0;

        while (dataSize > 0)
        {
            type = *(unsigned char*)p;
            ++p;
            --dataSize;

            bufferSize = *(unsigned char*)p;
            ++p;
            --dataSize;

            if (bufferSize)
            {
                buffer = new char[bufferSize + 1]();
                if (!buffer)
                {
                    break;
                }

                memcpy(buffer, p, bufferSize);

                p += bufferSize;
                dataSize -= bufferSize;

                // 1 昵称
                if (0x0a == type)
                {
                    friendInfo.nickName = buffer;
                }
                // 2 微信号
                else if (0x12 == type)
                {
                    if (!std::string(buffer).empty())
                    {
                        friendInfo.account = buffer;
                    }
                }
                // 3 备注
                else if (0x1a == type)
                {
                    friendInfo.remark = buffer;
                }

                delete buffer;
                buffer = NULL;
                bufferSize = 0;
            }
        }

    } while (0);
}

void IosGetFriendInfoEx(sqlite3 *db, const std::string &account, const std::string &tableName, bool isRecovery)
{
    int	iRet = -1;
    AccountInfo	*pAccountInfo = &g_accountInfo[account];
    std::string sql;
    sqlite3_stmt *stmtTmp = NULL;

    // 2017.12.14
    // 发现自己也是自己的好友，同样可以聊天
    FriendInfo friendInfo;
    friendInfo.account = g_accountInfoEx[account].weixinNumber;
    friendInfo.alias = friendInfo.account;
    friendInfo.nickName = g_accountInfoEx[account].nickName;
    if (!friendInfo.account.empty())
    {
        pAccountInfo->friendList.push_back(friendInfo);
    }

    do 
    {
        if (!db)
        {
            break;
        }

        sql = "select userName, dbContactRemark, type from " + tableName + " where type != 4";
        iRet = sqliteTmp.sqlite3_prepare(db, sql.c_str(), sql.size(), &stmtTmp, NULL);
        if (SQLITE_OK != iRet)
        {
            break;
        }

        // 2017.10.28 
        // friendInfo.alias - 系统分配微信号
        // friendInfo.account - 真实微信号
        // 真实微信号可能为空，此时使用系统分配微信号

        while(SQLITE_ROW == sqliteTmp.sqlite3_step(stmtTmp))
        {
            FriendInfo friendInfo;
            friendInfo.alias = sqliteTmp.sqlite3_column_text(stmtTmp, 0) ? (char*)sqliteTmp.sqlite3_column_text(stmtTmp, 0) : "";
            friendInfo.account = friendInfo.alias;

            // 群聊
            int type = (int)sqliteTmp.sqlite3_column_int64(stmtTmp, 2);
            if (std::string::npos != friendInfo.alias.find("@chatroom"))
            {
                friendInfo.groupName = friendInfo.alias;
            }

            IosDecryptFriendData((const char*)sqliteTmp.sqlite3_column_blob(stmtTmp, 1), sqliteTmp.sqlite3_column_bytes(stmtTmp, 1), friendInfo);

            if (isRecovery)
            {
                if (!friendInfo.alias.empty())
                {
                    bool isExist = false;
                    for (std::list<FriendInfo>::iterator iter = pAccountInfo->friendList.begin();
                        iter != pAccountInfo->friendList.end();
                        ++iter)
                    {
                        if (friendInfo.alias == iter->alias)
                        {
                            isExist = true;
                            break;
                        }
                    }

                    if (!isExist)
                    {
                        pAccountInfo->recoveryFriendList.push_back(friendInfo);
                    }
                }
            }
            else
            {
                pAccountInfo->friendList.push_back(friendInfo);
            }
        }

        sqliteTmp.sqlite3_finalize(stmtTmp);

    } while (0);
}

void IosGetFriendInfo(sqlite3 *db, const std::string &account, int flag)
{
	if (!db)
		return;

	if (g_accountInfo.find(account) == g_accountInfo.end())
		return;

	if (2 == flag)
	{
		int				iRet;
		AccountInfo		*pAccountInfo = &g_accountInfo[account];
		sqlite3_stmt	*stmt;

		string tableName;
		string chatAccount;
		sqlite3_stmt	*stmtTmp;
		wstring dbFile = g_accountInfo[account].dbPath;
		iRet = sqliteTmp.sqlite3_open16(dbFile.c_str(), &db);
		if (SQLITE_OK != iRet)
			return;
		string sql = "select name from sqlite_master where type = 'table'";
		iRet = sqliteTmp.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmtTmp, NULL);
		if (SQLITE_OK != iRet)
			return;

		while(SQLITE_ROW == sqliteTmp.sqlite3_step(stmtTmp))
		{
			int size = sqliteTmp.sqlite3_column_bytes(stmtTmp, 0);
			const char * ptr = (const char *)sqliteTmp.sqlite3_column_blob(stmtTmp, 0);
			if (ptr)
			{
				tableName = ptr;
				int pos = tableName.find("Chat_");
				if (string::npos != pos)
				{
					FriendInfo		friendInfo;
					friendInfo.account = tableName.substr(pos + 5);
					pAccountInfo->friendList.push_back(friendInfo);
				}
				else 
					continue;
			}
		}
		sqliteTmp.sqlite3_finalize(stmtTmp);


		//////////////////////////////////////////////////////////////////////////
		char szSql[] = "SELECT UsrName, NickName FROM Friend";

		iRet = Sqlite3Helper.sqlite3_prepare(db, szSql, lstrlenA(szSql), &stmt, NULL);
		if (SQLITE_OK != iRet)
			return;

		std::string fAccount;
		std::string fNickName;
		while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
		{
			const char		*lpPointer;
			int				iSize;

			iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
			lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);

			if ( NULL == lpPointer )
			{
				fAccount = " ";
			}
			else
			{
				fAccount = lpPointer;
			}

			iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
			lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);

			if ( NULL == lpPointer )
			{
				fNickName = " ";
			}
			else
			{
				fNickName = lpPointer;
			}

			if (account.empty())
				continue;

			std::string hash;
			//GetMD5(account, hash);
			GetMD5(fAccount, hash);

			std::list<FriendInfo>::iterator friendItera;
			for (friendItera = pAccountInfo->friendList.begin(); friendItera != pAccountInfo->friendList.end(); friendItera++)
			{
				if (hash == (*friendItera).account)
				{
					(*friendItera).nickName = fNickName;
					break;
				}
			}
		}


		//////////////////////////////////////////////////////////////////////////
		char szSql2[] = "SELECT username FROM friend_meta";

		iRet = Sqlite3Helper.sqlite3_prepare(db, szSql2, lstrlenA(szSql2), &stmt, NULL);
		if (SQLITE_OK != iRet)
			return;

		std::string fUserName;
		while (SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
		{
			const char		*lpPointer;
			int				iSize;

			iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
			lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);

			if ( NULL == lpPointer )
			{
				fUserName = " ";
			}
			else
			{
				fUserName = lpPointer;
			}

			std::string hash;
			//GetMD5(account, hash);
			GetMD5(fUserName, hash);

			std::list<FriendInfo>::iterator friendItera;
			for (friendItera = pAccountInfo->friendList.begin(); friendItera != pAccountInfo->friendList.end(); friendItera++)
			{
				if (hash == (*friendItera).account)
				{
					if ((*friendItera).nickName.empty())
					{
						(*friendItera).nickName = fUserName;
					}

					break;
				}
			}
		}

		Sqlite3Helper.sqlite3_finalize(stmt);
	}
	else if (1 == flag)
	{
		IosGetFriendInfoEx(db, account, "Friend");
	}
}

void IosGetChatHistory(sqlite3 *db, const std::string &account)
{
	std::string chatAccount;
	AccountInfo *pAccount = &g_accountInfo[account];

	//get friend chat history
	//
    if (isHaveFriendInfos)
    {
        IosGetSingleChatHistoryWithoutFriendAccount(db, account, 0);
        return;
    }

	//std::list<FriendInfo>::iterator friendItera;
	//for (friendItera = pAccount->friendList.begin(); friendItera != pAccount->friendList.end(); friendItera++)
	//{
	//	chatAccount = friendItera->account;
 //       IosGetSingleChatHistory(db, account, chatAccount, 0);
 //   }
}

void IosGetSingleChatHistoryWithoutFriendAccountEx(sqlite3 *db,
                                                   const std::string &account,
                                                   const std::string &chatAccountMd5,
                                                   const std::string &tableName,
                                                   int iType,
                                                   bool isRecovery)
{
    if (!db)
    {
        return;
    }

    std::string groupName;
    std::string partname;
    std::string	hash = chatAccountMd5;
    std::string chatAccount;
    std::string senderUin;
    std::string senderName;
    ChatInfo chatInfo;
    bool isTroopChat = false;
    string troopChatAccount;
    int iRet = -1;
    std::string sql = "SELECT Message, datetime(CreateTime,'unixepoch','localtime'), Type, Des, MesLocalID FROM " + tableName;
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *stmtTmp = NULL;
    sqlite3_stmt *friendStmt = NULL;
    sqlite3 *friendDb = NULL;

    if (!g_accountInfo[account].firentDbPath.empty())
    {
        iRet = Sqlite3Helper.sqlite3_open16(g_accountInfo[account].firentDbPath.c_str(), &friendDb);
        if (SQLITE_OK != iRet)
        {
            friendDb = NULL;
        }
    }

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

    std::list<FriendInfo>::iterator friendItera;
    AccountInfo	*pAccountInfo = &g_accountInfo[account];
    bool isMatch = false;
    for (friendItera = pAccountInfo->friendList.begin(); friendItera != pAccountInfo->friendList.end(); ++friendItera)
    {
        std::string friendAccountMd5;
        GetMD5((*friendItera).alias, friendAccountMd5);
        if (chatAccountMd5 == friendAccountMd5)
        {
            isMatch = true;
            chatAccount = (*friendItera).account;
            groupName = (*friendItera).groupName;
            senderUin = chatAccount;
            senderName = (*friendItera).remark;
            if (senderName.empty())
            {
                senderName = (*friendItera).nickName;
            }
            break;
        }
    }

    if (!isMatch)
    {
        if (isRecovery && !pAccountInfo->recoveryFriendList.empty())
        {
            for (friendItera = pAccountInfo->recoveryFriendList.begin(); friendItera != pAccountInfo->recoveryFriendList.end(); ++friendItera)
            {
                std::string friendAccountMd5;
                GetMD5((*friendItera).alias, friendAccountMd5);

                if (chatAccountMd5 == friendAccountMd5)
                {
                    isMatch = true;
                    chatAccount = (*friendItera).account;
                    groupName = (*friendItera).groupName;
                    senderUin = chatAccount;
                    senderName = (*friendItera).remark;
                    if (senderName.empty())
                    {
                        senderName = (*friendItera).nickName;
                    }
                    break;
                }
            }

            if (!isMatch)
            {
                chatAccount = chatAccountMd5;
                senderUin = chatAccountMd5;
                for (friendItera = pAccountInfo->recoveryFriendList.begin(); friendItera != pAccountInfo->recoveryFriendList.end(); ++friendItera)
                {
                    if (senderUin == (*friendItera).account)
                    {
                        isMatch = true;
                        groupName = (*friendItera).groupName;
                        senderName = (*friendItera).remark;
                        if (senderName.empty())
                        {
                            senderName = (*friendItera).nickName;
                        }
                        break;
                    }
                }
            }
        }

        if (!isMatch)
        {
            chatAccount = chatAccountMd5;
            senderUin = chatAccountMd5;
            for (friendItera = pAccountInfo->friendList.begin(); friendItera != pAccountInfo->friendList.end(); ++friendItera)
            {
                if (senderUin == (*friendItera).account)
                {
                    isMatch = true;
                    groupName = (*friendItera).groupName;
                    senderName = (*friendItera).remark;
                    if (senderName.empty())
                    {
                        senderName = (*friendItera).nickName;
                    }
                    break;
                }
            }
        }
    }

    iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
    if (SQLITE_OK != iRet)
    {
        //Sqlite3Helper.sqlite3_close(db);
        if (friendDb)
        {
            Sqlite3Helper.sqlite3_close(friendDb);
        }

        return; 
    }

    while(SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
    {
        ChatHistory	chatHistory;
        const  char *lpPointer;
        string filePath;
        string thumbPath;
        stringstream sstr;
        int		iSize;
        int		msgType;		
        int 	mesLocalID;
        string  body_xml;
        const char *szXml = NULL;
        tinyxml2::XMLDocument  doc; 
        tinyxml2::XMLElement  *element = NULL;

        //message
        iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
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
                chatHistory.message = szTmp;
                delete [] szTmp;

                if ( isTroopChat )
                {
                    size_t iPos = chatHistory.message.find(":");
                    if (-1 != iPos)
                    {
                        troopChatAccount = chatHistory.message.substr(0, iPos);
                        if (chatHistory.message.size() > (iPos + 2))
                        {
                            chatHistory.message = chatHistory.message.substr(iPos + 2);
                        }
                    }   
                }
            }
        }

        //time
        iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
        lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
        if (lpPointer)
        {
            chatHistory.time = lpPointer;
        }

        //issend
        chatHistory.isSend = (Sqlite3Helper.sqlite3_column_int64(stmt, 3) == 0);
        if (chatHistory.isSend)
        {
            if (g_accountInfoEx.find(account) != g_accountInfoEx.end())
            {
                if (!g_accountInfoEx[account].nickName.empty())
                {
                    chatHistory.senderUin = g_accountInfoEx[account].nickName;
                }
            }

            if (chatHistory.senderUin.empty())
            {
                chatHistory.senderUin = account;
            }
        }
        else
        {
            if ( isTroopChat )
            {
                chatHistory.senderUin = troopChatAccount;
            }

            chatHistory.senderUin = senderUin;
            chatHistory.senderName = senderName;
        }

        //type
        msgType = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 2);

        ///////
        ChatHistory chatHistoryBack = chatHistory;

        // 群聊
        if (1 != chatHistory.isSend && !groupName.empty() && friendDb)
        {
            if (10000 == msgType)
            {
                chatHistory.senderName = StringConvertor::AcsiiToUtf8("系统消息");
            }
            else
            {
                chatHistory.senderUin.clear();
                chatHistory.senderName.clear();

                std::string str = ":";
                size_t pos = chatHistory.message.find(str);
                if (std::string::npos != pos && (chatHistory.message.size() - 1) != pos)
                {
                    std::string realChatAccount = chatHistory.message.substr(0, pos);
                    if ('\n' == chatHistory.message[pos + 1])
                    {
                        chatHistory.message = chatHistory.message.substr(pos + str.size() + 1);
                    }
                    else
                    {
                        chatHistory.message = chatHistory.message.substr(pos + str.size());
                    }

                    std::string sql = "select dbContactRemark from Friend where userName = '" + realChatAccount + "'";
                    iRet = Sqlite3Helper.sqlite3_prepare(friendDb, sql.c_str(), sql.size(), &friendStmt, NULL);
                    if (SQLITE_OK == iRet)
                    {
                        if (SQLITE_ROW == Sqlite3Helper.sqlite3_step(friendStmt))
                        {
                            FriendInfo friendInfo;
                            IosDecryptFriendData((const char*)Sqlite3Helper.sqlite3_column_blob(friendStmt, 0), Sqlite3Helper.sqlite3_column_bytes(friendStmt, 0), friendInfo);

                            chatHistory.senderUin = friendInfo.alias.empty() ? realChatAccount : friendInfo.alias;
                            chatHistory.senderName = friendInfo.remark;
                            if (chatHistory.senderName.empty())
                            {
                                chatHistory.senderName = friendInfo.nickName;
                            }
                        }

                        Sqlite3Helper.sqlite3_finalize(friendStmt);
                    }
                }
            }
        }

        if (chatHistory.senderUin.empty())
        {
            chatHistory = chatHistoryBack;
        }

        ///////

        if ( msgType == 34 ||
            msgType == 42 ||
            msgType == 48 ||
            msgType == 43 ||
            msgType == 62 ||
            msgType == 49 ||
            msgType == 47)
        {

            body_xml = chatHistory.message;
            szXml = body_xml.c_str();
            doc.Parse(szXml);
            tinyxml2::XMLHandle docHandle( &doc ); 
            tinyxml2::XMLHandle xmlHandle = docHandle.FirstChildElement("msg");
            element = xmlHandle.ToElement();
        }

        mesLocalID = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 4);	

        //1:文字、系统表情  3:图片  34:语音  42:发送名片 43:视频  62:录像、小视频
        //50:语音通话、视频通话  48:地图位置   10000:实时对讲
        switch (msgType)
        {
        case 1:
            {
                chatHistory.msgType = WEIXIN_MSG_TEXT;
                break;
            }

        case 3:
            {
                chatHistory.msgType = WEIXIN_MSG_PIC;
                chatHistory.message = "[图片]";
                sstr<<mesLocalID<<".pic";
                sstr>>filePath;
                sstr.clear();
                sstr<<mesLocalID<<".pic_thum";
                sstr>>thumbPath;
                filePath  = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Img\\" + hash + "\\" + filePath;
                thumbPath = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Img\\" + hash + "\\" + thumbPath;
                chatHistory.filePath  = filePath;
                chatHistory.thumbPath = thumbPath;
                break;
            }

        case 34:
            {
                chatHistory.msgType = WEIXIN_MSG_AUDIO;
                chatHistory.message = "[语音]";
                string strMesLocalID;
                sstr<<mesLocalID;
                sstr>>strMesLocalID;
                sstr.clear();

                //语音文件不能直接播放，需要添加文件头
                filePath = strMesLocalID + ".aud";
                filePath = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Audio\\" + hash + "\\" + filePath;

                chatHistory.filePath  = filePath;
                if ( element != NULL )
                {
                    element = element->FirstChildElement("voicemsg");
                    if ( element != NULL )
                    {
                        string duration;
                        const char* tmp = element->Attribute("voicelength");
                        if (tmp)
                        {
                            duration.append(tmp);
                        }
                        //将毫秒转换成秒
                        if ( duration.length() > 3 )
                        {
                            duration = duration.insert(duration.length() - 3, 1, '.');
                        }
                        else 
                        {
                            while (duration.length() < 4)
                            {
                                duration = "0" + duration;
                            }
                            if ( duration.length() > 3 )
                            {
                                duration = duration.insert(duration.length() - 3, 1, '.');
                            }
                        }
                        chatHistory.duration = duration;
                    }
                }
                break;
            }

        case 42:
            {
                chatHistory.msgType = WEIXIN_MSG_CARD;
                if ( element != NULL )
                {
                    string userName;
                    const char* elementTmp = element->Attribute("username");
                    if (elementTmp)
                    {
                        userName.append(elementTmp);
                    }
                    string nickName;
                    elementTmp = element->Attribute("nickname");
                    if (elementTmp)
                    {
                        nickName.append(elementTmp);
                    }
                    string city;
                    elementTmp = element->Attribute("city");
                    if (elementTmp)
                    {
                        city.append(elementTmp);
                    }
                    string sex;
                    elementTmp = element->Attribute("sex");
                    if (elementTmp)
                    {
                        sex.append(elementTmp);
                        if (sex == string("0"))
                        {
                            sex = "男";
                        }
                        else
                        {
                            sex = "女";

                        }
                    }

                    std::string str = "[名片] ";
                    chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                    str = "账号:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += userName;
                    str = ",昵称:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += nickName;
                    str = ",城市:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += city;
                    str = ",性别:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += StringConvertor::AcsiiToUtf8(sex);

                    chatHistory.message = string("[名片] ")+"账号:"+ userName+",昵称:"+nickName+",城市:"+city+",性别:"+sex;
                    chatHistory.contactAccount  = userName;
                    chatHistory.contactNickName = nickName;
                    chatHistory.contactCity	    = city;
                    chatHistory.contactSex		= StringConvertor::AcsiiToUtf8(sex);
                }
                break;
            }
        case 47:
            {
                chatHistory.msgType = WEIXIN_WEB_PIC;
            }
            break;
        case 48:
            {
                chatHistory.msgType = WEIXIN_MSG_MAP;
                if ( element != NULL )
                {
                    if ( (element = element->FirstChildElement("location")) != NULL )
                    {
                        string latitude;
                        const char* elementTmp = element->Attribute("x");
                        if (elementTmp)
                        {
                            latitude.append(elementTmp);
                        }
                        string longitude;
                        elementTmp = element->Attribute("y");
                        if (elementTmp)
                        {
                            longitude.append(elementTmp);
                        }
                        string label;
                        elementTmp = element->Attribute("label");
                        if (elementTmp)
                        {
                            label.append(elementTmp);
                        }

                        std::string str = "[地图位置] ";
                        chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += label;
                        str = " 纬度:";
                        chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += latitude;
                        str = " 经度:";
                        chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += longitude;

                        //chatHistory.message   = "[地图位置] "+label+" 纬度:"+latitude+", 经度:"+longitude;
                        chatHistory.location  = label;
                        chatHistory.latitude  = latitude;
                        chatHistory.longitude = longitude;
                    }
                }

                break;
            }
        case 49:
            {
                chatHistory.msgType = WEIXIN_MSG_TEXT;
                int paysubtype = 0;
                std::string pay_memo;
                std::string money;
                std::string str;
                const char *data = NULL;
                size_t pos = std::string::npos;
                chatHistory.message.clear();

                if (element != NULL)
                {
                    element = element->FirstChildElement("appmsg");
                    if (element)
                    {
                        tinyxml2::XMLElement *childElement = element->FirstChildElement("title");
                        if (childElement)
                        {
                            data = childElement->GetText();
                        }

                        str = data ? std::string(data) : "";
                        if (std::string::npos == str.find(StringConvertor::AcsiiToUtf8("微信转账")))
                        {
                            chatHistory.message = str;

                            element = element->FirstChildElement("mmreader");
                            if (element)
                            {
                                element = element->FirstChildElement("category");
                                if (element)
                                {
                                    element = element->FirstChildElement("item");
                                    if (element)
                                    {
                                        element = element->FirstChildElement("digest");
                                        if (element)
                                        {
                                            data = element->GetText();
                                            str = data ? std::string(data) : "";
                                            chatHistory.message += str;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            element = element->FirstChildElement("wcpayinfo");
                            if (element)
                            {
                                tinyxml2::XMLElement *childElement = element->FirstChildElement("paysubtype");
                                if (childElement)
                                {
                                    childElement->QueryIntText(&paysubtype);
                                }

                                childElement = element->FirstChildElement("feedesc");
                                if (childElement)
                                {
                                    data = childElement->GetText();
                                }

                                // 两种格式的人民币符号
                                // ¥ 0xC2 0xA5
                                // ￥0xEF 0xBF 0xA5
                                if (data && strlen(data) > 2)
                                {
                                    if (0xC2 == (unsigned char)data[0] && 0xA5 == (unsigned char)data[1])
                                    {
                                        str = std::string(data + 2);

                                        pos = str.find(']', 0);
                                        if (std::string::npos != pos)
                                        {
                                            money = str.substr(0, pos);
                                        }
                                        else
                                        {
                                            money = str;
                                        }
                                    }
                                }

                                if (money.empty())
                                {
                                    if (data && strlen(data) > 3)
                                    {
                                        if (0xEF == (unsigned char)data[0] && 0xBF == (unsigned char)data[1] && 0xA5 == (unsigned char)data[2])
                                        {
                                            str = std::string(data + 3);

                                            pos = str.find(']', 0);
                                            if (std::string::npos != pos)
                                            {
                                                money = str.substr(0, pos);
                                            }
                                            else
                                            {
                                                money = str;
                                            }
                                        }
                                    }
                                }

                                childElement = element->FirstChildElement("pay_memo");
                                if (childElement)
                                {
                                    data = childElement->GetText();
                                    pay_memo = data ? std::string(data) : "";
                                }
                            }

                            if (!money.empty())
                            {
                                std::string sender;

                                if (chatHistory.isSend)
                                {
                                    sender = senderName;
                                    if (sender.empty())
                                    {
                                        if ( isTroopChat )
                                        {
                                            sender = troopChatAccount;
                                        }
                                        else
                                        {
                                            sender = senderUin;
                                        }
                                    }
                                }
                                else
                                {
                                    sender = account;
                                    if (g_accountInfoEx.end() != g_accountInfoEx.find(account))
                                    {
                                        if (!g_accountInfoEx[account].nickName.empty())
                                        {
                                            sender = g_accountInfoEx[account].nickName;
                                        }
                                    }
                                }

                                if (1 == paysubtype || 8 == paysubtype)
                                {
                                    chatHistory.message = StringConvertor::AcsiiToUtf8("向") +
                                        sender +
                                        StringConvertor::AcsiiToUtf8("转账\n金额：") +
                                        money +
                                        StringConvertor::AcsiiToUtf8("元");
                                }
                                else if (3 == paysubtype)
                                {
                                    chatHistory.message = StringConvertor::AcsiiToUtf8("已收钱\n金额：") +
                                        money +
                                        StringConvertor::AcsiiToUtf8("元");
                                }

                                if (!pay_memo.empty())
                                {
                                    chatHistory.message += StringConvertor::AcsiiToUtf8("\n备注：") + pay_memo;
                                }
                            }
                        }
                    }
                }
                break;
            }
        case 50:
            {
                chatHistory.msgType = WEIXIN_MSG_CALL;
                body_xml = chatHistory.message;
                szXml = body_xml.c_str();
                doc.Parse(szXml);
                tinyxml2::XMLHandle docHandle( &doc ); 
                tinyxml2::XMLHandle xmlHandle = docHandle.FirstChildElement("voiplocalinfo");
                element = xmlHandle.ToElement();
                if ( element != NULL )
                {
                    element = element->FirstChildElement("duration");
                    if ( element != NULL )
                    {
                        string duration;
                        const char* tmp = element->GetText();
                        if (tmp)
                        {
                            duration.append(tmp);
                        }

                        std::string str = "[语音、视频通话] 时长:";
                        chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += duration;
                        str = "秒";
                        chatHistory.message += StringConvertor::AcsiiToUtf8(str);

                        //chatHistory.message = string("[语音、视频通话] ") + "时长:" + duration + "秒";
                        chatHistory.duration = duration;
                    }
                }
                break;
            }

        case 43:
        case 62:
            {
                if ( msgType == 43 )
                {
                    chatHistory.msgType = WEIXIN_MSG_VIDEO;
                }
                else
                {
                    chatHistory.msgType = WEIXIN_MSG_PREVIDEO;
                }
                string duration;
                string size;
                if ( element != NULL )
                {
                    if ( (element = element->FirstChildElement("videomsg")) != NULL )
                    {
                        const char* tmp = element->Attribute("length");;
                        if (tmp)
                        {
                            size.append(tmp);
                        }
                        tmp = element->Attribute("playlength");
                        if (tmp)
                        {
                            duration.append(tmp);
                        }
                        int    sizeKb = atoi(size.c_str()) / 1024;
                        sstr<<"[视频]大小:"<<sizeKb<<"kb,时长:"<<duration<<"秒";
                        sstr>>chatHistory.message;
                        sstr.clear();

                        chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
                        chatHistory.duration = duration;
                    }
                }
                sstr<<mesLocalID<<".mp4";
                sstr>>filePath;
                sstr.clear();
                sstr<<mesLocalID<<".video_thum";
                sstr>>thumbPath;
                filePath  = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Video\\" + hash + "\\" + filePath;
                thumbPath = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Video\\" + hash + "\\" + thumbPath;
                chatHistory.filePath  = filePath;
                chatHistory.thumbPath = thumbPath;

                std::string str;
                if (43 == msgType)
                {
                    str = "[视频] 时长:";
                }
                else
                {
                    str = "[即时拍摄视频] 时长:";
                }
                chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                chatHistory.message += duration;
                str = "秒";
                chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                //chatHistory.message = (msgType == 43) ? "[视频]":"[即时拍摄视频]" + string(" 时长:") + duration + "秒";
                break;
            }

        case 10000:
            chatHistory.msgType = WEIXIN_INTERPHONE;
            break;

        default:
            chatHistory.msgType = WEIXIN_MSG_OTHER;
            break;

        }

        chatInfo.chatList.push_back(chatHistory);
    }

    Sqlite3Helper.sqlite3_finalize(stmt);

    if (friendDb)
    {
        Sqlite3Helper.sqlite3_close(friendDb);
    }

    if (isRecovery)
    {
        g_accountInfo[account].recoveryChatMap[chatAccount + "-" + partname] = chatInfo;
    }
    else
    {
        g_accountInfo[account].chatMap[chatAccount + "-" + partname] = chatInfo;
    }
}

void IosGetSingleChatHistoryWithoutFriendAccount(sqlite3 *db, const std::string &account, int iType)
{
    std::string     tableName;
    std::string     chatAccountMd5;
    int iRet = -1;
    sqlite3_stmt *stmtTmp = NULL;

    if (!db)
        return;

    string sql = "select name from sqlite_master where type = 'table'";
    iRet = sqliteTmp.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmtTmp, NULL);
    if (SQLITE_OK != iRet)
        return;

    while(SQLITE_ROW == sqliteTmp.sqlite3_step(stmtTmp))
    {
        int size = sqliteTmp.sqlite3_column_bytes(stmtTmp, 0);
        const char * ptr = (const char *)sqliteTmp.sqlite3_column_blob(stmtTmp, 0);
        if (ptr)
        {
            tableName = ptr;
            if (37 == tableName.size())
            {
                int pos = tableName.find("Chat_");
                if (string::npos != pos)
                {
                    chatAccountMd5 = tableName.substr(pos + 5);
                }
                else
                {
                    continue;
                }
            }
            else 
            {
                continue;
            }
        }

        IosGetSingleChatHistoryWithoutFriendAccountEx(db, account, chatAccountMd5, tableName, iType);
    }
    sqliteTmp.sqlite3_finalize(stmtTmp);
}
    

void IosGetSingleChatHistory(sqlite3 *db, const std::string &account, const std::string &chatAccount, int iType)
{
	std::string		hash;
	std::string     tableName;
	std::string		sql;
	std::string		partname;
	bool isTroopChat = false;
	string troopChatAccount;

	int				iRet;
	sqlite3_stmt	*stmt;

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

	if ( chatAccount.find("@chatroom") != string::npos )
	{
		isTroopChat = true;
	}

	GetMD5(chatAccount, hash);


	//AdkTRACEA(("[hash]:%s\r\n", hash.c_str()));

	tableName = "Chat_" + hash;
	sql = "SELECT * FROM sqlite_master WHERE type=\"table\" AND name = \"" + tableName + "\"";

	iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
	if (SQLITE_OK != iRet)
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

	//
	//get account chat history
	ChatInfo		chatInfo;

	switch(iType)
	{
	case 0:
		sql = "SELECT Message, datetime(CreateTime,'unixepoch','localtime'), Type, Status, MesLocalID FROM " + tableName;
		break;
	default:
		;
	}

	iRet = Sqlite3Helper.sqlite3_prepare(db, sql.c_str(), sql.length(), &stmt, NULL);
	if (SQLITE_OK != iRet)
	{
		return; 
	}

	while(SQLITE_ROW == Sqlite3Helper.sqlite3_step(stmt))
	{
		ChatHistory	chatHistory;
		const  char *lpPointer;
		string filePath;
		string thumbPath;
		stringstream sstr;
		int		iSize;
		int		msgType;		
		int 	mesLocalID;
		string  body_xml;
		const char *szXml = NULL;
		tinyxml2::XMLDocument  doc; 
		tinyxml2::XMLElement  *element = NULL;

		//message
		//
		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 0);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 0);
		if (lpPointer)
		{
            chatHistory.message = lpPointer;
			if ( isTroopChat )
			{
				int iPos = chatHistory.message.find(":");
				if (iPos != -1)
				{
					troopChatAccount = chatHistory.message.substr(0, iPos);
					if (chatHistory.message.length() > (iPos + 2))
					{
						chatHistory.message = chatHistory.message.substr(iPos + 2);
					}		
				}			
			}
		}

		//time
		//
		iSize = Sqlite3Helper.sqlite3_column_bytes(stmt, 1);
		lpPointer = (const char *)Sqlite3Helper.sqlite3_column_blob(stmt, 1);
		if (lpPointer)
		{
            chatHistory.time = lpPointer;
		}

		//type
		//
		msgType = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 2);
		if ( msgType == 34 || msgType == 42 || msgType == 48 || msgType == 43 || msgType == 62)
		{

			body_xml = chatHistory.message;
			szXml = body_xml.c_str();
			doc.Parse(szXml);
			tinyxml2::XMLHandle docHandle( &doc ); 
			tinyxml2::XMLHandle xmlHandle = docHandle.FirstChildElement("msg");
			element = xmlHandle.ToElement();
		}

		//localID
		//
		mesLocalID = (int)Sqlite3Helper.sqlite3_column_int64(stmt, 4);	

		//1:文字、系统表情  3:图片  34:语音  42:发送名片 43:视频  62:录像、小视频
		//50:语音通话、视频通话  48:地图位置   10000:实时对讲
		switch (msgType)
		{
		case 1:
			{
				chatHistory.msgType = WEIXIN_MSG_TEXT;
				break;
			}

		case 3:
			{
				chatHistory.msgType = WEIXIN_MSG_PIC;
				chatHistory.message = "[图片]";
                chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
				sstr<<mesLocalID<<".pic";
				sstr>>filePath;
				sstr.clear();
				sstr<<mesLocalID<<".pic_thum";
				sstr>>thumbPath;
				filePath  = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Img\\" + hash + "\\" + filePath;
				thumbPath = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Img\\" + hash + "\\" + thumbPath;
				chatHistory.filePath  = filePath;
				chatHistory.thumbPath = thumbPath;
				break;
			}

		case 34:
			{
				chatHistory.msgType = WEIXIN_MSG_AUDIO;
				chatHistory.message = "[语音]";
                chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
                string strMesLocalID;
				sstr<<mesLocalID;
				sstr>>strMesLocalID;
				sstr.clear();

				//语音文件不能直接播放，需要添加文件头
				filePath = strMesLocalID + ".aud";
				filePath = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Audio\\" + hash + "\\" + filePath;
	
				chatHistory.filePath  = filePath;
				if ( element != NULL )
				{
					element = element->FirstChildElement("voicemsg");
					if ( element != NULL )
					{
						string duration;
						const char* tmp = element->Attribute("voicelength");
						if (tmp)
						{
							duration.append(tmp);
						}
						//将毫秒转换成秒
						if ( duration.length() > 3 )
						{
							duration = duration.insert(duration.length() - 3, 1, '.');
						}
						else 
						{
							while (duration.length() < 4)
							{
								duration = "0" + duration;
							}
							duration = duration.insert(duration.length() - 3, 1, '.');
						}
						chatHistory.duration = duration;
					}
				}
				break;
			}

		case 42:
			{
				chatHistory.msgType = WEIXIN_MSG_CARD;
				if ( element != NULL )
				{
					string userName;
					const char* elementTmp = element->Attribute("username");
					if (elementTmp)
					{
						userName.append(elementTmp);
					}
					string nickName;
					elementTmp = element->Attribute("nickname");
					if (elementTmp)
					{
						nickName.append(elementTmp)
							;						}
					string city;
					elementTmp = element->Attribute("city");
					if (elementTmp)
					{
						city.append(elementTmp);
					}
					string sex;
					elementTmp = element->Attribute("sex");
					if (elementTmp)
					{
						sex.append(elementTmp);
						if (sex == string("0"))
						{
							sex = "男";
						}
						else
						{
							sex = "女";

						}
					}
                    std::string str = "[名片] ";
                    chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                    str = "账号:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += userName;
                    str = ",昵称:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += nickName;
                    str = ",城市:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += city;
                    str = ",性别:";
                    chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                    chatHistory.message += StringConvertor::AcsiiToUtf8(sex);

					//chatHistory.message = string("[名片] ")+"账号:"+ userName+",昵称:"+nickName+",城市:"+city+",性别:"+sex;
					chatHistory.contactAccount  = userName;
					chatHistory.contactNickName = nickName;
					chatHistory.contactCity	    = city;
					chatHistory.contactSex		= StringConvertor::AcsiiToUtf8(sex);
				}
				break;
			}

		case 48:
			{
				chatHistory.msgType = WEIXIN_MSG_MAP;
				if ( element != NULL )
				{
					if ( (element = element->FirstChildElement("location")) != NULL )
					{
						string latitude;
						const char* elementTmp = element->Attribute("x");
						if (elementTmp)
						{
							latitude.append(elementTmp);
						}
						string longitude;
						elementTmp = element->Attribute("y");
						if (elementTmp)
						{
							longitude.append(elementTmp);
						}

                        std::string str = "[地图位置] ";
                        chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                        str = "纬度:";
                        chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += latitude;
                        str = "，经度:";
                        chatHistory.message += StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += longitude;

						//chatHistory.message = string("[地图位置] ") + "纬度:" + latitude + ", 经度:" + longitude;
						chatHistory.latitude  = latitude;
						chatHistory.longitude = longitude;
					}
				}

				break;
			}

		case 50:
			{
				chatHistory.msgType = WEIXIN_MSG_CALL;
				body_xml = chatHistory.message;
				szXml = body_xml.c_str();
				doc.Parse(szXml);
				tinyxml2::XMLHandle docHandle( &doc ); 
				tinyxml2::XMLHandle xmlHandle = docHandle.FirstChildElement("voiplocalinfo");
				element = xmlHandle.ToElement();
				if ( element != NULL )
				{
					element = element->FirstChildElement("duration");
					if ( element != NULL )
					{
						string duration;
						const char* tmp = element->GetText();
						if (tmp)
						{
							duration.append(tmp);
						}

                        std::string str = "[语音、视频通话] 时长:";
                        chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                        chatHistory.message += duration;
                        str = "秒";
                        chatHistory.message = StringConvertor::AcsiiToUtf8(str);

						//chatHistory.message = string("[语音、视频通话] ") + "时长:" + duration + "秒";
						chatHistory.duration = duration;
					}
				}
				break;
			}

		case 43:
		case 62:
			{
				if ( msgType == 43 )
				{
					chatHistory.msgType = WEIXIN_MSG_VIDEO;
				}
				else
				{
					chatHistory.msgType = WEIXIN_MSG_PREVIDEO;
				}
				string duration;
				string size;
				if ( element != NULL )
				{
					if ( (element = element->FirstChildElement("videomsg")) != NULL )
					{	
						const char* tmp = element->Attribute("length");
						if (tmp)
						{
							size.append(tmp);
						}
						tmp = element->Attribute("playlength");
						if (tmp)
						{
							duration.append(tmp);
						}
						int    sizeKb = atoi(size.c_str()) / 1024;
						sstr<<"[视频]大小:"<<sizeKb<<"kb,时长:"<<duration<<"秒";
						sstr>>chatHistory.message;
						sstr.clear();
                        chatHistory.message = StringConvertor::AcsiiToUtf8(chatHistory.message);
						chatHistory.duration = duration;
					}
				}
				sstr<<mesLocalID<<".mp4";
				sstr>>filePath;
				sstr.clear();
				sstr<<mesLocalID<<".video_thum";
				sstr>>thumbPath;
				filePath  = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Video\\" + hash + "\\" + filePath;
				thumbPath = StringConvertor::UnicodeToUtf8(g_wsDirPath) + "\\Documents\\" + account + "\\Video\\" + hash + "\\" + thumbPath;
				chatHistory.filePath  = filePath;
				chatHistory.thumbPath = thumbPath;

                std::string str;
                if (43 == msgType)
                {
                    str = "[视频] 时长:";
                }
                else
                {
                    str = "[即时拍摄视频] 时长:";
                }
                chatHistory.message = StringConvertor::AcsiiToUtf8(str);
                chatHistory.message += duration;
                str = "秒";
                chatHistory.message += StringConvertor::AcsiiToUtf8(str);

				//chatHistory.message = (msgType == 43) ? "[视频]":"[即时拍摄视频]" + string(" 时长:") + duration + "秒";
				break;
			}

		case 10000:
			chatHistory.msgType = WEIXIN_INTERPHONE;
			break;

		default:
			chatHistory.msgType = WEIXIN_MSG_OTHER;
			break;

		}

		//issend
		chatHistory.isSend = (Sqlite3Helper.sqlite3_column_int64(stmt, 3) == 2);
		if (chatHistory.isSend)
		{
			chatHistory.senderUin = account;
		}
		else
		{
			chatHistory.senderUin = chatAccount;
			if ( isTroopChat )
			{
				chatHistory.senderUin = troopChatAccount;
			}
		}

		chatInfo.chatList.push_back(chatHistory);
	}

	g_accountInfo[account].chatMap[chatAccount + "-" + partname] = chatInfo;
	Sqlite3Helper.sqlite3_finalize(stmt);
	return;
}


FriendInfo* WEIXIN_GetFirstFriend(std::string &account)
{
	if (g_accountInfo.find(account) == g_accountInfo.end())
	{
		return NULL;
	}

	AccountInfo	*pAccountInfo = &g_accountInfo[account];
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

FriendInfo* WEIXIN_GetNextFriend(std::string &account)
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

FriendInfo* WEIXIN_GetRecoveryFirstFriend(std::string &account)
{
    if (g_accountInfo.find(account) == g_accountInfo.end())
    {
        return NULL;
    }

    AccountInfo	*pAccountInfo = &g_accountInfo[account];
    pAccountInfo->friendItera = pAccountInfo->recoveryFriendList.begin();
    if (pAccountInfo->friendItera == pAccountInfo->recoveryFriendList.end())
    {
        return NULL;
    }
    else
    {
        return &(*pAccountInfo->friendItera);
    }
}

FriendInfo* WEIXIN_GetRecoveryNextFriend(std::string &account)
{
    if (g_accountInfo.find(account) == g_accountInfo.end())
    {
        return NULL;
    }

    AccountInfo	*pAccountInfo = &g_accountInfo[account];
    if (pAccountInfo->friendItera == pAccountInfo->recoveryFriendList.end())
    {
        return NULL;
    }

    pAccountInfo->friendItera++;
    if (pAccountInfo->friendItera == pAccountInfo->recoveryFriendList.end())
    {
        return NULL;
    }
    else
    {
        return &(*pAccountInfo->friendItera);
    }
}

ChatHistory* WEIXIN_GetFirstChatHistory(std::string &account, std::string &chatAccount, int iType)
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
	std::map<std::string,ChatInfo>::iterator itera = g_accountInfo[account].chatMap.find(ttlName);
	if (itera == g_accountInfo[account].chatMap.end())
	{
		return NULL;
	}

	ChatInfo *pChatInfo = &itera->second;

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

ChatHistory* WEIXIN_GetNextChatHistory(std::string &account, std::string &chatAccount, int iType)
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
	std::map<std::string,ChatInfo>::iterator itera = g_accountInfo[account].chatMap.find(ttlName);
	if (itera == g_accountInfo[account].chatMap.end())
	{
		return NULL;
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

ChatHistory* WEIXIN_GetRecoveryFirstChatHistory(std::string &account, std::string &chatAccount, int iType)
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
    std::map<std::string,ChatInfo>::iterator itera = g_accountInfo[account].recoveryChatMap.find(ttlName);
    if (itera == g_accountInfo[account].recoveryChatMap.end())
    {
        return NULL;
    }

    ChatInfo *pChatInfo = &itera->second;

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

ChatHistory* WEIXIN_GetRecoveryNextChatHistory(std::string &account, std::string &chatAccount, int iType)
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
    std::map<std::string,ChatInfo>::iterator itera = g_accountInfo[account].recoveryChatMap.find(ttlName);
    if (itera == g_accountInfo[account].recoveryChatMap.end())
    {
        return NULL;
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