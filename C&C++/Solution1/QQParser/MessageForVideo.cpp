#include "stdafx.h"
#include "MessageForVideo.h"

MsgForVideo::MsgForVideo(char* msgData, int iDatalen)
{
	codeInput = new CodedInputStreamMicro(msgData, iDatalen);
}

MsgForVideo::~MsgForVideo()
{
	if (codeInput)
	{
		delete codeInput;
	}
	fieldMap.clear();
}
	
void MsgForVideo::doParse()
{
    if (!codeInput)
    {
        return;
    }

    int arrayOfInt[] = { 10, 18, 26, 32, 40, 48, 56, 64, 72, 80, 88, 98};
    /*
    "bytes_file_uuid", "bytes_file_md5", "bytes_file_name", "uint32_file_format", 
    "uint32_file_time", "uint32_file_size", "uint32_thumb_width", "uint32_thumb_height", 
    "uint32_file_status", "uint32_file_progress", "uint32_file_type", "bytes_thumb_file_md5" 
    */
    MsgForVideo::FieldType arryOftype[] = {
        VALUE_STRING, VALUE_STRING, VALUE_STRING, VALUE_UINT32, 
        VALUE_UINT32, VALUE_UINT32, VALUE_UINT32, VALUE_UINT32, 
        VALUE_UINT32, VALUE_UINT32, VALUE_UINT32, VALUE_STRING
    };
    int iArry = 12;
    for (int i = 0; i < iArry; i++) 
    {
        fieldMap.insert(std::map<int, MsgForVideo::FieldType>::value_type(arrayOfInt[i], arryOftype[i]));
    }

	std::map<int, MsgForVideo::FieldType>::iterator it;
	
	while (!codeInput->isAtEnd())
	{
		char* szBuf = NULL;
		int iTag = codeInput->readTag();
		bool bBreak = false;

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

			//video md5（视频文件）
			if (18 == it->first)
			{
				if (szBuf)
				{
					splitBytes(szBuf, 16, this->strPath);
					delete [] szBuf;
					szBuf = NULL;
				}
				
				//this->strPath.append(szBuf);
			}
			//thumb md5(视频封面对应图片)
			if (98 == it->first)
			{
				//图片对应MD5
				if (szBuf)
				{
					splitBytes(szBuf, 16, this->strThumbFile);
					delete [] szBuf;
					szBuf = NULL;
				}
				
				//this->strThumbFile.append(szBuf);
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
			bBreak = this->codeInput->skipField(iTag);
		}
		if (bBreak)
		{
			bBreak;
		}
	}
}

void MsgForVideo::splitBytes(char* in, int insize, std::string& dst)
{
	do
	{
		if (!in || 0 >= insize)
		{
			break;
		}
		dst.clear();
		int i = 0;
		while(i < insize)
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