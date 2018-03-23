#include "stdafx.h"
#include "MessageForPic.h"
#include <strsafe.h>


MsgForPic::MsgForPic(char* msgData, int iDatalen, bool isSend)
{
	this->isSend = isSend;
	codeInput = new CodedInputStreamMicro(msgData, iDatalen);
}

MsgForPic::~MsgForPic()
{
	if (codeInput)
	{
		delete codeInput;
	}
	fieldMap.clear();
}	
	
void MsgForPic::doParse()
{
    if (!codeInput)
    {
        return;
    }

  	int arrayOfInt[] = { 10, 16, 24, 32, 42, 50};
	MsgForPic::FieldType arryOftype[] = {VALUE_STRING, VALUE_UINT64, VALUE_UINT32, VALUE_BOOL, VALUE_STRING, VALUE_STRING};
	int iArry = 6;
	for (int i = 0; i < iArry; i++) 
	{
		fieldMap.insert(std::map<int, MsgForPic::FieldType>::value_type(arrayOfInt[i], arryOftype[i]));
	}

	std::map<int, MsgForPic::FieldType>::iterator it;
	
	while (!codeInput->isAtEnd())
	{
		char* szBuf = NULL;
		int iTag = codeInput->readTag();

		//parseUnknownField
		it = fieldMap.find(iTag);
		if (fieldMap.end() != it)
		{
			switch(it->second)
			{
			case VALUE_STRING:
			  codeInput->readBytes(&szBuf);
			  break;
			case VALUE_UINT64:
			  codeInput->readInt64();
			  break;
			case VALUE_UINT32:
			  codeInput->readInt32();
			  break;
			case VALUE_BOOL:
			  codeInput->readBool();
			  break;
			default:
			  break;
			}

			//send->localpath
			if (this->isSend && (10 == it->first))
			{
				if (szBuf)
				{
					this->strPath.append(szBuf);
					delete [] szBuf;
					szBuf = NULL;
				}
					
				break;
			}
			//recv->Crc64(chatimg:大写md5值)
			if (!this->isSend && (50 == it->first))
			{
				//图片对应MD5
				//splitBytes(szBuf, this->strPath);
				if (szBuf)
				{
					this->strPath = szBuf;
					delete [] szBuf;
					szBuf = NULL;
				}
				
				char szCrcBuf[260 + 8] = {0};
                StringCbPrintfA(szCrcBuf, sizeof(szCrcBuf), "chatimg:%s", this->strPath.c_str());
				long long llcrc = Crc64((unsigned char*)szCrcBuf,40);
				char szPath[17 + 6] = {0};
				this->strPath.clear();

				if (llcrc > 0)//负值的情况下先转正数再转16进制
				{
					this->strPath.append("Tencent\\MobileQQ\\diskcache\\Cache_");
				}
				else
				{
					this->strPath.append("Tencent\\MobileQQ\\diskcache\\Cache_-");
					llcrc = -llcrc;
				}

                StringCbPrintfA(szPath, sizeof(szPath), "%016llx", llcrc);
				//sprintf(szPath, "%016llx", llcrc);
				this->strPath.append(szPath);
				break;
			}

			if (szBuf)
			{
				delete [] szBuf;
				szBuf = NULL;
			}
		}
		else
		{
			this->codeInput->skipField(iTag);
		}		  
	}
}

long long MsgForPic::Crc64(unsigned char* in, int iLen) 
{
	long long CRCTable[256] = {0};
    long long v1;
    if(!in|| iLen == 0) 
	{
        v1 = 0;
    }
    else 
	{
        v1 = -1;
        int v3;
        for(v3 = 0; v3 < 256; ++v3) 
		{
            long long v7 = ((long long)v3);
            int v4;
            for(v4 = 0; v4 < 8; ++v4) {
                if(((((int)v7)) & 1) != 0) {
                    v7 = v7 >> 1 ^ -7661587058870466123L;
                }
                else {
                    v7 >>= 1;
                }
            }

            CRCTable[v3] = v7;
        }
        int v6 = 40;
        int v5;
        for(v5 = 0; v5 < v6; ++v5) 
		{
            v1 = CRCTable[((((int)v1)) ^ in[v5]) & 255] ^ v1 >> 8;
        }
    }
    return v1;
}

void MsgForPic::splitBytes(char* in, std::string& dst)
{
	do
	{
		dst.clear();
		if (!in)
		{
			break;
		}
		int i = 0;
		while(in[i])
		{
			char low = 0;
			char high = 0;
			high = (in[i] & 0xf0) >> 4;
			low = in[i] & 0x0f;
			if (high >= 0x00 && high <= 0x09)
			{
				//数字
				high += '0';
			}
			else
			{
				//字母(转换为大写)
				high += 55;
			}

			
			if (low >= 0x00 && low <= 0x09)
			{
				low += '0';
			}
			else
			{
				low += 55;
			}
			dst.append(1, high);
			dst.append(1, low);
			i++;
		}
	}while(false);	
}