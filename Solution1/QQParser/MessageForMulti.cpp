#include "stdafx.h"
#include "MessageForMulti.h"
#include "MessageForPic.h"
#include "MessageForPtt.h"
#include "MessageForVideo.h"
#include <strsafe.h>


MsgForMulti::MsgForMulti(char* msgData, int iDatalen, bool isSend)
{
	this->isSend = isSend;
	msgBuf = NULL;
	codeInput = NULL;
	if (msgData)
	{
		msgBuf = msgData;
		codeInput = new CodedInputStreamMicro(msgData, iDatalen);
	}
}

MsgForMulti::~MsgForMulti()
{
	if (codeInput)
	{
		delete codeInput;
	}
}	
	
bool MsgForMulti::doParse()
{
    if (!codeInput)
    {
        return false;
    }

	int iTag = codeInput->readTag();
	//���0a
	if (iTag != 0xa)
	{
		return false;
	}
	bool bReadTag = false;

	while (!codeInput->isAtEnd())
	{
		if (bReadTag)
		{
			iTag = codeInput->readTag();
		}
		else
		{
			bReadTag = true;
		}
		
		bool bTail = false;
		do 
		{	
			//���0a

			if (iTag < 0)
			{
				//��ȡ�쳣
				MultiMsg msg;
				msg.msgType = 0;
				msg.content = (msgBuf + codeInput->bufferPos);
				msgLst.push_back(msg);
				return true;
			}

			if (iTag != 0xa)
			{
				break;
			}


			//multiMsg ���Ȼ�ȡ
			int iTmpPos = codeInput->bufferPos;
			int iLen = codeInput->readRawVarint32(true);
			if ((iLen < 0) || (iLen >= codeInput->bufferSize))
			{
				//��ȡ�쳣
				MultiMsg msg;
				msg.msgType = 0;
				msg.content = (msgBuf + iTmpPos);
				msgLst.push_back(msg);
				return true;
			}

			iTag = codeInput->readTag();
			switch (iTag)
			{
			case 0x12:
				{
					//pic
					iLen = codeInput->readRawVarint32(true);
					if (iLen > 0)
					{			
						MsgForPic msgPic(msgBuf + codeInput->bufferPos, iLen, this->isSend);
						msgPic.doParse();
						MultiMsg msg;
						msg.msgType = 1;
						msg.content = msgPic.strPath;
						msgLst.push_back(msg);

						//������ƫ��
						codeInput->bufferPos += iLen;
					}
				}
				break;
			case 0xa:
				{
					//text
					char* szBuf = NULL;
					int iCopy = codeInput->readBytes(&szBuf, true);
					if (szBuf)
					{
						MultiMsg msg;
						msg.msgType = 0;
						int iTmp = 0;
						while (iTmp < iCopy)
						{
							char ch = szBuf[iTmp];
							if (ch != '\0')
							{
								msg.content.append(1, ch);
							}
							else
							{
								msg.content.append(1, ' ');
							}
							iTmp++;
						}
						delete [] szBuf;

						
						if (codeInput->bufferSize == (codeInput->bufferPos + iLen - iCopy))
						{
							int iPendding = iLen - iCopy;
							for (int i = 0; i < iPendding; i++)
							{
								msg.content.append(1, msgBuf[codeInput->bufferPos + i]);
							}
						}
						msgLst.push_back(msg);
					}		
				}
				break;
			default:
				{
					//������ptt,video���Ǿ����ǲ���������ʱ����
					iLen = codeInput->readRawVarint32(true);
					MultiMsg msg;
					msg.msgType = 2;
					msg.content = "[�ļ�]";
					msgLst.push_back(msg);

					//������ƫ��
					codeInput->bufferPos += iLen;
				}
				break;
			}
		} while (false);
	}
	return true;
}