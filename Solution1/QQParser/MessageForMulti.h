#pragma once
/*
 *����ͼƬmsgData
*/
#include "CodeInputStream.h"
#include "Parser.h"
#include <map>
#include <string>
#include <list>

class MsgForMulti
{
public:
	MsgForMulti(char* msgData, int iDatalen, bool isSend);
	~MsgForMulti();
    bool doParse();

public:
	std::list<MultiMsg> msgLst;
private:
	char* msgBuf;
	bool isSend;
	CodedInputStreamMicro* codeInput;
};
