#include "stdafx.h"

#include <string.h>

#include "CodeInputStream.h"

CodedInputStreamMicro::CodedInputStreamMicro(char* buffer, int len)
{
	if (buffer && len > 0)
	{
		this->buffer = new char[len + 1]();
		memcpy(this->buffer, buffer, len);
	}
    this->bufferPos = 0;
	this->bufferSize = len;
}

CodedInputStreamMicro::~CodedInputStreamMicro()
{
	if (this->buffer)
	{
		delete [] this->buffer;
	}
}

bool CodedInputStreamMicro::isAtEnd()
{
	return ((this->bufferPos < 0) || (this->bufferPos >= this->bufferSize));
}

bool CodedInputStreamMicro::readBool()
{
    return (this->readRawVarint32() != 0);
}

int CodedInputStreamMicro::readBytes(char** szBuf, bool bMultiMsg)
{
	int iCopy = 0;
	do
	{
		if(*szBuf)
		{
			break;
		}
		//multiMsg读取大小有区别
		int v1 = this->readRawVarint32(bMultiMsg);
		if((v1 > 0) && (v1 <= this->bufferSize - this->bufferPos)) 
		{
			*szBuf = new char[v1+1]();
			memcpy(*szBuf, this->buffer + this->bufferPos, v1);
			this->bufferPos = v1 + this->bufferPos;
			iCopy = v1;
		}
	}while(false);

	return iCopy;
}

int CodedInputStreamMicro::readInt32() 
{
    return this->readRawVarint32();
}

long long CodedInputStreamMicro::readInt64()
{
    return this->readRawVarint64();
}

char CodedInputStreamMicro::readRawByte()
{
	if ((this->bufferPos < 0) || (this->bufferPos >= this->bufferSize))
	{
		this->bufferPos++;
		return 0;
	}
    this->bufferPos++;
    return this->buffer[this->bufferPos - 1];
}

int CodedInputStreamMicro::readRawLittleEndian32()
{
    return this->readRawByte() & 255 | (this->readRawByte() & 255) << 8 | (this->readRawByte() & 255)
             << 16 | (this->readRawByte() & 255) << 24;
}

long long CodedInputStreamMicro::readRawLittleEndian64()
{
    return ((((long long)this->readRawByte())) & 255) << 8 | (((long long)this->readRawByte())) & 255 | ((((
            long long)this->readRawByte())) & 255) << 16 | ((((long long)this->readRawByte())) & 255) << 24 | ((((
            long long)this->readRawByte())) & 255) << 32 | ((((long long)this->readRawByte())) & 255) << 40 | ((((
            long long)this->readRawByte())) & 255) << 48 | ((((long long)this->readRawByte())) & 255) << 56;
}

//增加混合消息长度读取
int CodedInputStreamMicro::readRawVarint32(bool bMultiMsg)
{
    int v0 = this->readRawByte();
    if(v0 < 0) 
	{
        v0 &= 127;
        int v1 = this->readRawByte();
        if(v1 >= 0) {
			if (bMultiMsg){
				int iLen = (v1*127) + v0;
				return iLen;
			}
            v0 |= v1 << 7;
        }
        else {
            v0 |= (v1 & 127) << 7;
            v1 = this->readRawByte();
            if(v1 >= 0) {
                v0 |= v1 << 14;
            }
            else {
                v0 |= (v1 & 127) << 14;
                v1 = this->readRawByte();
                if(v1 >= 0) {
                    v0 |= v1 << 21;
                }
                else {
                    v0 |= (v1 & 127) << 21;
                    v1 = this->readRawByte();
                    v0 |= v1 << 28;
                    if(v1 < 0) {
                        v1 = 0;
                        while(true) {
                            if(v1 >= 5) {
                                break;
                            }
                            else if(this->readRawByte() < 0) {
                                ++v1;
                                continue;
                            }

                            return v0;
                        }
                    }
                }
            }
        }
    }

    return v0;
}

long long CodedInputStreamMicro::readRawVarint64()
{
    int v2 = 0;
    long long v0 = 0;
    while(v2 < 64) 
	{
        int v3 = this->readRawByte();
        v0 |= (((long long)(v3 & 127))) << v2;
        if((v3 & 128) == 0) {
            return v0;
        }

        v2 += 7;
    }
	return 0;
}

int CodedInputStreamMicro::readSFixed32()
{
    return this->readRawLittleEndian32();
}

long long CodedInputStreamMicro::readSFixed64()
{
    return this->readRawLittleEndian64();
}

int CodedInputStreamMicro::readSInt32()
{
    return this->readRawVarint32();
}

int CodedInputStreamMicro::readTag()
{
    if(this->isAtEnd()) 
	{
        return 0;
    }
    else 
	{
		this->lastTag = this->readRawVarint32();
        return this->lastTag;
	}
}

int CodedInputStreamMicro::readUInt32()
{
    return this->readRawVarint32();
}

long long CodedInputStreamMicro::readUInt64()
{
    return this->readRawVarint64();
}


bool CodedInputStreamMicro::skipField(int arg4) 
{
    bool v0 = true;
	int iType = arg4&7;
    switch(iType) {
        case 0: {
            this->readInt32();
			break;
        }
        case 1: {
            this->readRawLittleEndian64();
			break;
        }
        case 2: {
            this->skipRawBytes(this->readRawVarint32());
			break;
        }
        case 3: {
            this->skipMessage();
			unsigned int iTmp = (unsigned int)arg4;
			iTmp = iTmp>>3;
			int iTagTmp = iTmp << 3 | 4;
			v0 = (this->lastTag == iTagTmp);
			break;
        }
        case 4: {
            v0 = false;
			break;
        }
        case 5: {
            this->readRawLittleEndian32();
			break;
        }
		default:
			v0 = false;
			break;

    }
    return v0;
}

void CodedInputStreamMicro::skipMessage()
{
	int v0 = 0;
    do {
        v0 = this->readTag();
        if(v0 == 0) {
            return;
        }
    }
    while(this->skipField(v0));
}

bool CodedInputStreamMicro::skipRawBytes(int arg5)
{
	bool bSkip = false;
	do
	{		
		if(arg5 < 0) {
			break;
		}

		if(arg5 <= this->bufferSize - this->bufferPos) 
		{
			this->bufferPos += arg5;
			bSkip = true;
		}
	}while(false);

	return bSkip;
}