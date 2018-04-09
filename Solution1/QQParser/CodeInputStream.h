/*
 *½âÎömsData
*/
#pragma once
class CodedInputStreamMicro 
{

public:
    CodedInputStreamMicro(char* buffer, int len);
	~CodedInputStreamMicro();
    bool isAtEnd();
    bool readBool();
    int readBytes(char** szBuf,bool bMultiMsg = false);
    int readInt32();
    long long readInt64();
    char readRawByte();
    int readRawLittleEndian32();
    long long readRawLittleEndian64();
    int readRawVarint32(bool bMultiMsg = false);
    long long readRawVarint64();
    int readSFixed32();
    long long readSFixed64();
    int readSInt32();
    int readTag();
    int readUInt32();
    long long readUInt64();
    bool skipField(int arg4) ;
    void skipMessage();
    bool skipRawBytes(int arg5);

public:
	int bufferPos;
	int bufferSize;

private:
    char* buffer;   
	int lastTag;
};

