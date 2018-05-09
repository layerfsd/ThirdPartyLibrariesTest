#ifndef SQLITEDATARECOVERY_H
#define SQLITEDATARECOVERY_H

#include "SQLiteDataRecoveryHeader.h"
#include "..\Utils\sqlite3.h"
#include <strsafe.h>
#include <list>
#include <set>
#include <map>


// Column
struct ColumnInfo
{
	unsigned char dataType;		// 0(NULL) 1(signed integer) 2(float/double) 3(BLOB) 4(TEXT)
	bool isPrimaryKey;			// 是否为主键
	bool isNotNull;             // 是否可以为NULL
	bool isAutoIncrement;		// 是否自增
    std::wstring columnName;    // 字段名
};

// Table
struct TableInfo
{
    RecoveryAppType recoveryAppType;                            // 恢复应用类型
    int mainColumn;                                             // 关键列
    int uniqueColumn;                                           // 唯一列
    unsigned long rootPageNumber;                               // 起始页
    std::wstring mainColumnName;                                // 关键列列名
    std::wstring uniqueColumnName;                              // 唯一列列名，该列内容唯一，作为恢复结果的过滤项
    std::wstring srcTableName;                                  // 原数据所在表名
    std::wstring dstTableName;                                  // 恢复数据所在表名
	std::wstring srcTableDDL;									// 原数据所在表创建语句
	std::wstring dstTableDDL;									// 恢复数据所在表创建语句
    std::string chatAccount;                                    // IOS 好友/群/讨论组QQ账号
    std::list<ColumnInfo> columnInfos;							// 列信息
    std::set<unsigned long> leafPageNumbers;                    // 表所有叶子页
    std::set<unsigned long> unusedPageNumbers;                  // 表所有未使用区域页
    std::set<unsigned long> overflowPageNumbers;                // 表所有溢出区域页
};

// Record
struct RecordInfo
{
    unsigned char dataType; // 0(NULL) 1(signed integer) 2(float/double) 3(BLOB) 4(TEXT)
    unsigned short dataSize;
    long long integerData;
    double doubleData;
    unsigned char *blobData;
    std::string stringData;
};



class SQLiteDataRecoveryInternal : public SQLiteDataRecovery
{
public:
    virtual void StartRecovery(const std::wstring &dbPath, const std::list<TargetTableInfo> &targetTableInfos);

private:
    std::wstring m_dbPath;
    std::wstring m_dbPathBack;
    HANDLE m_fileHandle;
    sqlite3 *m_dbHandler;
    unsigned short m_pageSize;
    unsigned long m_pageCount;
    std::list<TableInfo> m_tableInfos;
    std::list<TargetTableInfo> m_targetTableInfos;
    std::multimap<std::wstring, std::list<RecordInfo>> m_recoveryData;
    std::set<long long> m_ftsUsernameIds;
    long long m_maxFtsUsernameId;
    std::set<std::string> m_ftsUsernamesExist;

    // Android当前账号微信号
    std::wstring m_androidWeixinNumber;

    // Android微信辅助校验
    // <key, value> = <timesamp, docid>
    std::map<long long, long long> m_androidWeixinMsgIDMap;
    
    // <key, value> = <cell offset, cell RowID>
    // 用于计算IOS短信RowID
    std::map<unsigned short, unsigned long long> m_currentPageRowIDMap;

    // <key, value> = <cell offset, cell data>
    std::map<unsigned short, std::list<RecordInfo>> m_currentPageRowIDIsZeroMap;

    // Android通讯录辅助校验
    std::set<long long> m_androidContactAccountIDs;
    std::multimap<long long, std::string> m_androidDeleteContactIDMap;

    // Android短信辅助校验
    std::set<long long> m_androidMimeTypeIDs;

    // IOS通讯录辅助校验
    std::multimap<long long, std::string> m_iosContactIDMap;

private:
    inline unsigned short Get16Bits(bool isBigEndian, const unsigned char *data);

    inline unsigned int Get24Bits(bool isBigEndian, const unsigned char *data);

    inline unsigned int Get32Bits(bool isBigEndian, const unsigned char *data);

    inline unsigned long long Get40Bits(bool isBigEndian, const unsigned char *data);

    inline unsigned long long Get48Bits(bool isBigEndian, const unsigned char *data);

    inline unsigned long long Get56Bits(bool isBigEndian, const unsigned char *data);

    inline unsigned long long Get64Bits(bool isBigEndian, const unsigned char *data);

    inline double GetDoubleData(bool isBigEndian, const unsigned char *data);

    inline int putVarint64(unsigned char *p, unsigned long long v);

    inline int sqlite3PutVarint(unsigned char *p, unsigned long long v);

    inline unsigned char sqlite3GetVarint(const unsigned char *p, unsigned long long *v);

    inline void ClearList(std::list<RecordInfo> &recordInfos);

    inline void ClearData();

    void DoCheckPoint(const std::wstring &dataPath);

    // 0 - 没有溢出页 非0 - 第一个溢出页的页号
    unsigned long GetFirstOverflowPageNumber(unsigned char *cell);

    void GetTableOverflowPageNumbers(TableInfo &tableInfo);

    void GetTableLeafPageNumbers(unsigned long rootPageNumber,
        std::set<unsigned long> &pageNumbers);

    void GetTablePageNumbers(unsigned long rootPageNumber,
        std::set<unsigned long> &pageNumbers);

    void GetIndexPageNumbers(unsigned long rootPageNumber,
        std::set<unsigned long> &pageNumbers);

    void GetTableUnusedPageNumbers();

    void GetTableColumnInfo(TableInfo &tableInfo);

    int GetTableColumnType(const std::wstring &typeString);

    bool GetAllTableInfo();

    void GetOneRecordData(RecordInfo &recordInfo,
        unsigned char *recordDataBuffer);

    void GetAllRecordInfo(std::list<RecordInfo> &recordInfos,
        int &typeBegin,
        int typeEnd,
        unsigned char *&recordBegin,
        unsigned char *recordEnd);

    bool CreateNewTable(const TableInfo &tableInfo);

    // 检查恢复数据在恢复数据所在表中是否存在
    bool CheckRecoveryDataStep6(const std::list<RecordInfo> &recordInfos,
        const TableInfo &tableInfo);

    // 检查恢复数据在原数据所在表中是否存在
    bool CheckRecoveryDataStep5(const std::list<RecordInfo> &recordInfos,
        const TableInfo &tableInfo);

    // 检查恢复是否在m_recoveryData中是否存在
    bool CheckRecoveryDataStep4(const std::list<RecordInfo> &recordInfos,
        const TableInfo &tableInfo);

    // 根据应用类型检查恢复数据是否正确
    bool CheckRecoveryDataStep3(const std::list<RecordInfo> &recordInfos,
        const TableInfo &tableInfo,
        unsigned long long &RowID);

	bool CheckRecoveryDataStep2(std::list<RecordInfo> &recordInfos,
		const TableInfo &tableInfo,
		int typeBegin,
		int typeEnd,
		unsigned char *begin,
		unsigned char *end,
        unsigned char *&dataBegin,
        unsigned long long &RowID);

	bool CheckRecoveryDataStep1(const TableInfo &tableInfo,
        unsigned char *begin,
        unsigned char *end,
        unsigned char *&dataBegin,
        unsigned char *pageBuffer,
        bool isFreeBlock = false);

    bool UnusedSpaceDataRecoveryEx(unsigned char *pageBuffer,
        unsigned char *begin,
        unsigned char *end,
        std::map<unsigned short, unsigned short> &freeBlockOffsetMap,
        const TableInfo &tableInfo);

    void UnusedSpaceDataRecovery(unsigned char *pageBuffer,
        unsigned short cellCount,
        const std::map<unsigned short, unsigned short> &cellOffsetMap,
        std::map<unsigned short, unsigned short> &freeBlockOffsetMap,
        const TableInfo &tableInfo);

    void WalModeDataRecovery();

    void AndroidWeixinFTS5MetaMessageRecovery(const TableInfo &tableInfo);

    void AndroidWeixinFTS5IndexMessageRecovery(const TableInfo &tableInfo);

    void AndroidWeixinRecoveryFromFTSDb(const TableInfo &tableInfo);

    void IosWeixinRecoveryFromFTSDb(const TableInfo &tableInfo);

    bool WriteToNewTable();

    static void BlobDestructor(void *p);

    void GetAndroidContactAccountIDs();

    void GetAndroidDeleteContactIDs(const TableInfo &tableInfo);

    void GetAndroidMimeTypeIDs();

    void GetAndroidWeixinMsgIDMapExist();

    void GetAndroidWeixinMsgIDRecovery(const TableInfo &tableInfo);

    void GetAndroidWeixinNumber();

    void GetIosContactIDMap(const TableInfo &tableInfo);

    void GetIosSmsRowID(const TableInfo &tableInfo);

    void GetIosFTSUsernameIds(const TableInfo &tableInfo);

    void GetIosFTSUsernamesExist(const TableInfo &tableInfo);
};


#endif // SQLITEDATARECOVERY_H