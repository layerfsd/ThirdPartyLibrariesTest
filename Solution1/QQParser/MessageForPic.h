#pragma once
/*
 *解析图片msgData
*/
#include "CodeInputStream.h"
#include <map>
#include <string>

class MsgForPic
{

private:
	enum FieldType
	{
		VALUE_BOOL = 0,
		VALUE_UINT32,
		VALUE_UINT64,
		VALUE_STRING
	};

public:
	MsgForPic(char* msgData, int iDataen, bool isSend);
	~MsgForPic();

/*
        char*[] arrayOfString = { "localPath", "size", "type", "isRead", 
			                      "uuid", "md5" };
								    public final PBStringField bigMsgUrl = PBField.initString("");
		  public final PBBoolField enableEnc = PBField.initBool(false);
		  public final PBInt32Field fileSizeFlag = PBField.initInt32(0);
		  public RichMsg.ForwardExtra fowardInfo = new RichMsg.ForwardExtra();
		  public final PBUInt64Field groupFileID = PBField.initUInt64(0L);
		  public final PBBoolField isRead = PBField.initBool(false);
		  public final PBInt32Field isReport = PBField.initInt32(0);
		  public final PBStringField localPath = PBField.initString("");
		  public final PBStringField localUUID = PBField.initString("");
		  public final PBStringField md5 = PBField.initString("");
		  public final PBUInt32Field notPredownloadReason = PBField.initUInt32(0);
		  public final PBInt32Field preDownNetwork = PBField.initInt32(0);
		  public final PBInt32Field preDownState = PBField.initInt32(0);
		  public final PBInt32Field previewed = PBField.initInt32(0);
		  public final PBStringField rawMsgUrl = PBField.initString("");
		  public final PBStringField serverStorageSource = PBField.initString("");
		  public final PBUInt64Field size = PBField.initUInt64(0L);
		  public final PBStringField thumbMsgUrl = PBField.initString("");
		  public final PBUInt32Field type = PBField.initUInt32(0);
		  public final PBInt32Field uiOperatorFlag = PBField.initInt32(0);
		  public final PBUInt32Field uint32_current_len = PBField.initUInt32(0);
		  public final PBUInt32Field uint32_download_len = PBField.initUInt32(0);
		  public final PBUInt32Field uint32_height = PBField.initUInt32(0);
		  public final PBUInt32Field uint32_image_type = PBField.initUInt32(0);
		  public final PBUInt32Field uint32_show_len = PBField.initUInt32(0);
		  public final PBUInt32Field uint32_thumb_height = PBField.initUInt32(0);
		  public final PBUInt32Field uint32_thumb_width = PBField.initUInt32(0);
		  public final PBUInt32Field uint32_width = PBField.initUInt32(0);
		  public final PBStringField uuid = PBField.initString("");
		  public final PBInt32Field version = PBField.initInt32(0);
*/
   void doParse();


private:
	long long  Crc64(unsigned char* in, int iLen) ;
	void splitBytes(char* in, std::string& dst);

public:
	std::string strPath;

private:
	bool isSend;
	CodedInputStreamMicro* codeInput;
	std::map<int, MsgForPic::FieldType> fieldMap;
};
