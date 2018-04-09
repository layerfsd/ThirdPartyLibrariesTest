#pragma once
/*
 *Ω‚ŒˆÕº∆¨msgData
*/
#include "CodeInputStream.h"
#include <map>
#include <string>

class MsgForPtt
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
	MsgForPtt(char* msgData, int iDataen);
	~MsgForPtt();

/*
    int[] arrayOfInt = { 10, 18, 26, 32, 40, 48, 56, 64, 72, 80, 88, 98, 106, 112, 120, 128, 136, 144, 152, 162, 168, 176, 184, 194, 200, 208, 216 };
    String[] arrayOfString = { "bytes_file_uuid", "bytes_file_md5", "bytes_file_name", "uint32_file_format", "uint32_file_time", "uint32_file_size", "uint32_thumb_width", "uint32_thumb_height", "uint32_file_status", "uint32_file_progress", "uint32_file_type", "bytes_thumb_file_md5", "bytes_source", "file_lastmodified", "uint32_thumb_file_size", "uint32_busi_type", "uin32_from_chat_type", "uin32_to_chat_type", "uin32_uiOperatorFlag", "bytes_video_file_source_dir", "bool_support_progressive", "uint32_file_width", "uint32_file_height", "bytes_local_file_md5", "uint32_is_local_video", "uint32_transfered_size", "uint32_sub_busi_type" };
  public final PBBoolField bool_support_progressive = PBField.initBool(false);
  public final PBBytesField bytes_file_md5 = PBField.initBytes(ByteStringMicro.EMPTY);
  public final PBBytesField bytes_file_name = PBField.initBytes(ByteStringMicro.EMPTY);
  public final PBBytesField bytes_file_uuid = PBField.initBytes(ByteStringMicro.EMPTY);
  public final PBBytesField bytes_local_file_md5 = PBField.initBytes(ByteStringMicro.EMPTY);
  public final PBBytesField bytes_source = PBField.initBytes(ByteStringMicro.EMPTY);
  public final PBBytesField bytes_thumb_file_md5 = PBField.initBytes(ByteStringMicro.EMPTY);
  public final PBBytesField bytes_video_file_source_dir = PBField.initBytes(ByteStringMicro.EMPTY);
  public final PBUInt64Field file_lastmodified = PBField.initUInt64(0L);
  public final PBUInt32Field uin32_from_chat_type = PBField.initUInt32(0);
  public final PBUInt32Field uin32_to_chat_type = PBField.initUInt32(0);
  public final PBUInt32Field uin32_uiOperatorFlag = PBField.initUInt32(0);
  public final PBUInt32Field uint32_busi_type = PBField.initUInt32(0);
  public final PBUInt32Field uint32_file_format = PBField.initUInt32(0);
  public final PBUInt32Field uint32_file_height = PBField.initUInt32(0);
  public final PBUInt32Field uint32_file_progress = PBField.initUInt32(0);
  public final PBUInt32Field uint32_file_size = PBField.initUInt32(0);
  public final PBUInt32Field uint32_file_status = PBField.initUInt32(0);
  public final PBUInt32Field uint32_file_time = PBField.initUInt32(0);
  public final PBUInt32Field uint32_file_type = PBField.initUInt32(0);
  public final PBUInt32Field uint32_file_width = PBField.initUInt32(0);
  public final PBUInt32Field uint32_is_local_video = PBField.initUInt32(0);
  public final PBUInt32Field uint32_sub_busi_type = PBField.initUInt32(0);
  public final PBUInt32Field uint32_thumb_file_size = PBField.initUInt32(0);
  public final PBUInt32Field uint32_thumb_height = PBField.initUInt32(0);
  public final PBUInt32Field uint32_thumb_width = PBField.initUInt32(0);
  public final PBUInt32Field uint32_transfered_size = PBField.initUInt32(0);

*/
   void doParse();


public:
	std::string strPath;

private:
	CodedInputStreamMicro* codeInput;
	std::map<int, MsgForPtt::FieldType> fieldMap;
};
