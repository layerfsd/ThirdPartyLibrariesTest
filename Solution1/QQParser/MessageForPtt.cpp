#include "stdafx.h"
#include "MessageForPtt.h"

MsgForPtt::MsgForPtt(char* msgData, int iDatalen)
{
	codeInput = new CodedInputStreamMicro(msgData, iDatalen);
}

MsgForPtt::~MsgForPtt()
{
	if (codeInput)
	{
		delete codeInput;
	}
	fieldMap.clear();
}
	
void MsgForPtt::doParse()
{  
	 int arrayOfInt[] = { 10};
	 /*
		 "bytes_file_uuid", "bytes_file_md5", "bytes_file_name", "uint32_file_format", 
		  "uint32_file_time", "uint32_file_size", "uint32_thumb_width", "uint32_thumb_height", 
		  "uint32_file_status", "uint32_file_progress", "uint32_file_type", "bytes_thumb_file_md5" 
	*/
	MsgForPtt::FieldType arryOftype[] = {VALUE_STRING};
	int iArry = 1;
	for (int i = 0; i < iArry; i++) 
	{
		fieldMap.insert(std::map<int, MsgForPtt::FieldType>::value_type(arrayOfInt[i], arryOftype[i]));
	}

	std::map<int, MsgForPtt::FieldType>::iterator it;
	
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

			//audio路径
			if (10 == it->first)
			{
				if (szBuf)
				{
					this->strPath.append(szBuf);
					delete [] szBuf;
					szBuf = NULL;
				}           
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
			break;
		}
	}
}