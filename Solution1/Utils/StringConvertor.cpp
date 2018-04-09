#include "StringConvertor.h"
#include <Windows.h>
#include <vector>
#include <winnls.h>

using std::vector;

wstring StringConvertor::AcsiiToUnicode(const string& str)
{
	int wideSize = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
	if (ERROR_NO_UNICODE_TRANSLATION == wideSize)
		return L"";

	if (0 == wideSize)
		return L"";

	vector<wchar_t> resultVec(wideSize);
	int resultSize = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &resultVec[0], wideSize);
	if (resultSize != wideSize)
		return L"";

	return wstring(&resultVec[0]);
}

string StringConvertor::AcsiiToUtf8(const string& str)
{
	string resultStr("");
	
	wstring tmpStr = AcsiiToUnicode(str);
	resultStr = UnicodeToUtf8(tmpStr);

	return resultStr;
}

string StringConvertor::UnicodeToAcsii(const wstring& str)
{
	int acsiiSize = WideCharToMultiByte(CP_OEMCP, 0, str.c_str(), -1, NULL, 0, NULL , NULL);
	if (ERROR_NO_UNICODE_TRANSLATION == acsiiSize)
		return "";

	if (0 == acsiiSize)
		return "";

	vector<char> resultVec(acsiiSize);
	int resultSize = WideCharToMultiByte(CP_OEMCP, 0, str.c_str(), -1, &resultVec[0], acsiiSize, NULL , NULL);
	if (resultSize != acsiiSize)
		return "";

	return string(&resultVec[0]);
}

string StringConvertor::UnicodeToUtf8(const wstring& str)
{
	int utf8Size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL , NULL);
	if (0 == utf8Size)
		return "";

	vector<char> resultVec(utf8Size);
	int resultSize = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &resultVec[0], utf8Size, NULL , NULL);
	if (resultSize != utf8Size)
		return "";

	return string(&resultVec[0]);
}

string StringConvertor::Utf8ToAscii(const string& str)
{
	string resultStr("");

	wstring tmpStr = Utf8ToUnicode(str);
	resultStr = UnicodeToAcsii(tmpStr);

	return resultStr;
}

wstring StringConvertor::Utf8ToUnicode(const string& str)
{
	int wideSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
	if (ERROR_NO_UNICODE_TRANSLATION == wideSize)
		return L"";

	if (0 == wideSize)
		return L"";

	vector<wchar_t> resultVec(wideSize);
	int resultSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &resultVec[0], wideSize);
	if (resultSize != wideSize)
		return L"";

	return wstring(&resultVec[0]);
}

