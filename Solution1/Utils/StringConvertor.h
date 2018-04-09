#pragma once
#include <string>

using std::string;
using std::wstring;		

class StringConvertor
{
public:	
	static wstring AcsiiToUnicode(const string& str);

	static string AcsiiToUtf8(const string& str);

	static string UnicodeToAcsii(const wstring& str);

	static string UnicodeToUtf8(const wstring& str);

	static string Utf8ToAscii(const string& str);

	static wstring Utf8ToUnicode(const string& str);
};

