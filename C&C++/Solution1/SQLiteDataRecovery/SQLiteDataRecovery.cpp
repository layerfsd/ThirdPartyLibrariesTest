#include "SQLiteDataRecovery.h"
#include "..\Utils\StringConvertor.h"
#include <assert.h>
#include <io.h>
#include <Shlwapi.h>
#include <DebugLog.h>

#pragma comment(lib, "Shlwapi")
#pragma comment(lib, "sqlite3")

SQLiteDataRecovery* CreateRecoveryClass()
{
    return new SQLiteDataRecoveryInternal();
}

void DestoryRecoveryClass(SQLiteDataRecovery *obj)
{
    if (obj)
        delete obj;
}

/*
 参考资料：
 1、Database File Format
 http://www.sqlite.org/fileformat2.html
 2、一种基于SQLite3文件格式的删除数据恢复方法.pdf
 3、Huffman Coding In SQLite
 https://mobileforensics.wordpress.com/2011/09/17/huffman-coding-in-sqlite-a-primer-for-mobile-forensics/
 4、A recovery method of deleted record for SQLite database
 https://www.researchgate.net/publication/226423207_A_recovery_method_of_deleted_record_for_SQLite_database
 5、Undark - a SQLite recovery tool for deleted data or corrupt database
 https://github.com/witwall/undark
*/
void SQLiteDataRecoveryInternal::StartRecovery(const std::wstring &dbPath, const std::list<TargetTableInfo> &targetTableInfos)
{
    ClearData();

#ifdef _DEBUG
    return;
#endif

    m_dbPath = dbPath;
    m_dbPathBack = m_dbPath + L"_back";
    m_targetTableInfos = targetTableInfos;

    LARGE_INTEGER offset = {0};
    unsigned char *pageBuffer = NULL;
    unsigned long readBytes = 0;
    unsigned short firstFreeBlockOffset = 0;
    unsigned short nextFreeBlockOffset = 0;
    unsigned short freeBlockSize = 0;
    unsigned short cellOffset = 0;
    unsigned short cellCount = 0;
    unsigned short cellSize = 0;
    unsigned long long v = 0;
    unsigned short len = 0;
    bool isWalMode = false;
    bool isWeixinMessageTable = false;

    // <key, value> = <offset, cellsize>
    std::map<unsigned short, unsigned short> cellOffsetMap;

    // <key, value> = <freeblock offset, freeblock cellsize>
    std::map<unsigned short, unsigned short> freeBlockOffsetMap;

    do 
    {
        std::wstring dbWalFile = m_dbPath + L"-wal";

        if (!CopyFileW(m_dbPath.c_str(), m_dbPathBack.c_str(), FALSE))
        {
            break;
        }

        isWalMode = -1 != GetFileAttributesW(dbWalFile.c_str());
        if (isWalMode)
        {
            DoCheckPoint(m_dbPath);
        }

        if (!GetAllTableInfo())
        {
            break;
        }

        pageBuffer = new unsigned char[m_pageSize]();
        if (!pageBuffer)
        {
            break;
        }

        for (std::list<TableInfo>::const_iterator iter = m_tableInfos.begin();
            iter != m_tableInfos.end();
            ++iter)
        {
            if (ANDROID_RECOVERY_WEIXIN == iter->recoveryAppType &&
                L"message" == iter->srcTableName)
            {
                isWeixinMessageTable = true;
            }
            else if (IOS_RECOVERY_WEIXIN == iter->recoveryAppType &&
                std::wstring::npos != iter->srcTableName.find(L"Chat_"))
            {
                isWeixinMessageTable = true;
            }
            else
            {
                isWeixinMessageTable = false;
            }

            if (IOS_RECOVERY_WEIXIN == iter->recoveryAppType &&
                L"fts_username_id" == iter->srcTableName)
            {
                GetIosFTSUsernameIds(*iter);
                GetIosFTSUsernamesExist(*iter);
            }

            if (!isWeixinMessageTable)
            {
                for (std::set<unsigned long>::const_iterator iter2 = iter->leafPageNumbers.begin();
                    iter2 != iter->leafPageNumbers.end();
                    ++iter2)
                {
                    offset.QuadPart = (*iter2 - 1) * m_pageSize;
                    cellOffsetMap.clear();
                    freeBlockOffsetMap.clear();
                    m_currentPageRowIDMap.clear();
                    m_currentPageRowIDIsZeroMap.clear();

                    do 
                    {
                        if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                        {
                            break;
                        }

                        memset(pageBuffer, 0, m_pageSize);
                        if (!ReadFile(m_fileHandle, pageBuffer, m_pageSize, &readBytes, NULL) || m_pageSize != readBytes)
                        {
                            break;
                        }

                        // cell 结构如下
                        //                    |-------------HeaderSize-----------|
                        // | CellSize | RowID | HeaderSize | Type 1 |...| Type N | Data 1 |...| Data N |
                        // |----Cell Header---|----------------------Cell Size-------------------------|

                        cellCount = Get16Bits(true, pageBuffer + 3);
                        for (unsigned short i = 0; i < cellCount; ++i)
                        {
                            cellOffset = Get16Bits(true, pageBuffer + 8 + i * 2);

                            // CellSize 本身所占字节数
                            len = sqlite3GetVarint(pageBuffer + cellOffset, &v);
                            cellSize = len;

                            // CellSize字节数
                            cellSize += (unsigned short)v;

                            // RowID字节数
                            cellSize += sqlite3GetVarint(pageBuffer + cellOffset + len, &v);

                            cellOffsetMap.insert(std::make_pair(cellOffset, cellSize));

                            if (IOS_RECOVERY_SMS == iter->recoveryAppType &&
                                L"handle" == iter->srcTableName)
                            {
                                m_currentPageRowIDMap.insert(std::make_pair(cellOffset, v));
                            }
                        }

                        firstFreeBlockOffset = Get16Bits(true, pageBuffer + 1);
                        if (0 != firstFreeBlockOffset)
                        {
                            do 
                            {
                                offset.QuadPart = (*iter2 - 1) * m_pageSize + firstFreeBlockOffset;
                                if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                                {
                                    continue;
                                }

                                // next freeblock address
                                if (!ReadFile(m_fileHandle, &nextFreeBlockOffset, 2, &readBytes, NULL))
                                {
                                    continue;
                                }

                                nextFreeBlockOffset = Get16Bits(true, (unsigned char*)(&nextFreeBlockOffset));

                                // current freeblock size
                                if (!ReadFile(m_fileHandle, &freeBlockSize, 2, &readBytes, NULL))
                                {
                                    firstFreeBlockOffset = nextFreeBlockOffset;
                                    continue;
                                }

                                freeBlockSize = Get16Bits(true, (unsigned char*)(&freeBlockSize));
                                freeBlockOffsetMap.insert(std::make_pair(firstFreeBlockOffset, freeBlockSize));

                                firstFreeBlockOffset = nextFreeBlockOffset;

                            } while (nextFreeBlockOffset);
                        }

                        UnusedSpaceDataRecovery(pageBuffer, cellCount, cellOffsetMap, freeBlockOffsetMap, *iter);

                    } while (false);

                    if (IOS_RECOVERY_SMS == iter->recoveryAppType &&
                        L"handle" == iter->srcTableName &&
                        !m_currentPageRowIDMap.empty() &&
                        !m_currentPageRowIDIsZeroMap.empty())
                    {
                        GetIosSmsRowID(*iter);
                    }
                }

                freeBlockOffsetMap.clear();

                // 未使用区域数据恢复

                if (!iter->unusedPageNumbers.empty())
                {
                    for (std::set<unsigned long>::const_iterator iter3 = iter->unusedPageNumbers.begin();
                        iter3 != iter->unusedPageNumbers.end();
                        ++iter3)
                    {
                        offset.QuadPart = (*iter3 - 1) * m_pageSize;

                        do 
                        {
                            if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                            {
                                break;
                            }

                            memset(pageBuffer, 0, m_pageSize);
                            if (!ReadFile(m_fileHandle, pageBuffer, m_pageSize, &readBytes, NULL) || m_pageSize != readBytes)
                            {
                                break;
                            }

                            if (UnusedSpaceDataRecoveryEx(pageBuffer, pageBuffer, pageBuffer + m_pageSize, freeBlockOffsetMap, *iter))
                            {
                                // 将其他表中此空闲页剔除
                                for (std::list<TableInfo>::iterator iter4 = m_tableInfos.begin();
                                    iter4 != m_tableInfos.end();
                                    ++iter4)
                                {
                                    if (iter->srcTableName != iter4->srcTableName)
                                    {
                                        if (iter4->unusedPageNumbers.end() != iter4->unusedPageNumbers.find(*iter3))
                                        {
                                            iter4->unusedPageNumbers.erase(*iter3);
                                        }
                                    }
                                }
                            }

                        } while (false);
                    }
                }
            }
            
            if (IOS_RECOVERY_CONTACT == iter->recoveryAppType &&
                L"ABMultiValue" == iter->srcTableName)
            {
                GetIosContactIDMap(*iter);
            }
            else if (ANDROID_RECOVERY_CONTACT == iter->recoveryAppType &&
                L"data" == iter->srcTableName)
            {
                GetAndroidDeleteContactIDs(*iter);
            }
            else if (ANDROID_RECOVERY_WEIXIN == iter->recoveryAppType &&
                L"message" == iter->srcTableName)
            {
                GetAndroidWeixinNumber();
                AndroidWeixinRecoveryFromFTSDb(*iter);
            }
            else if (ANDROID_RECOVERY_WEIXIN == iter->recoveryAppType &&
                L"FTS5IndexMessage_content" == iter->srcTableName)
            {
                AndroidWeixinFTS5IndexMessageRecovery(*iter);
            }
            else if (ANDROID_RECOVERY_WEIXIN == iter->recoveryAppType &&
                L"FTS5MetaMessage" == iter->srcTableName)
            {
                AndroidWeixinFTS5MetaMessageRecovery(*iter);
                GetAndroidWeixinMsgIDMapExist();
                GetAndroidWeixinMsgIDRecovery(*iter);
            }
            else if (IOS_RECOVERY_WEIXIN == iter->recoveryAppType &&
                std::wstring::npos != iter->srcTableName.find(L"Chat_"))
            {
                IosWeixinRecoveryFromFTSDb(*iter);
            }
        }

        if (pageBuffer)
        {
            delete [] pageBuffer;
            pageBuffer = NULL;
        }

        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
    
    } while (false);

    if (pageBuffer)
    {
        delete [] pageBuffer;
        pageBuffer = NULL;
    }

    if (INVALID_HANDLE_VALUE != m_fileHandle)
    {
        CloseHandle(m_fileHandle);
    }

    if (isWalMode)
    {
        WalModeDataRecovery();
    }

    if (!m_tableInfos.empty() && !m_recoveryData.empty())
    {
        WriteToNewTable();
    }

    if (m_dbHandler)
    {
        sqlite3_close(m_dbHandler);
    }

    DeleteFileW(m_dbPathBack.c_str());
}

unsigned short SQLiteDataRecoveryInternal::Get16Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned short)(data[1] + (data[0] << 8)) :
        (unsigned short)(data[0] + (data[1] << 8));
}

unsigned int SQLiteDataRecoveryInternal::Get24Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned int)(data[2] + (data[1] << 8) + (data[0] << 16)) :
        (unsigned int)(data[0] + (data[1] << 8) + (data[2] << 16));
}

unsigned int SQLiteDataRecoveryInternal::Get32Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned int)(data[3] + (data[2] << 8) + (data[1] << 16) + (data[0] << 24)) :
        (unsigned int)(data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24));
}

unsigned long long SQLiteDataRecoveryInternal::Get40Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned long long)(
        (unsigned long long)data[4] +
        ((unsigned long long)data[3] << 8) +
        ((unsigned long long)data[2] << 16) +
        ((unsigned long long)data[1] << 24) +
        ((unsigned long long)data[0] << 32))
        :
        (unsigned long long)(
        (unsigned long long)data[0] +
        ((unsigned long long)data[1] << 8) +
        ((unsigned long long)data[2] << 16) +
        ((unsigned long long)data[3] << 24) +
        ((unsigned long long)data[4] << 32));
}

unsigned long long SQLiteDataRecoveryInternal::Get48Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned long long)(
        (unsigned long long)data[5] +
        ((unsigned long long)data[4] << 8) +
        ((unsigned long long)data[3] << 16) +
        ((unsigned long long)data[2] << 24) +
        ((unsigned long long)data[1] << 32) +
        ((unsigned long long)data[0] << 40))
        :
        (unsigned long long)(
        (unsigned long long)data[0] +
        ((unsigned long long)data[1] << 8) +
        ((unsigned long long)data[2] << 16) +
        ((unsigned long long)data[3] << 24) +
        ((unsigned long long)data[4] << 32) +
        ((unsigned long long)data[5] << 40));
}

unsigned long long SQLiteDataRecoveryInternal::Get56Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned long long)(
        (unsigned long long)data[6] +
        ((unsigned long long)data[5] << 8) +
        ((unsigned long long)data[4] << 16) +
        ((unsigned long long)data[3] << 24) +
        ((unsigned long long)data[2] << 32) +
        ((unsigned long long)data[1] << 40) +
        ((unsigned long long)data[0] << 48))
        :
        (unsigned long long)(
        (unsigned long long)data[0] +
        ((unsigned long long)data[1] << 8) +
        ((unsigned long long)data[2] << 16) +
        ((unsigned long long)data[3] << 24) +
        ((unsigned long long)data[4] << 32) +
        ((unsigned long long)data[5] << 40) +
        ((unsigned long long)data[6] << 48));
}

unsigned long long SQLiteDataRecoveryInternal::Get64Bits(bool isBigEndian, const unsigned char *data)
{
    return isBigEndian ?
        (unsigned long long)(
        (unsigned long long)data[7] +
        ((unsigned long long)data[6] << 8) +
        ((unsigned long long)data[5] << 16) +
        ((unsigned long long)data[4] << 24) +
        ((unsigned long long)data[3] << 32) +
        ((unsigned long long)data[2] << 40) +
        ((unsigned long long)data[1] << 48) +
        ((unsigned long long)data[0] << 56))
        :
        (unsigned long long)(
        (unsigned long long)data[0] +
        ((unsigned long long)data[1] << 8) +
        ((unsigned long long)data[2] << 16) +
        ((unsigned long long)data[3] << 24) +
        ((unsigned long long)data[4] << 32) +
        ((unsigned long long)data[5] << 40) +
        ((unsigned long long)data[6] << 48) +
        ((unsigned long long)data[7] << 56));
}

double SQLiteDataRecoveryInternal::GetDoubleData(bool isBigEndian, const unsigned char *data)
{
    unsigned char buffer[8] = {0};
    
    if (isBigEndian)
    {
        buffer[7] = data[0];
        buffer[6] = data[1];
        buffer[5] = data[2];
        buffer[4] = data[3];
        buffer[3] = data[4];
        buffer[2] = data[5];
        buffer[1] = data[6];
        buffer[0] = data[7];
    }
    else
    {
        memcpy(buffer, data, 8);
    }

    return *((double*)buffer);
}

/*
** The variable-length integer encoding is as follows:
**
** KEY:
**         A = 0xxxxxxx    7 bits of data and one flag bit
**         B = 1xxxxxxx    7 bits of data and one flag bit
**         C = xxxxxxxx    8 bits of data
**
**  7 bits - A
** 14 bits - BA
** 21 bits - BBA
** 28 bits - BBBA
** 35 bits - BBBBA
** 42 bits - BBBBBA
** 49 bits - BBBBBBA
** 56 bits - BBBBBBBA
** 64 bits - BBBBBBBBC
*/

/*
** Write a 64-bit variable-length integer to memory starting at p[0].
** The length of data write will be between 1 and 9 bytes.  The number
** of bytes written is returned.
**
** A variable-length integer consists of the lower 7 bits of each byte
** for all bytes that have the 8th bit set and one byte with the 8th
** bit clear.  Except, if we get to the 9th byte, it stores the full
** 8 bits and is the last byte.
*/
int SQLiteDataRecoveryInternal::putVarint64(unsigned char *p, unsigned long long v) {
    int i, j, n;
    unsigned char buf[10];
    if (v & (((unsigned long long)0xff000000) << 32)) {
        p[8] = (unsigned char)v;
        v >>= 8;
        for (i = 7; i >= 0; i--) {
            p[i] = (unsigned char)((v & 0x7f) | 0x80);
            v >>= 7;
        }
        return 9;
    }
    n = 0;
    do {
        buf[n++] = (unsigned char)((v & 0x7f) | 0x80);
        v >>= 7;
    } while (v != 0);
    buf[0] &= 0x7f;
    assert(n <= 9);
    for (i = 0, j = n - 1; j >= 0; j--, i++) {
        p[i] = buf[j];
    }
    return n;
}

int SQLiteDataRecoveryInternal::sqlite3PutVarint(unsigned char *p, unsigned long long v) {
    if (v <= 0x7f) {
        p[0] = v & 0x7f;
        return 1;
    }
    if (v <= 0x3fff) {
        p[0] = ((v >> 7) & 0x7f) | 0x80;
        p[1] = v & 0x7f;
        return 2;
    }
    return putVarint64(p, v);
}

/*
** Bitmasks used by sqlite3GetVarint().  These precomputed constants
** are defined here rather than simply putting the constant expressions
** inline in order to work around bugs in the RVT compiler.
**
** SLOT_2_0     A mask for  (0x7f<<14) | 0x7f
**
** SLOT_4_2_0   A mask for  (0x7f<<28) | SLOT_2_0
*/
#define SLOT_2_0     0x001fc07f
#define SLOT_4_2_0   0xf01fc07f

/*
** Read a 64-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
*/
unsigned char SQLiteDataRecoveryInternal::sqlite3GetVarint(const unsigned char *p, unsigned long long *v) {
    unsigned int a, b, s;

    a = *p;
    /* a: p0 (unmasked) */
    if (!(a & 0x80))
    {
        *v = a;
        return 1;
    }

    p++;
    b = *p;
    /* b: p1 (unmasked) */
    if (!(b & 0x80))
    {
        a &= 0x7f;
        a = a << 7;
        a |= b;
        *v = a;
        return 2;
    }

    /* Verify that constants are precomputed correctly */
    assert(SLOT_2_0 == ((0x7f << 14) | (0x7f)));
    assert(SLOT_4_2_0 == ((0xfU << 28) | (0x7f << 14) | (0x7f)));

    p++;
    a = a << 14;
    a |= *p;
    /* a: p0<<14 | p2 (unmasked) */
    if (!(a & 0x80))
    {
        a &= SLOT_2_0;
        b &= 0x7f;
        b = b << 7;
        a |= b;
        *v = a;
        return 3;
    }

    /* CSE1 from below */
    a &= SLOT_2_0;
    p++;
    b = b << 14;
    b |= *p;
    /* b: p1<<14 | p3 (unmasked) */
    if (!(b & 0x80))
    {
        b &= SLOT_2_0;
        /* moved CSE1 up */
        /* a &= (0x7f<<14)|(0x7f); */
        a = a << 7;
        a |= b;
        *v = a;
        return 4;
    }

    /* a: p0<<14 | p2 (masked) */
    /* b: p1<<14 | p3 (unmasked) */
    /* 1:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
    /* moved CSE1 up */
    /* a &= (0x7f<<14)|(0x7f); */
    b &= SLOT_2_0;
    s = a;
    /* s: p0<<14 | p2 (masked) */

    p++;
    a = a << 14;
    a |= *p;
    /* a: p0<<28 | p2<<14 | p4 (unmasked) */
    if (!(a & 0x80))
    {
        /* we can skip these cause they were (effectively) done above
        ** while calculating s */
        /* a &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
        /* b &= (0x7f<<14)|(0x7f); */
        b = b << 7;
        a |= b;
        s = s >> 18;
        *v = ((unsigned long long)s) << 32 | a;
        return 5;
    }

    /* 2:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
    s = s << 7;
    s |= b;
    /* s: p0<<21 | p1<<14 | p2<<7 | p3 (masked) */

    p++;
    b = b << 14;
    b |= *p;
    /* b: p1<<28 | p3<<14 | p5 (unmasked) */
    if (!(b & 0x80))
    {
        /* we can skip this cause it was (effectively) done above in calc'ing s */
        /* b &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
        a &= SLOT_2_0;
        a = a << 7;
        a |= b;
        s = s >> 18;
        *v = ((unsigned long long)s) << 32 | a;
        return 6;
    }

    p++;
    a = a << 14;
    a |= *p;
    /* a: p2<<28 | p4<<14 | p6 (unmasked) */
    if (!(a & 0x80))
    {
        a &= SLOT_4_2_0;
        b &= SLOT_2_0;
        b = b << 7;
        a |= b;
        s = s >> 11;
        *v = ((unsigned long long)s) << 32 | a;
        return 7;
    }

    /* CSE2 from below */
    a &= SLOT_2_0;
    p++;
    b = b << 14;
    b |= *p;
    /* b: p3<<28 | p5<<14 | p7 (unmasked) */
    if (!(b & 0x80))
    {
        b &= SLOT_4_2_0;
        /* moved CSE2 up */
        /* a &= (0x7f<<14)|(0x7f); */
        a = a << 7;
        a |= b;
        s = s >> 4;
        *v = ((unsigned long long)s) << 32 | a;
        return 8;
    }

    p++;
    a = a << 15;
    a |= *p;
    /* a: p4<<29 | p6<<15 | p8 (unmasked) */

    /* moved CSE2 up */
    /* a &= (0x7f<<29)|(0x7f<<15)|(0xff); */
    b &= SLOT_2_0;
    b = b << 8;
    a |= b;

    s = s << 4;
    b = p[-4];
    b &= 0x7f;
    b = b >> 3;
    s |= b;

    *v = ((unsigned long long)s) << 32 | a;

    return 9;
}

void SQLiteDataRecoveryInternal::ClearList(std::list<RecordInfo> &recordInfos)
{
    for (std::list<RecordInfo>::iterator iter = recordInfos.begin();
        iter != recordInfos.end();
        ++iter)
    {
        if (3 == iter->dataType && iter->blobData)
        {
            delete [] iter->blobData;
            iter->blobData = NULL;
        }
    }

    recordInfos.clear();
}

void SQLiteDataRecoveryInternal::ClearData()
{
    m_dbPath.clear();
    m_dbPathBack.clear();
    m_tableInfos.clear();
    m_targetTableInfos.clear();
    m_recoveryData.clear();
    m_fileHandle = INVALID_HANDLE_VALUE;
    m_dbHandler = NULL;
    m_pageSize = 0;
    m_pageCount = 0;
    m_tableInfos.clear();
    m_androidWeixinMsgIDMap.clear();
    m_androidWeixinNumber.clear();
    m_currentPageRowIDMap.clear();
    m_currentPageRowIDIsZeroMap.clear();
    m_androidContactAccountIDs.clear();
    m_androidDeleteContactIDMap.clear();
    m_androidMimeTypeIDs.clear();
    m_iosContactIDMap.clear();
}

void SQLiteDataRecoveryInternal::DoCheckPoint(const std::wstring &dataPath)
{
    sqlite3 *db = NULL;
    int ret = -1;

    do 
    {
        ret = sqlite3_open16(dataPath.c_str(), &db);
        if (SQLITE_OK != ret)
        {
            break;
        }

        ret = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
        if (SQLITE_OK != ret)
        {
            break;
        }

        /*【不需要】如果以WAL模式打开数据库且存在WAL文件时
        SQLite会自动checkpoint
        ret = sqlite3_wal_checkpoint(db, NULL);
        if (SQLITE_OK != ret)
        {
            break;
        }*/
    } while (0);

    if (db)
    {
        sqlite3_close(db);
    }
}

unsigned long SQLiteDataRecoveryInternal::GetFirstOverflowPageNumber(unsigned char *cell)
{
    unsigned long pageNumber = 0;
    unsigned char len = 0;
    unsigned int U = 0;
    unsigned int P = 0;
    unsigned int X = 0;
    unsigned int M = 0;
    unsigned int K = 0;
    unsigned long long v = 0;

    do 
    {
        if (!cell)
        {
            break;
        }

        // CellSize
        len = sqlite3GetVarint(cell, &v);
        cell += len;

        P = (unsigned int)v;
        U = m_pageSize == 1 ? 65535 : m_pageSize;
        X = U - 35;
        M = ((U - 12) * 32 / 255) - 23;
        K = M + ((P - M) % (U - 4));

        if (P <= X)
        {
            break;
        }

        // RowID
        len = sqlite3GetVarint(cell, &v);
        cell += len;

        // first overflow page number
        if (K <= X)
        {
            cell += K;
        }
        else
        {
            cell += M;
        }

        pageNumber = Get32Bits(true, cell);

    } while (0);

    return pageNumber;
}

void SQLiteDataRecoveryInternal::GetTableOverflowPageNumbers(TableInfo &tableInfo)
{
    LARGE_INTEGER offset = {0};
    unsigned char *pageBuffer = NULL;
    unsigned long readBytes = 0;
    unsigned short cellOffset = 0;
    unsigned short cellCount = 0;
    unsigned long long cellSize = 0;
    unsigned char len = 0;

    do 
    {
        if (tableInfo.leafPageNumbers.empty())
        {
            break;
        }

        pageBuffer = new unsigned char[m_pageSize]();
        if (!pageBuffer)
        {
            break;
        }

        for (std::set<unsigned long>::const_iterator iter2 = tableInfo.leafPageNumbers.begin();
            iter2 != tableInfo.leafPageNumbers.end();
            ++iter2)
        {
            offset.QuadPart = (*iter2 - 1) * m_pageSize;
            readBytes = 0;

            do 
            {
                memset(pageBuffer, 0, m_pageSize);
                if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                {
                    break;
                }

                if (!ReadFile(m_fileHandle, pageBuffer, m_pageSize, &readBytes, NULL) || m_pageSize != readBytes)
                {
                    break;
                }

                cellCount = Get16Bits(true, pageBuffer + 3);

                // 统计所有溢出页
                for (unsigned short i = 0; i < cellCount; ++i)
                {
                    cellOffset = Get16Bits(true, pageBuffer + 8 + 2 * i);

                    unsigned long overflowPageNumber = GetFirstOverflowPageNumber(pageBuffer + cellOffset);

                    while (overflowPageNumber > 0)
                    {
                        if (overflowPageNumber > m_pageCount)
                        {
                            break;
                        }

                        tableInfo.overflowPageNumbers.insert(overflowPageNumber);;
                        offset.QuadPart = (overflowPageNumber - 1) * m_pageSize;
                        if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                        {
                            break;
                        }

                        // next overflow page address
                        if (!ReadFile(m_fileHandle, &overflowPageNumber, 4, &readBytes, NULL))
                        {
                            break;
                        }

                        overflowPageNumber = Get32Bits(true, (unsigned char*)(&overflowPageNumber));
                    }
                }
            } while (false);
        }

    } while (false);

    if (pageBuffer)
    {
        delete [] pageBuffer;
    }
}

void SQLiteDataRecoveryInternal::GetTableLeafPageNumbers(unsigned long rootPageNumber,
                                                    std::set<unsigned long> &pageNumbers)
{
    LARGE_INTEGER offset = {0};
    unsigned long readBytes = 0;
    unsigned short cellCount = 0;
    unsigned short cellOffset = 0;
    unsigned long pageNumber = 0;
    unsigned char pageHeader[12] = {0};
    unsigned char buffer[4] = {0};

    do 
    {
        if (INVALID_HANDLE_VALUE == m_fileHandle)
        {
            break;
        }

        offset.QuadPart = (rootPageNumber - 1) * m_pageSize;
        memset(pageHeader, 0, sizeof(pageHeader));

        if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
        {
            break;
        }

        if (!ReadFile(m_fileHandle, pageHeader, sizeof(pageHeader), &readBytes, NULL))
        {
            break;
        }

		if (0x0D == *pageHeader)
		{
			pageNumbers.insert(rootPageNumber);
		}
		else if (0x05 == *pageHeader)
		{
			cellCount = Get16Bits(true, pageHeader + 3);

			for (unsigned short index = 0; index < cellCount; ++index)
			{
                pageNumber = 0;
				offset.QuadPart = (rootPageNumber - 1) * m_pageSize + 12 + 2 * index;
				if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
				{
					break;
				}

                memset(buffer, 0, sizeof(buffer));
                if (!ReadFile(m_fileHandle, buffer, sizeof(buffer), &readBytes, NULL))
				{
					break;
				}

                cellOffset = Get16Bits(true, buffer);

                offset.QuadPart = (rootPageNumber - 1) * m_pageSize + cellOffset;
				if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
				{
					break;
				}

                memset(buffer, 0, sizeof(buffer));
                if (!ReadFile(m_fileHandle, buffer, sizeof(buffer), &readBytes, NULL))
				{
					break;
				}

				pageNumber = Get32Bits(true, buffer);
				if (2 < pageNumber)
				{
					GetTableLeafPageNumbers(pageNumber, pageNumbers);
				}
			}
		}

    } while (false);
}

void SQLiteDataRecoveryInternal::GetTablePageNumbers(unsigned long rootPageNumber,
                                                     std::set<unsigned long> &pageNumbers)
{
    LARGE_INTEGER offset = {0};
    unsigned long readBytes = 0;
    unsigned short cellCount = 0;
    unsigned short cellOffset = 0;
    unsigned long pageNumber = 0;
    unsigned char pageHeader[12] = {0};

    do 
    {
        if (INVALID_HANDLE_VALUE == m_fileHandle)
        {
            break;
        }

        offset.QuadPart = (rootPageNumber - 1) * m_pageSize;
        memset(pageHeader, 0, sizeof(pageHeader));

        if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
        {
            break;
        }

        if (!ReadFile(m_fileHandle, pageHeader, sizeof(pageHeader), &readBytes, NULL))
        {
            break;
        }

        if (0x0D == *pageHeader)
        {
            pageNumbers.insert(rootPageNumber);
        }
        else if (0x05 == *pageHeader)
        {
            pageNumbers.insert(rootPageNumber);

            cellCount = Get16Bits(true, pageHeader + 3);

            for (unsigned short index = 0; index < cellCount; ++index)
            {
                pageNumber = 0;
                offset.QuadPart = (rootPageNumber - 1) * m_pageSize + 12 + 2 * index;
                if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                {
                    break;
                }

                if (!ReadFile(m_fileHandle, &cellOffset, sizeof(cellOffset), &readBytes, NULL))
                {
                    break;
                }

                cellOffset = Get16Bits(true, (unsigned char*)(&cellOffset));
                offset.QuadPart = (rootPageNumber - 1) * m_pageSize + cellOffset;
                if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                {
                    break;
                }

                if (!ReadFile(m_fileHandle, &pageNumber, sizeof(pageNumber), &readBytes, NULL))
                {
                    break;
                }

                pageNumber = Get32Bits(true, (unsigned char*)(&pageNumber));
                if (2 < pageNumber)
                {
                    GetTablePageNumbers(pageNumber, pageNumbers);
                }
            }
        }

    } while (false);
}

void SQLiteDataRecoveryInternal::GetIndexPageNumbers(unsigned long rootPageNumber,
                                                         std::set<unsigned long> &pageNumbers)
{
    LARGE_INTEGER offset = {0};
    unsigned long readBytes = 0;
    unsigned short cellCount = 0;
    unsigned short cellOffset = 0;
    unsigned long pageNumber = 0;
    unsigned char pageHeader[12] = {0};

    do 
    {
        if (INVALID_HANDLE_VALUE == m_fileHandle)
        {
            break;
        }

        offset.QuadPart = (rootPageNumber - 1) * m_pageSize;
        memset(pageHeader, 0, sizeof(pageHeader));

        if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
        {
            break;
        }

        if (!ReadFile(m_fileHandle, pageHeader, sizeof(pageHeader), &readBytes, NULL))
        {
            break;
        }

        if (0x0A == *pageHeader)
        {
            pageNumbers.insert(rootPageNumber);
        }
        else if (0x02 == *pageHeader)
        {
            pageNumbers.insert(rootPageNumber);

            cellCount = Get16Bits(true, pageHeader + 3);

            for (unsigned short index = 0; index < cellCount; ++index)
            {
                pageNumber = 0;
                offset.QuadPart = (rootPageNumber - 1) * m_pageSize + 12 + 2 * index;
                if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                {
                    break;
                }

                if (!ReadFile(m_fileHandle, &cellOffset, sizeof(cellOffset), &readBytes, NULL))
                {
                    break;
                }

                cellOffset = Get16Bits(true, (unsigned char*)(&cellOffset));
                offset.QuadPart = (rootPageNumber - 1) * m_pageSize + cellOffset;
                if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
                {
                    break;
                }

                if (!ReadFile(m_fileHandle, &pageNumber, sizeof(pageNumber), &readBytes, NULL))
                {
                    break;
                }

                pageNumber = Get32Bits(true, (unsigned char*)(&pageNumber));
                if (2 < pageNumber)
                {
                    GetIndexPageNumbers(pageNumber, pageNumbers);
                }
            }
        }
    } while (false);
}

void SQLiteDataRecoveryInternal::GetTableUnusedPageNumbers()
{
    /* 2017.12.12
    * 发现一个现象：如果提前创建好目标表，目标表会占用
    * 之前的空闲页，而下面的条件会将此空闲页过滤掉，导致
    * 恢复数据不全。因此将创建目标表及写入恢复的工作统一
    * 放置最后来完成。
    */
    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    std::wstring columnName;
    std::wstring columnType;
    std::set<unsigned long> allLeafPageNumbers;
    std::set<unsigned long> tablePageNumbers;
    std::set<unsigned long> indexPageNumbers;
    unsigned long rootPageNumber = 0;
    std::wstring tableName;
    const wchar_t *data = NULL;

    do 
    {
        if (!m_dbHandler)
        {
            break;
        }

        sql = L"select rootpage, tbl_name from sqlite_master where type = 'table'";
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            while (SQLITE_ROW == sqlite3_step(stmt))
            {
                rootPageNumber = (unsigned long)sqlite3_column_int64(stmt, 0);
                if (rootPageNumber > 2)
                {
                    tablePageNumbers.clear();
                    GetTablePageNumbers(rootPageNumber, tablePageNumbers);
                }

                data = (const wchar_t*)sqlite3_column_text16(stmt, 1);
                tableName = data ? data : L"";

                if (!tablePageNumbers.empty())
                {
                    for (std::set<unsigned long>::const_iterator iterNumber = tablePageNumbers.begin();
                        iterNumber != tablePageNumbers.end();
                        ++iterNumber)
                    {
                        allLeafPageNumbers.insert(*iterNumber);
                    }
                }
            }
            sqlite3_finalize(stmt);
        }

        sql = L"select rootpage from sqlite_master where type = 'index'";
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            while (SQLITE_ROW == sqlite3_step(stmt))
            {
                rootPageNumber = (unsigned long)sqlite3_column_int64(stmt, 0);
                if (rootPageNumber > 2)
                {
                    indexPageNumbers.clear();
                    GetIndexPageNumbers(rootPageNumber, indexPageNumbers);
                }

                if (!indexPageNumbers.empty())
                {
                    for (std::set<unsigned long>::const_iterator iterNumber = indexPageNumbers.begin();
                        iterNumber != indexPageNumbers.end();
                        ++iterNumber)
                    {
                        allLeafPageNumbers.insert(*iterNumber);
                    }
                }
            }
            sqlite3_finalize(stmt);
        }

        unsigned long pageNumber = 0;
        for (std::list<TableInfo>::iterator iter = m_tableInfos.begin();
            iter != m_tableInfos.end();
            ++iter)
        {
            // 如果起始页不是叶子页，也归为未使用区域
            if (iter->leafPageNumbers.end() == iter->leafPageNumbers.find(iter->rootPageNumber))
            {
                iter->unusedPageNumbers.insert(iter->rootPageNumber);
            }

            if (1 < iter->leafPageNumbers.size())
            {
                std::set<unsigned long>::const_iterator iterLeft = iter->leafPageNumbers.begin();
                std::set<unsigned long>::const_iterator iterRight = iterLeft;
                ++iterRight;

                for (; iterRight != iter->leafPageNumbers.end();
                    ++iterLeft,
                    ++iterRight)
                {
                    for (unsigned long i = *iterLeft + 1; i < *iterRight; ++i)
                    {
                        if (allLeafPageNumbers.end() == allLeafPageNumbers.find(i))
                        {
                            iter->unusedPageNumbers.insert(i);
                        }
                    }
                }

                pageNumber = *iter->leafPageNumbers.rbegin();
                for (unsigned long i = pageNumber + 1; i <= m_pageCount; ++i)
                {
                    if (allLeafPageNumbers.end() == allLeafPageNumbers.find(i))
                    {
                        if (allLeafPageNumbers.end() == allLeafPageNumbers.find(i))
                        {
                            iter->unusedPageNumbers.insert(i);
                        }
                    }
                }
            }
            else if (1 == iter->leafPageNumbers.size())
            {
                pageNumber = *iter->leafPageNumbers.begin();

                for (unsigned long i = pageNumber + 1; i <= m_pageCount; ++i)
                {
                    if (allLeafPageNumbers.end() == allLeafPageNumbers.find(i))
                    {
                        iter->unusedPageNumbers.insert(i);
                    }
                }
            }
        }

    } while (false);
}

void SQLiteDataRecoveryInternal::GetTableColumnInfo(TableInfo &tableInfo)
{
	unsigned long long v = 0;
	unsigned long long cellSize = 0;
	unsigned long long RowID = 0;
	unsigned long long HeaderSize = 0;
	unsigned char len = 0;
	unsigned char *begin = NULL;
	unsigned char *end = NULL;
	int typeBegin = 0;
	int typeEnd = (int)tableInfo.columnInfos.size();
	std::list<RecordInfo> recordInfos;
    unsigned char *pageBuffer = NULL;
    LARGE_INTEGER offset = { 0 };
    unsigned long readBytes = 0;
    unsigned short cellCount = 0;
    unsigned short cellOffset = 0;

    do 
    {
        if (tableInfo.leafPageNumbers.empty())
        {
            break;
        }

        pageBuffer = new unsigned char[m_pageSize]();
        if (!pageBuffer)
        {
            break;
        }

        for (std::set<unsigned long>::const_iterator iter = tableInfo.leafPageNumbers.begin();
            iter != tableInfo.leafPageNumbers.end();
            ++iter)
        {
            offset.QuadPart = (*iter - 1) * m_pageSize;
            if (!SetFilePointerEx(m_fileHandle, offset, NULL, FILE_BEGIN))
            {
                break;
            }

            if (!ReadFile(m_fileHandle, pageBuffer, m_pageSize, &readBytes, NULL) || m_pageSize != readBytes)
            {
                break;
            }

            cellCount = Get16Bits(true, pageBuffer + 3);
            if (0 != cellCount)
            {
                cellOffset = Get16Bits(true, pageBuffer + 8);
                begin = pageBuffer + cellOffset;
                end = begin;
                break;
            }
        }

        if (!begin)
        {
            break;
        }

        // CellSize
        len = sqlite3GetVarint(begin, &cellSize);
        begin += len;
        end += len;
        end += cellSize;

        // RowID
        len = sqlite3GetVarint(begin, &RowID);
        begin += len;
        end += len;

        // HeadSize
        len = sqlite3GetVarint(begin, &HeaderSize);
        begin += len;

        // Type 1 - Type N
        GetAllRecordInfo(recordInfos, typeBegin, typeEnd, begin, end);

        if (recordInfos.size() != tableInfo.columnInfos.size())
        {
            break;
        }

        std::list<RecordInfo>::const_iterator iter3 = recordInfos.begin();
        std::list<ColumnInfo>::iterator iter4 = tableInfo.columnInfos.begin();
        for (; iter3 != recordInfos.end(); ++iter3, ++iter4)
        {
            iter4->dataType = iter3->dataType;
        }

        // 经查阅资料得知：SQLite只有主键且主键为整数才可以自增，且一个表中只能有一个自增列
        for (std::list<ColumnInfo>::iterator iter = tableInfo.columnInfos.begin();
            iter != tableInfo.columnInfos.end();
            ++iter)
        {
            if (iter->isPrimaryKey && 0 == iter->dataType)
            {
                iter->isAutoIncrement = true;
                iter->dataType = 1;
                break;
            }
        }

    } while (false);

    if (pageBuffer)
    {
        delete[] pageBuffer;
    }
}

int SQLiteDataRecoveryInternal::GetTableColumnType(const std::wstring &typeString)
{
    int type = 4;

    if (L"INT" == typeString ||
        L"INTEGER" == typeString ||
        L"TINYINT" == typeString ||
        L"SMALLINT" == typeString ||
        L"MEDIUMINT" == typeString ||
        L"BIGINT" == typeString ||
        L"UNSIGNED BIG INT" == typeString ||
        L"INT2" == typeString ||
        L"INT8" == typeString)
    {
        type = 1;
    }
    else if (L"REAL" == typeString ||
        L"DOUBLE" == typeString ||
        L"DOUBLE PRECISION" == typeString ||
        L"FLOAT" == typeString ||
        L"NUMERIC" == typeString ||
        std::wstring::npos != typeString.find(L"DECIMAL") ||
        L"BOOLEAN" == typeString ||
        L"DATE" == typeString ||
        L"DATETIME" == typeString )
    {
        type = 2;
    }
    else if (std::wstring::npos != typeString.find(L"CHAR") ||
        L"TEXT" == typeString ||
        L"CLOB" == typeString)
    {
        type = 4;
    }
    else if (L"BLOB" == typeString ||
        L"no datatype specified" == typeString)
    {
        type = 3;
    }

    return type;
}

bool SQLiteDataRecoveryInternal::GetAllTableInfo()
{
    const unsigned char magicHeader[] = "SQLite format 3\000";
    unsigned char dbHeader[100] = {0};
    unsigned long readBytes = 0;
    bool ret = false;
    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    const wchar_t *data = NULL;
    std::wstring columnName;
    std::wstring columnType;
    std::wstring tableName;
    std::wstring ddl;
    std::wstring iosQQFriendTableDDL;
    std::wstring iosQQDiscussTableDDL;
    std::wstring iosQQTroopTableDDL;
    std::wstring iosWeixinDDL;

    std::list<ColumnInfo> columnInfos;
    std::list<ColumnInfo> iosQQFriendTableColumnInfos;
    std::list<ColumnInfo> iosQQDiscussTableColumnInfos;
    std::list<ColumnInfo> iosQQTroopTableColumnInfos;
    std::list<ColumnInfo> iosWeixinColumnInfos;

    DWORD error = 0;

    do 
    {
        if (m_targetTableInfos.empty())
        {
            break;
        }

        for (std::list<TargetTableInfo>::const_iterator iter = m_targetTableInfos.begin();
            iter != m_targetTableInfos.end();
            ++iter)
        {
            TableInfo tableInfo;
            tableInfo.columnInfos.clear();
            tableInfo.srcTableDDL.clear();
            tableInfo.dstTableDDL.clear();
            tableInfo.rootPageNumber = 0;
            tableInfo.chatAccount = iter->chatAccount;
            tableInfo.mainColumn = 0;
            tableInfo.uniqueColumn = 0;
            tableInfo.mainColumnName = iter->mainColumnName;
            tableInfo.uniqueColumnName = iter->uniqueColumnName;
            tableInfo.leafPageNumbers.clear();
            tableInfo.unusedPageNumbers.clear();
            tableInfo.overflowPageNumbers.clear();
            tableInfo.recoveryAppType = iter->recoveryAppType;
            tableInfo.srcTableName = iter->srcTableName;
            tableInfo.dstTableName = iter->dstTableName;

            m_tableInfos.push_back(tableInfo);
        }

        m_fileHandle = CreateFileW(
            m_dbPath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);

        if (INVALID_HANDLE_VALUE == m_fileHandle)
        {
            error = GetLastError();
            break;
        }

        if (!ReadFile(m_fileHandle, dbHeader, sizeof(dbHeader), &readBytes, NULL))
        {
            break;
        }

        if (0 != memcmp(dbHeader, magicHeader, 16))
        {
            break;
        }

        if (SQLITE_OK != sqlite3_open16(m_dbPath.c_str(), &m_dbHandler))
        {
            m_dbHandler = NULL;
            break;
        }

        sql = L"PRAGMA page_size";
        if (SQLITE_OK != sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            break;
        }

        if (SQLITE_ROW == sqlite3_step(stmt))
        {
            m_pageSize = (unsigned short)sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);

        if (0 == m_pageSize)
        {
            break;
        }

        sql = L"PRAGMA page_count";
        if (SQLITE_OK != sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            break;
        }

        if (SQLITE_ROW == sqlite3_step(stmt))
        {
            m_pageCount = (unsigned short)sqlite3_column_int(stmt, 0);
        }

        sqlite3_finalize(stmt);

        if (0 == m_pageCount)
        {
            break;
        }

        for (std::list<TableInfo>::iterator iter = m_tableInfos.begin();
            iter != m_tableInfos.end();
            )
        {
            sql = L"PRAGMA table_info(" + iter->srcTableName + L")";
            if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
            {
                int index = 1;
                ddl = L" (";
                while (SQLITE_ROW == sqlite3_step(stmt))
                {
                    data = (const wchar_t*)(sqlite3_column_text16(stmt, 1));
                    columnName = data != NULL ? data : L"";
                    data = (const wchar_t*)(sqlite3_column_text16(stmt, 2));
                    columnType = data != NULL ? data : L"";

                    if (columnName == iter->uniqueColumnName)
                    {
                        iter->uniqueColumn = index;
                    }
                    else if (columnName == iter->mainColumnName)
                    {
                        iter->mainColumn = index;
                    }

                    ColumnInfo columnInfo;
                    columnInfo.columnName = columnName;

                    if (L"ABPersonFullTextSearch_content" != iter->srcTableName)
                    {
                        columnInfo.dataType = GetTableColumnType(columnType);
                    }
                    else
                    {
                        if (L"docid" != columnName)
                        {
                            columnInfo.dataType = 4;
                            columnType = L"INTEGER";
                        }
                        else
                        {
                            columnInfo.dataType = 1;
                            columnType = L"TEXT";
                        }
                    }

                    ddl += columnName + L" " + columnType + L",";

                    columnInfo.isPrimaryKey = 1 == sqlite3_column_int64(stmt, 5);
                    columnInfo.isAutoIncrement = false;
                    columnInfo.isNotNull = 1 == sqlite3_column_int64(stmt, 3);
                    iter->columnInfos.push_back(columnInfo);
                    ++index;
                }
                sqlite3_finalize(stmt);
            }

            unsigned long rootPageNumber = 0;
            sql = L"select rootpage, sql from sqlite_master where type = 'table' and tbl_name = '" + iter->srcTableName + L"'";
            if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
            {
                if (SQLITE_ROW == sqlite3_step(stmt))
                {
					rootPageNumber = (unsigned long)sqlite3_column_int64(stmt, 0);
                    iter->rootPageNumber = rootPageNumber;

					data = (const wchar_t*)(sqlite3_column_text16(stmt, 1));
					iter->srcTableDDL = data != NULL ? data : L"";
                }
                sqlite3_finalize(stmt);
            }

            if (L" (" != ddl)
            {
                ddl.erase(ddl.size() - 1);
                ddl += L")";
                iter->dstTableDDL = L"create table " + iter->dstTableName + ddl;

                if (2 < rootPageNumber)
                {
                    GetTableLeafPageNumbers(rootPageNumber, iter->leafPageNumbers);
                }
                else
                {
                    iter = m_tableInfos.erase(iter);
                    continue;
                }

                if (iter->leafPageNumbers.empty())
                {
                    iter = m_tableInfos.erase(iter);
                    continue;
                }

                GetTableOverflowPageNumbers(*iter);

                GetTableColumnInfo(*iter);   

                if (IOS_RECOVERY_QQ == iter->recoveryAppType &&
                    std::wstring::npos != iter->srcTableName.find(L"tb_c2cMsg"))
                {
                    iosQQFriendTableDDL = ddl;
                    iosQQFriendTableColumnInfos = iter->columnInfos;
                }
                else if (IOS_RECOVERY_QQ == iter->recoveryAppType &&
                    std::wstring::npos != iter->srcTableName.find(L"tb_discussGrp"))
                {
                    iosQQDiscussTableDDL = ddl;
                    iosQQDiscussTableColumnInfos = iter->columnInfos;
                }
                else if (IOS_RECOVERY_QQ == iter->recoveryAppType &&
                    std::wstring::npos != iter->srcTableName.find(L"tb_TroopMsg"))
                {
                    iosQQTroopTableDDL = ddl;
                    iosQQTroopTableColumnInfos = iter->columnInfos;
                }
                else if (IOS_RECOVERY_WEIXIN == iter->recoveryAppType &&
                    std::wstring::npos != iter->srcTableName.find(L"Chat_"))
                {
                    iosWeixinDDL = ddl;
                    iosWeixinColumnInfos = iter->columnInfos;
                }
            }

            ++iter;
        }

        if (m_tableInfos.empty())
        {
            break;
        }

        for (std::list<TableInfo>::iterator iter2 = m_tableInfos.begin();
            iter2 != m_tableInfos.end();
            )
        {
            if (iter2->dstTableDDL.empty())
            {
                if (IOS_RECOVERY_QQ == iter2->recoveryAppType &&
                    std::wstring::npos != iter2->srcTableName.find(L"tb_c2cMsg"))
                {
                    if (!iosQQFriendTableDDL.empty())
                    {
                        iter2->dstTableDDL = L"create table " + iter2->dstTableName + iosQQFriendTableDDL;
                        iter2->columnInfos = iosQQFriendTableColumnInfos;
                    }
                }
                else if (IOS_RECOVERY_QQ == iter2->recoveryAppType &&
                    std::wstring::npos != iter2->srcTableName.find(L"tb_discussGrp"))
                {
                    if (!iosQQDiscussTableDDL.empty())
                    {
                        iter2->dstTableDDL = L"create table " + iter2->dstTableName + iosQQDiscussTableDDL;
                        iter2->columnInfos = iosQQDiscussTableColumnInfos;
                    }
                }
                else if (IOS_RECOVERY_QQ == iter2->recoveryAppType &&
                    std::wstring::npos != iter2->srcTableName.find(L"tb_TroopMsg"))
                {
                    if (!iosQQTroopTableDDL.empty())
                    {
                        iter2->dstTableDDL = L"create table " + iter2->dstTableName + iosQQTroopTableDDL;
                        iter2->columnInfos = iosQQTroopTableColumnInfos;
                    }
                }
                else if (IOS_RECOVERY_WEIXIN == iter2->recoveryAppType &&
                    std::wstring::npos != iter2->srcTableName.find(L"Chat_"))
                {
                    if (!iosWeixinDDL.empty())
                    {
                        iter2->dstTableDDL = L"create table " + iter2->dstTableName + iosWeixinDDL;
                        iter2->columnInfos = iosWeixinColumnInfos;
                    }
                }
            }

            if (iter2->dstTableDDL.empty())
            {
                iter2 = m_tableInfos.erase(iter2);
                continue;
            }

            ++iter2;
        }

        if (m_tableInfos.empty())
        {
            break;
        }

        GetTableUnusedPageNumbers();

        // 从空闲页中剔除溢出页
        std::set<unsigned long> allOverflowPageNumbers;
        for (std::list<TableInfo>::iterator iter = m_tableInfos.begin();
            iter != m_tableInfos.end();
            ++iter)
        {
            if (!iter->overflowPageNumbers.empty())
            {
                for (std::set<unsigned long>::iterator iterNumber = iter->overflowPageNumbers.begin();
                    iterNumber != iter->overflowPageNumbers.end();
                    ++iterNumber)
                {
                    allOverflowPageNumbers.insert(*iterNumber);
                }
            }
        }

        for (std::list<TableInfo>::iterator iter = m_tableInfos.begin();
            iter != m_tableInfos.end();
            ++iter)
        {
            if (!iter->unusedPageNumbers.empty())
            {
                for (std::set<unsigned long>::iterator iterNumber = iter->unusedPageNumbers.begin();
                    iterNumber != iter->unusedPageNumbers.end();
                    )
                {
                    if (allOverflowPageNumbers.end() != allOverflowPageNumbers.find(*iterNumber))
                    {
                        iterNumber = iter->unusedPageNumbers.erase(iterNumber);
                        continue;
                    }

                    ++iterNumber;
                }
            }
        }

        if (ANDROID_RECOVERY_CONTACT == m_tableInfos.begin()->recoveryAppType)
        {
            GetAndroidContactAccountIDs();
            GetAndroidMimeTypeIDs();
        }

        ret = true;
    } while (false);


    return ret;
}

void SQLiteDataRecoveryInternal::GetOneRecordData(RecordInfo &recordInfo,
                              unsigned char *recordDataBuffer)
{
    unsigned char content[8];
    
    do 
    {
        if (!recordDataBuffer)
        {
            break;
        }

        if (recordInfo.dataSize > m_pageSize)
        {
            break;
        }

        unsigned char *buffer = new unsigned char[recordInfo.dataSize + 1]();
        if (buffer)
        {
            memcpy(buffer, recordDataBuffer, recordInfo.dataSize);
            memset(content, 0, sizeof(content));
            switch (recordInfo.dataType)
            {
            case 1:
                {
                    switch (recordInfo.dataSize)
                    {
                    case 0:
                        {
                        }
                        break;
                    case 1:
                        {
                            recordInfo.integerData = *buffer;
                        }
                        break;
                    case 2:
                        {
                            recordInfo.integerData = Get16Bits(true, buffer);
                        }
                        break;
                    case 3:
                        {
                            recordInfo.integerData = Get24Bits(true, buffer);
                        }
                        break;
                    case 4:
                        {
                            recordInfo.integerData = Get32Bits(true, buffer);
                        }
                        break;
                    case 5:
                        {
                            recordInfo.integerData = Get40Bits(true, buffer);
                        }
                        break;
                    case 6:
                        {
                            recordInfo.integerData = Get48Bits(true, buffer);
                        }
                        break;
                    case 7:
                        {
                            recordInfo.integerData = Get56Bits(true, buffer);
                        }
                        break;
                    case 8:
                        {
                            recordInfo.integerData = Get64Bits(true, buffer);
                        }
                        break;
                    default:
                        break;
                    }
                }
                break;
            case 2:
                {
                    recordInfo.doubleData = GetDoubleData(true, buffer);
                }
                break;
            case 3:
                {
                    recordInfo.blobData = new unsigned char[recordInfo.dataSize]();
                    if (recordInfo.blobData)
                    {
                        memcpy(recordInfo.blobData, buffer, recordInfo.dataSize);
                    }
                }
                break;
            case 4:
                {
                    recordInfo.stringData = (char*)buffer;
                }
                break;
            default:
                break;
            }

            delete [] buffer;
        }
    } while (0);
}

void SQLiteDataRecoveryInternal::GetAllRecordInfo(std::list<RecordInfo> &recordInfos,
	int &typeBegin,
	int typeEnd,
	unsigned char *&recordBegin,
	unsigned char *recordEnd)
{
    if (!recordBegin || !recordEnd)
    {
        return;
    }

    unsigned long long v = 0;
    unsigned short recordDataSize = 0;
    unsigned char len = 0;

    for (; typeBegin < typeEnd; ++typeBegin)
    {
		if (recordBegin >= recordEnd)
		{
			break;
		}

        // Type N
        len = sqlite3GetVarint(recordBegin, &v);
        recordBegin += len;
        RecordInfo recordInfo;
        recordInfo.dataSize = 0;
        recordInfo.dataType = 0;
        recordInfo.blobData = NULL;
        recordInfo.doubleData = 0.0;
        recordInfo.integerData = 0;
        recordInfo.stringData.clear();

        switch (v)
        {
        case 0:
            {
                recordInfo.dataSize = 0;
                recordInfo.dataType = 0;
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            {
                recordInfo.dataSize = (unsigned short)v;
                recordInfo.dataType = 1;
            }
            break;
        case 5:
            {
                recordInfo.dataSize = 6;
                recordInfo.dataType = 1;
            }
            break;
        case 6:
            {
                recordInfo.dataSize = 8;
                recordInfo.dataType = 1;
            }
            break;
        case 7:
            {
                recordInfo.dataSize = 8;
                recordInfo.dataType = 2;
            }
            break;
        case 8:
            {
                recordInfo.dataSize = 0;
                recordInfo.dataType = 1;
                recordInfo.integerData = 0;
            }
            break;
        case 9:
            {
                recordInfo.dataSize = 0;
                recordInfo.dataType = 1;
                recordInfo.integerData = 1;
            }
            break;
        default:
            {
                if (v >= 12 && v % 2 == 0)
                {
                    recordInfo.dataSize = (unsigned short)((v - 12) / 2);
                    recordInfo.dataType = 3;
                }
                else if (v >= 13 && v % 2 == 1)
                {
                    recordInfo.dataSize = (unsigned short)((v - 13) / 2);
                    recordInfo.dataType = 4;
                }
            }
            break;
        }

        recordInfos.push_back(recordInfo);
    }
}

bool SQLiteDataRecoveryInternal::CreateNewTable(const TableInfo &tableInfo)
{
    if (!m_dbHandler)
    {
        return false;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    bool isExist = false;
    const wchar_t *data = NULL;
    bool ret = false;
    int code = -1;

    do 
    {
        sql = L"select name from sqlite_master where type = 'table' and name = '" + tableInfo.dstTableName + L"'";
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            if (SQLITE_ROW == sqlite3_step(stmt))
            {
                data = (const wchar_t*)(sqlite3_column_text16(stmt, 0));
                std::wstring name = data != NULL ? data : L"";
                if (name == tableInfo.dstTableName)
                {
                    isExist = true;
                }
            }

            sqlite3_finalize(stmt);
        }

        if (isExist)
        {
            ret = true;
            break;
        }

        sql = tableInfo.dstTableDDL;
        code = sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL);
        if (SQLITE_OK == code)
        {
            code = sqlite3_step(stmt);
            if (SQLITE_DONE == code)
            {
                ret = true;
            }
            sqlite3_finalize(stmt);
        }

    } while (false);

    return ret;
}

bool SQLiteDataRecoveryInternal::CheckRecoveryDataStep6(const std::list<RecordInfo> &recordInfos,
                                                        const TableInfo &tableInfo)
{
    if (tableInfo.uniqueColumnName.empty())
    {
        return false;
    }

    bool isExist = false;
    bool shouldBreak = false;

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    wchar_t buffer[128] = {0};
    int index = 1;

    do 
    {
        if (!m_dbHandler)
        {
            break;
        }

        index = 1;
        for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
            iter != recordInfos.end();
            ++iter, ++index)
        {
            if (shouldBreak)
            {
                break;
            }

            if (index == tableInfo.uniqueColumn)
            {
                shouldBreak = true;

                switch (iter->dataType)
                {
                case 0:
                    break;
                case 1:
                    {
                        StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                        sql = L"select * from " + tableInfo.dstTableName + L" where " + tableInfo.uniqueColumnName + L" = " + std::wstring(buffer);
                    }
                    break;
                case 2:
                    {
                        StringCbPrintfW(buffer, sizeof(buffer), L"%f", iter->doubleData);
                        sql = L"select * from " + tableInfo.dstTableName + L" where " + tableInfo.uniqueColumnName + L" = " + std::wstring(buffer);
                    }
                    break;
                case 3:
                    {
                        if (iter->blobData && iter->dataSize)
                        {
                            wchar_t *buffer2 = new wchar_t[2 * iter->dataSize + 2]();
                            if (buffer2)
                            {
                                for (unsigned short i = 0; i < iter->dataSize; ++i)
                                {
                                    StringCchPrintfW(buffer2, 2 * iter->dataSize + 2, L"%s%02X", buffer2, iter->blobData[i]);
                                }

                                sql = L"select * from " + tableInfo.dstTableName + L" where hex(" + tableInfo.uniqueColumnName + L") = '" + std::wstring(buffer2) + L"'";

                                delete [] buffer2;
                            }
                        }
                    }
                    break;
                case 4:
                    {
                        sql = L"select * from " + tableInfo.dstTableName + L" where " + tableInfo.uniqueColumnName + L" = '" + StringConvertor::Utf8ToUnicode(iter->stringData) + L"'";
                    }
                    break;
                default:
                    break;
                }

                if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
                {
                    if (SQLITE_ROW == sqlite3_step(stmt))
                    {
                        isExist = true;
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }
    } while (false);

    return isExist;
}

bool SQLiteDataRecoveryInternal::CheckRecoveryDataStep5(const std::list<RecordInfo> &recordInfos,
                                                    const TableInfo &tableInfo)
{
    if (tableInfo.uniqueColumnName.empty())
    {
        return false;
    }

    sqlite3 *dbHandler = NULL;
    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    wchar_t buffer[128] = {0};
    int index = 1;
    bool isExist = false;
    bool shouldBreak = false;

    do 
    {
        if (!m_dbHandler)
        {
            break;
        }

        for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
            iter != recordInfos.end();
            ++iter, ++index)
        {
            if (shouldBreak)
            {
                break;
            }

            if (index == tableInfo.uniqueColumn)
            {
                shouldBreak = true;

                switch (iter->dataType)
                {
                case 0:
                    break;
                case 1:
                    {
                        StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                        sql = L"select * from " + tableInfo.srcTableName + L" where " + tableInfo.uniqueColumnName + L" = " + std::wstring(buffer);
                    }
                    break;
                case 2:
                    {
                        StringCbPrintfW(buffer, sizeof(buffer), L"%f", iter->doubleData);
                        sql = L"select * from " + tableInfo.srcTableName + L" where " + tableInfo.uniqueColumnName + L" = " + std::wstring(buffer);
                    }
                    break;
                case 3:
                    {
                        if (iter->blobData && iter->dataSize)
                        {
                            wchar_t *buffer2 = new wchar_t[2 * iter->dataSize + 2]();
                            if (buffer2)
                            {
                                for (unsigned short i = 0; i < iter->dataSize; ++i)
                                {
                                    StringCchPrintfW(buffer2, 2 * iter->dataSize + 2, L"%s%02X", buffer2, iter->blobData[i]);
                                }

                                sql = L"select * from " + tableInfo.srcTableName + L" where hex(" + tableInfo.uniqueColumnName + L") = '" + std::wstring(buffer2) + L"'";

                                delete [] buffer2;
                            }
                        }
                    }
                    break;
                case 4:
                    {
                        sql = L"select * from " + tableInfo.srcTableName + L" where " + tableInfo.uniqueColumnName + L" = '" + StringConvertor::Utf8ToUnicode(iter->stringData) + L"'";
                    }
                    break;
                default:
                    break;
                }

                if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
                {
                    if (SQLITE_ROW == sqlite3_step(stmt))
                    {
                        isExist = true;
                    }
                    sqlite3_finalize(stmt);
                }
            }
        }

    } while (false);

    return isExist;
}

bool SQLiteDataRecoveryInternal::CheckRecoveryDataStep4(const std::list<RecordInfo> &recordInfos,
                                                        const TableInfo &tableInfo)
{
    if (m_recoveryData.end() == m_recoveryData.find(tableInfo.srcTableName))
    {
        return false;
    }

    if (tableInfo.uniqueColumnName.empty())
    {
        return false;
    }

    bool shouldBreak = false;
    bool isExist = false;
    std::list<ColumnInfo>::const_iterator iterColumn1 = tableInfo.columnInfos.begin();
    std::list<ColumnInfo>::const_iterator iterColumn2 = tableInfo.columnInfos.begin();

    // 根据具体应用类型校验
    switch (tableInfo.recoveryAppType)
    {
    case ANDROID_RECOVERY_CONTACT:
        {
            if (L"data" == tableInfo.srcTableName)
            {
                
            }
            else if (L"raw_contacts" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;

                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                    	break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }
                                    
                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    case ANDROID_RECOVERY_SMS:
        {
            if (L"sms" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;
                                    
                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    case ANDROID_RECOVERY_RECORD:
        {
            if (L"calls" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;
                                    
                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    case ANDROID_RECOVERY_WHATSAPP:
        {

        }
        break;
    case IOS_RECOVERY_CONTACT:
        {
            if (L"ABPersonFullTextSearch_content" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;
                                    
                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
            else if (L"ABMultiValue" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;
                                    
                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_SMS:
        {
            if (L"message" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;
                                    
                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
            else if (L"handle" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;
                                    
                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_RECORD:
        {
            if (L"ZCALLRECORD" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;
                                    
                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_QQ:
        {
            iterColumn1 = tableInfo.columnInfos.begin();
            for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                iter1 != recordInfos.end();
                ++iter1, ++iterColumn1)
            {
                if (shouldBreak)
                {
                    break;
                }

                if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                {
                    std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                        std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                    while (findPair.first != findPair.second)
                    {
                        if (shouldBreak)
                        {
                            break;
                        }

                        iterColumn2 = tableInfo.columnInfos.begin();
                        for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                            iter2 != findPair.first->second.end();
                            ++iter2, ++iterColumn2)
                        {
                            if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                            {
                                shouldBreak = true;

                                switch (iterColumn2->dataType)
                                {
                                case 0:
                                    break;
                                case 1:
                                    {
                                        isExist = iter1->integerData == iter2->integerData;
                                    }
                                    break;
                                case 2:
                                    {
                                        isExist = iter1->doubleData == iter2->doubleData;
                                    }
                                    break;
                                case 3:
                                    {
                                        if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                        {
                                            isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                        }
                                    }
                                    break;
                                case 4:
                                    {
                                        if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                        {
                                            isExist = true;
                                        }
                                    }
                                    break;
                                default:
                                    break;
                                }

                                break;
                            }
                        }

                        ++findPair.first;
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_WEIXIN:
        {
            if (std::wstring::npos != tableInfo.srcTableName.find(L"Chat_"))
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;

                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
            else if (L"Friend" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;

                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_WHATSAPP:
        {
            if (L"ZWAMESSAGE" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;

                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_TELEGRAM:
        {
            if (L"messages_v29" == tableInfo.srcTableName)
            {
                iterColumn1 = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter1 = recordInfos.begin();
                    iter1 != recordInfos.end();
                    ++iter1, ++iterColumn1)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn1->columnName)
                    {
                        std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                            std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

                        while (findPair.first != findPair.second)
                        {
                            if (shouldBreak)
                            {
                                break;
                            }

                            iterColumn2 = tableInfo.columnInfos.begin();
                            for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                                iter2 != findPair.first->second.end();
                                ++iter2, ++iterColumn2)
                            {
                                if (tableInfo.uniqueColumnName == iterColumn2->columnName)
                                {
                                    shouldBreak = true;

                                    switch (iterColumn2->dataType)
                                    {
                                    case 0:
                                        break;
                                    case 1:
                                        {
                                            isExist = iter1->integerData == iter2->integerData;
                                        }
                                        break;
                                    case 2:
                                        {
                                            isExist = iter1->doubleData == iter2->doubleData;
                                        }
                                        break;
                                    case 3:
                                        {
                                            if (iter1->blobData && iter2->blobData && (iter1->dataSize == iter2->dataSize))
                                            {
                                                isExist = !!memcmp(iter1->blobData, iter2->blobData, iter2->dataSize);
                                            }
                                        }
                                        break;
                                    case 4:
                                        {
                                            if (!iter1->stringData.empty() && iter1->stringData == iter2->stringData)
                                            {
                                                isExist = true;
                                            }
                                        }
                                        break;
                                    default:
                                        break;
                                    }

                                    break;
                                }
                            }

                            ++findPair.first;
                        }
                    }
                }
            }
        }
        break;
    default:
        break;
    }

    return isExist;
}

bool SQLiteDataRecoveryInternal::CheckRecoveryDataStep3(const std::list<RecordInfo> &recordInfos,
                                                        const TableInfo &tableInfo,
                                                        unsigned long long &RowID)
{
    if (recordInfos.empty())
    {
        return false;
    }

    bool shouldBreak = false;
    bool isFind = false;

    // 根据具体应用类型校验
    switch (tableInfo.recoveryAppType)
    {
    case ANDROID_RECOVERY_CONTACT:
        {
            if (L"data" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"mimetype_id" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (m_androidMimeTypeIDs.end() == m_androidMimeTypeIDs.find(iter->integerData))
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                            else
                            {
                                isFind = true;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"raw_contact_id" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 == iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"is_read_only" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData && 1 != iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"is_primary" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData && 1 != iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"is_super_primary" == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData && 1 != iter->integerData)
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
            else if (L"raw_contacts" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"account_id" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (m_androidContactAccountIDs.end() == m_androidContactAccountIDs.find(iter->integerData))
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                            else
                            {
                                isFind = true;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"raw_contact_is_read_only" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData && 1 != iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"deleted" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData && 1 != iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"display_name" == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (4 == iter->dataType &&
                            !iter->stringData.empty() &&
                            !m_androidDeleteContactIDMap.empty())
                        {
                            bool isFindContactID = false;
                            if (m_tableInfos.size() == 2)
                            {
                                for (std::multimap<long long, std::string>::const_iterator iter2 = m_androidDeleteContactIDMap.begin();
                                    iter2 != m_androidDeleteContactIDMap.end();
                                    ++iter2)
                                {
                                    if (isFindContactID)
                                    {
                                        break;
                                    }

                                    if (iter->stringData == iter2->second)
                                    {
                                        isFindContactID = true;
                                        RowID = iter2->first;
                                    }
                                }
                            }

                            isFind = isFindContactID;
                        }
                    }
                }
            }
        }
        break;
    case ANDROID_RECOVERY_SMS:
        {
            if (L"sms" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (13 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                            else
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"address" == iterColumn->columnName)
                    {
                        if (4 == iter->dataType)
                        {
                            if (iter->stringData.empty())
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"read" == iterColumn->columnName)
                    {
                        shouldBreak = true;
                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData && 1 != iter->integerData)
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    case ANDROID_RECOVERY_RECORD:
        {
            if (L"calls" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (13 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                        }
                    }
                    else if (L"countryiso" == iterColumn->columnName)
                    {
                        shouldBreak = true;
                        if (4 == iter->dataType)
                        {
                            if ("CN" != iter->stringData && "315" != iter->stringData/*Oppo手机*/)
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    case ANDROID_RECOVERY_WHATSAPP:
        {

        }
        break;
    case ANDROID_RECOVERY_WEIXIN:
        {
            if (L"message" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"msgId" == iterColumn->columnName)
                    {
                        if (0 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"msgSvrId" == iterColumn->columnName)
                    {
                        if (1 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"isSend" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData &&
                                1 != iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"createTime" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (13 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                            else
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (tableInfo.mainColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;
                    }
                }
            }
            else if (L"FTS5IndexMessage_content" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"id" == iterColumn->columnName)
                    {
                        if (0 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"c0" == iterColumn->columnName)
                    {
                        shouldBreak = true;
                        if (4 != iter->dataType)
                        {
                            isFind = false;
                        }
                        else
                        {
                            std::wstring unicodeString = StringConvertor::Utf8ToUnicode(iter->stringData);
                            size_t size = unicodeString.size();
                            for (size_t i = 0; i < size; ++i)
                            {
                                if (unicodeString[i] > 0x20 && unicodeString[i] < 0xD7A3)
                                {
                                    isFind = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else if (L"FTS5MetaMessage" == tableInfo.srcTableName)
            {
                long long timestamp = 0;
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"timestamp" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (13 != wcslen(buffer) || L'1' != buffer[0])
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                            else
                            {
                                timestamp = iter->integerData;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"status" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                            else
                            {
                                isFind = true;
                                m_androidWeixinMsgIDMap.insert(std::make_pair(timestamp, 0));
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_CONTACT:
        {
            if (L"ABPersonFullTextSearch_content" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"docid" == iterColumn->columnName)
                    {
                        if (0 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (tableInfo.mainColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (4 == iter->dataType &&
                            !iter->stringData.empty() &&
                            !m_iosContactIDMap.empty())
                        {
                            bool isFindRecordID = false;
                            for (std::multimap<long long, std::string>::const_iterator iter2 = m_iosContactIDMap.begin();
                                iter2 != m_iosContactIDMap.end();
                                ++iter2)
                            {
                                if (isFindRecordID)
                                {
                                    break;
                                }

                                if (iter->stringData.size() >= iter2->second.size())
                                {
                                    isFindRecordID = true;
                                    for (size_t j = 0; j < iter2->second.size(); ++j)
                                    {
                                        if (iter->stringData[j] != iter2->second[j])
                                        {
                                            isFindRecordID = false;
                                            break;
                                        }
                                    }

                                    if (isFindRecordID)
                                    {
                                        RowID = iter2->first;
                                    }
                                }
                            }

                            isFind = isFindRecordID;
                        }
                    }
                }
            }
            else if (L"ABMultiValue" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"record_id" == iterColumn->columnName)
                    {
                        if (1 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"value" == iterColumn->columnName)
                    {
                        if (4 == iter->dataType)
                        {
                            if (iter->stringData.empty())
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (4 == iter->dataType &&
                            36 == iter->stringData.size() &&
                            '-' == iter->stringData[8] &&
                            '-' == iter->stringData[13] &&
                            '-' == iter->stringData[18] &&
                            '-' == iter->stringData[23])
                        {
                            isFind = true;
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_SMS:
        {
            if (L"message" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (4 == iter->dataType &&
                            36 == iter->stringData.size() &&
                            '-' == iter->stringData[8] &&
                            '-' == iter->stringData[13] &&
                            '-' == iter->stringData[18] &&
                            '-' == iter->stringData[23])
                        {
                            isFind = true;
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
            else if (L"handle" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"service" == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (4 == iter->dataType )
                        {
                            if (("SMS" != iter->stringData &&
                                "iMessage" != iter->stringData))
                            {
                                isFind = false;
                            }
                            else
                            {
                                isFind = true;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_RECORD:
        {
            if (L"ZCALLRECORD" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (4 == iter->dataType &&
                            36 == iter->stringData.size() &&
                            '-' == iter->stringData[8] &&
                            '-' == iter->stringData[13] &&
                            '-' == iter->stringData[18] &&
                            '-' == iter->stringData[23])
                        {
                            isFind = true;
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                    else if (L"ZDATE" == iterColumn->columnName)
                    {
                        if (2 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_QQ:
        {
            if (std::wstring::npos != tableInfo.srcTableName.find(L"tb_c2cMsg"))
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"uin" == iterColumn->columnName)
                    {
                        if (4 == iter->dataType)
                        {
                            if (iter->stringData.empty() || tableInfo.chatAccount != iter->stringData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (10 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                            else
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }

                }
            }
            else if (std::wstring::npos != tableInfo.srcTableName.find(L"tb_TroopMsg"))
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (10 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                            else
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
            else if (std::wstring::npos != tableInfo.srcTableName.find(L"tb_discussGrp"))
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"DiscussUin" == iterColumn->columnName)
                    {
                        if (4 == iter->dataType)
                        {
                            if (iter->stringData.empty() || tableInfo.chatAccount != iter->stringData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (10 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                            else
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_WEIXIN:
        {
            if (std::wstring::npos != tableInfo.srcTableName.find(L"Chat_"))
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"MesSvrID" == iterColumn->columnName)
                    {
                        if (1 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (10 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                            else
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
            else if (L"Friend" == tableInfo.srcTableName)
            {
            }
            else if (L"fts_message_table_0_content" == tableInfo.srcTableName ||
                L"fts_message_table_1_content" == tableInfo.srcTableName ||
                L"fts_message_table_2_content" == tableInfo.srcTableName ||
                L"fts_message_table_3_content" == tableInfo.srcTableName ||
                L"fts_message_table_4_content" == tableInfo.srcTableName ||
                L"fts_message_table_5_content" == tableInfo.srcTableName ||
                L"fts_message_table_6_content" == tableInfo.srcTableName ||
                L"fts_message_table_7_content" == tableInfo.srcTableName ||
                L"fts_message_table_8_content" == tableInfo.srcTableName ||
                L"fts_message_table_9_content" == tableInfo.srcTableName
                )
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"c1MesLocalID" == iterColumn->columnName)
                    {
                        if (1 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (L"usernameid" == iterColumn->columnName)
                    {
                        if (1 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                    else if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (10 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                            else
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
            else if (L"fts_username_id" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"usernameid" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 < iter->integerData &&
                                iter->integerData < m_maxFtsUsernameId)
                            {
                                if (m_ftsUsernameIds.end() != m_ftsUsernameIds.find(iter->integerData))
                                {
                                    shouldBreak = true;
                                    isFind = false;
                                }
                                else
                                {
                                    m_ftsUsernameIds.insert(iter->integerData);
                                }
                            }
                        }
                    }
                    else if (L"validfalg" == iterColumn->columnName)
                    {
                        if (1 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                        else
                        {
                            if (0 != iter->integerData &&
                                1 != iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                    }
                    else if (L"UsrName" == iterColumn->columnName)
                    {
                        if (4 != iter->dataType)
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                        else
                        {
                            if (m_ftsUsernamesExist.end() == m_ftsUsernamesExist.find(iter->stringData))
                            {
                                isFind = true;
                            }
                            else
                            {
                                isFind = true;
                            }
                        }
                    }
                    else if (L"tableid" == iterColumn->columnName)
                    {
                        shouldBreak = true;
                        if (1 != iter->dataType)
                        {
                            isFind = false;
                        }
                        else
                        {
                            if (0 > iter->integerData || 9 < iter->integerData)
                            {
                                isFind = false;
                            }
                            else
                            {
                                isFind = true;
                            }
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_WHATSAPP:
        {
            if (L"ZWAMESSAGE" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (L"Z_ENT" == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (1 == iter->dataType && 9 == iter->integerData)
                        {
                            isFind = true;
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    case IOS_RECOVERY_TELEGRAM:
        {
            if (L"messages_v29" == tableInfo.srcTableName)
            {
                std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
                for (std::list<RecordInfo>::const_iterator iter = recordInfos.begin();
                    iter != recordInfos.end();
                    ++iter, ++iterColumn)
                {
                    if (shouldBreak)
                    {
                        break;
                    }

                    if (tableInfo.uniqueColumnName == iterColumn->columnName)
                    {
                        shouldBreak = true;

                        if (1 == iter->dataType)
                        {
                            wchar_t buffer[64] = {0};
                            StringCbPrintfW(buffer, sizeof(buffer), L"%lld", iter->integerData);
                            if (10 == wcslen(buffer) && L'1' == buffer[0])
                            {
                                isFind = true;
                            }
                            else
                            {
                                isFind = false;
                            }
                        }
                        else
                        {
                            isFind = false;
                        }
                    }
                    else if (L"outgoing" == iterColumn->columnName)
                    {
                        if (1 == iter->dataType)
                        {
                            if (0 != iter->integerData && 1 != iter->integerData)
                            {
                                shouldBreak = true;
                                isFind = false;
                            }
                        }
                        else
                        {
                            shouldBreak = true;
                            isFind = false;
                        }
                    }
                }
            }
        }
        break;
    default:
        break;
    }

    return isFind;
}

bool SQLiteDataRecoveryInternal::CheckRecoveryDataStep2(std::list<RecordInfo> &recordInfos,
                                                        const TableInfo &tableInfo,
                                                        int typeBegin,
                                                        int typeEnd,
                                                        unsigned char *begin,
                                                        unsigned char *end,
                                                        unsigned char *&dataBegin,
                                                        unsigned long long &RowID)
{
	if (!begin || !end)
	{
		return false;
	}

	bool isFind = false;
	int typeIndex = typeBegin;

	do
	{
		ClearList(recordInfos);
		GetAllRecordInfo(recordInfos, typeBegin, typeEnd, begin, end);

		if (typeIndex == typeBegin)
		{
			break;
		}

		if ((typeBegin + 1) > tableInfo.mainColumn)
		{
            for (int i = 0; i < typeIndex; ++i)
            {
                RecordInfo recordInfo;
                recordInfo.dataSize = 0;
                recordInfo.dataType = 0;
                recordInfo.blobData = NULL;
                recordInfo.doubleData = 0.0;
                recordInfo.integerData = 0;
                recordInfo.stringData.clear();
                recordInfos.push_front(recordInfo);
            }

            int index = 0;
			for (std::list<RecordInfo>::iterator iter = recordInfos.begin();
				iter != recordInfos.end();
				++iter, ++index)
			{
                if (begin >= end)
                {
                    break;
                }

				if (index >= typeBegin)
				{
					break;
				}

                if (begin + iter->dataSize > end)
                {
                    iter->dataSize = end - begin;
                    GetOneRecordData(*iter, begin);
                    begin = end;
                    break;
                }

				GetOneRecordData(*iter, begin);
				begin += iter->dataSize;
			}

            // 关键列内容没有被覆盖即可作为恢复记录
            if ((index + 1) < tableInfo.mainColumn)
            {
                break;
            }

            if (!CheckRecoveryDataStep3(recordInfos, tableInfo, RowID))
            {
                break;
            }

            for (; typeBegin < typeEnd; ++typeBegin)
            {
                RecordInfo recordInfo;
                recordInfo.dataSize = 0;
                recordInfo.dataType = 0;
                recordInfo.blobData = NULL;
                recordInfo.doubleData = 0.0;
                recordInfo.integerData = 0;
                recordInfo.stringData.clear();
                recordInfos.push_back(recordInfo);
            }

            if (0 != RowID)
            {
                std::list<RecordInfo>::iterator iter1 = recordInfos.begin();
                std::list<ColumnInfo>::const_iterator iter2 = tableInfo.columnInfos.begin();
                for (; iter1 != recordInfos.end(); ++iter1, ++iter2)
                {
                    if (iter2->isAutoIncrement)
                    {
                        iter1->dataType = 1;
                        iter1->integerData = (long long)RowID;
                        break;
                    }
                }
            }

            if (CheckRecoveryDataStep4(recordInfos, tableInfo) ||
                CheckRecoveryDataStep5(recordInfos, tableInfo))
            {
                break;
            }
            
            dataBegin = begin;
            isFind = true;
        }

	} while (false);

    if (!isFind)
    {
        ClearList(recordInfos);
    }

	return isFind;
}

bool SQLiteDataRecoveryInternal::CheckRecoveryDataStep1(const TableInfo &tableInfo,
                                                       unsigned char *begin,
                                                       unsigned char *end,
                                                       unsigned char *&dataBegin,
                                                       unsigned char *pageBuffer,
                                                       bool isFreeBlock)
{
	if (!begin || !end || !pageBuffer)
	{
		return false;
	}

	// cell 结构如下
	//                    |-------------HeaderSize-----------|
	// | CellSize | RowID | HeaderSize | Type 1 |...| Type N | Data 1 |...| Data N |
	// |----Cell Header---|----------------------Cell Size-------------------------|

	// freeblock结构如下
	// | CellSize | RowID | HeaderSize | Type 1 |...| Type N | Data 1 |...| Data N |
	// |next freeblock offset|current freeblock size|
	// |-------2字节--------|--------2字节---------|

	// 经过分析发现，如果一个cell存在溢出页，此cell在删除后不会变为freeblock，
	// 否则会变为freeblock。因此freeblock实际上是原cell的前4字节被覆盖后的结果。
	// 一个B-tree 页最少有4个cell，一个B-tree页的页大小最多为2个字节，因此在不
	// 考虑溢出页的情况下一个cell的大小最多2个字节（1 ~ 16383）。

	// 2017.08.09
	// freeblock存在3种情况
	// 1、freeblock包含1个cell完整数据
	// 2、freeblock包含1个cell部分数据
	// 3、freeblock包含多于1个cell的数据
	// 以下为情况1的子情况

	// cell前4字节被覆盖有8种情况：
	// 1.1、覆盖CellSize部分
	// 1.2、覆盖至CellSize结束
	// 1.3、覆盖RowID部分
	// 1.4、覆盖至RowID结束
	// 1.5、覆盖HeaderSize部分
	// 1.6、覆盖至HeaderSize结束
	// 1.7、覆盖Type 1部分
	// 1.8、覆盖至Type 1结束

	// 下面分析8种情况的可能性：
	// 因为一个cell最多2个字节，因此情况1和2可以直接抛弃。
	// 如果存在情况7，那么此时CellSize一定是1个字节，Type 1一定大于1个字节，而
	// 根据上图得知，CellSize包含Type 1，此时两者互相矛盾，因此这种情况也可以抛弃。
	// 这样只剩下1.3、1.4、1.5、1.6、1.8。

	//分析的依据在于：
	// 1、Type数组的个数已知。
	// 2、HeadSize = HeadSize(本身所占字节数) + Type 1 + ... + Type N。
	// 3、CellSize、RowID、HeadSize、Type 1 - Type N都是可变长整数，可按照可变长整数原理
	// 计算其所在字节数。

	// 具体思路：
	// 从freeblock + 4处开始读可变长整数，直到读到我们假定的HeadSize时记录此HeadSize，然后
	// 接着按照Type数组的个数依此读取Type的值，相加后为HeadSize的值（HeadSize = HeadSize(本身所占字节数) + Type 1 + ... + Type N）
	// 将此HeadSize与读出的HeadSize值比较，如果相同则可以确定Type 1的位置，如果不成立则抛弃此种情况，
	// 继续下一种情况，直到确定Type 1的位置。

    unsigned long long cellSize = 0;
    unsigned long long RowID = 0;
	unsigned long long HeaderSize = 0;
	unsigned short HeaderSize_ = 0;
	unsigned char len = 0;
	unsigned char *backPoint = begin;
	unsigned char *Type1Offset = NULL;
	unsigned long long v = 0;
    std::list<RecordInfo> recordInfos;
    int typeBegin = 0;
	int typeEnd = (int)tableInfo.columnInfos.size();
	bool isFind = false;

	do
	{
        if (!isFreeBlock)
        {
            // 情况1.2
            do
            {
                begin = backPoint;
                HeaderSize = 0;
                HeaderSize_ = 0;
                RowID = 0;

                // RowID
                len = sqlite3GetVarint(begin, &RowID);
                begin += len;

                if (0 == RowID)
                {
                    break;
                }

                if (begin >= end)
                {
                    break;
                }

                // HeadSize
                len = sqlite3GetVarint(begin, &HeaderSize);
                begin += len;
                HeaderSize_ = len;
                Type1Offset = begin;

                if (HeaderSize <= typeEnd)
                {
                    break;
                }

                if (begin >= end)
                {
                    break;
                }

                // Type 1 - Type N
                for (int i = 0; i < typeEnd; ++i)
                {
                    len = sqlite3GetVarint(begin, &v);
                    begin += len;
                    HeaderSize_ += len;
                }

                if (begin >= end)
                {
                    break;
                }

                if (HeaderSize == HeaderSize_)
                {
                    begin = Type1Offset;
                    typeBegin = 0;

                    isFind = CheckRecoveryDataStep2(recordInfos, tableInfo, typeBegin, typeEnd, begin, end, dataBegin, RowID);
                }

            } while (false);

            if (isFind)
            {
                break;
            }

            // 情况1.6
            {
                begin = backPoint;
                HeaderSize = 0;
                HeaderSize_ = 1;
                RowID = 0;
                Type1Offset = begin;

                // Type 1 - Type N
                typeBegin = 0;
                isFind = CheckRecoveryDataStep2(recordInfos, tableInfo, typeBegin, typeEnd, begin, end, dataBegin, RowID);
            }

            if (isFind)
            {
                break;
            }

            // 情况1.8
            {
                begin = backPoint;
                HeaderSize = 0;
                HeaderSize_ = 1;
                RowID = 0;
                Type1Offset = begin;

                // Type 2 - Type N
                typeBegin = 1;
                isFind = CheckRecoveryDataStep2(recordInfos, tableInfo, typeBegin, typeEnd, begin, end, dataBegin, RowID);
            }
        }
        else
        {
            // 情况1.3
            do
            {
                begin = backPoint;
                HeaderSize = 0;
                HeaderSize_ = 0;
                RowID = 0;

                // RowID
                len = sqlite3GetVarint(begin, &RowID);
                begin += len;

                if (0 == RowID)
                {
                    break;
                }

                if (begin >= end)
                {
                    break;
                }

                // HeadSize
                len = sqlite3GetVarint(begin, &HeaderSize);
                begin += len;
                HeaderSize_ = len;
                Type1Offset = begin;

                if (HeaderSize <= typeEnd)
                {
                    break;
                }

                if (begin >= end)
                {
                    break;
                }

                // Type 1 - Type N
                for (int i = 0; i < typeEnd; ++i)
                {
                    len = sqlite3GetVarint(begin, &v);
                    begin += len;
                    HeaderSize_ += len;
                }

                if (begin >= end)
                {
                    break;
                }

                if (HeaderSize == HeaderSize_)
                {
                    begin = Type1Offset;
                    typeBegin = 0;

                    isFind = CheckRecoveryDataStep2(recordInfos, tableInfo, typeBegin, typeEnd, begin, end, dataBegin, RowID);
                }

            } while (false);

            if (isFind)
            {
                break;
            }

            // 情况1.4
            do
            {
                begin = backPoint;
                HeaderSize = 0;
                HeaderSize_ = 0;
                RowID = 0;

                // HeadSize
                len = sqlite3GetVarint(begin, &HeaderSize);
                begin += len;
                HeaderSize_ = len;
                Type1Offset = begin;

                if (HeaderSize <= typeEnd)
                {
                    break;
                }

                if (begin >= end)
                {
                    break;
                }

                // Type 1 - Type N
                for (int i = 0; i < typeEnd; ++i)
                {
                    len = sqlite3GetVarint(begin, &v);
                    begin += len;
                    HeaderSize_ += len;
                }

                if (begin >= end)
                {
                    break;
                }

                if (HeaderSize == HeaderSize_)
                {
                    begin = Type1Offset;
                    typeBegin = 0;
                    isFind = CheckRecoveryDataStep2(recordInfos, tableInfo, typeBegin, typeEnd, begin, end, dataBegin, RowID);
                }

            } while (false);

            if (isFind)
            {
                break;
            }

            // 情况1.5
            do
            {
                begin = backPoint;
                HeaderSize = 0;
                HeaderSize_ = 1;
                RowID = 0;

                // HeadSize
                len = sqlite3GetVarint(begin, &HeaderSize);
                begin += len;
                HeaderSize_ += len;
                Type1Offset = begin;

                if (begin >= end)
                {
                    break;
                }

                // Type 1 - Type N
                for (int i = 0; i < typeEnd; ++i)
                {
                    len = sqlite3GetVarint(begin, &v);
                    begin += len;
                    HeaderSize_ += len;
                }

                if (0x80 <= HeaderSize_)
                {
                    // Type 1 - Type N
                    begin = Type1Offset;
                    typeBegin = 0;
                    isFind = CheckRecoveryDataStep2(recordInfos, tableInfo, typeBegin, typeEnd, begin, end, dataBegin, RowID);
                }

            } while (false);

            if (isFind)
            {
                break;
            }

            // 情况1.6
            {
                begin = backPoint;
                HeaderSize = 0;
                HeaderSize_ = 1;
                RowID = 0;
                Type1Offset = begin;

                // Type 1 - Type N
                typeBegin = 0;
                isFind = CheckRecoveryDataStep2(recordInfos, tableInfo, typeBegin, typeEnd, begin, end, dataBegin, RowID);
            }

            if (isFind)
            {
                break;
            }

            // 情况1.8
            {
                begin = backPoint;
                HeaderSize = 0;
                HeaderSize_ = 1;
                RowID = 0;
                Type1Offset = begin;

                // Type 2 - Type N
                typeBegin = 1;
                isFind = CheckRecoveryDataStep2(recordInfos, tableInfo, typeBegin, typeEnd, begin, end, dataBegin, RowID);
            }
        }

	} while (false);

    if (isFind)
    {
        if (IOS_RECOVERY_SMS == tableInfo.recoveryAppType &&
            L"handle" == tableInfo.srcTableName)
        {
            m_currentPageRowIDMap.insert(std::make_pair(backPoint - pageBuffer, RowID));

            if (0 != RowID)
            {
                m_recoveryData.insert(std::make_pair(tableInfo.srcTableName, recordInfos));
            }
            else
            {
                m_currentPageRowIDIsZeroMap.insert(std::make_pair(backPoint - pageBuffer, recordInfos));
            }
        }
        else
        {
            m_recoveryData.insert(std::make_pair(tableInfo.srcTableName, recordInfos));
        }
    }

	return isFind;
}


bool SQLiteDataRecoveryInternal::UnusedSpaceDataRecoveryEx(unsigned char *pageBuffer,
                                                           unsigned char *begin,
                                                           unsigned char *end,
                                                           std::map<unsigned short, unsigned short> &freeBlockOffsetMap,
                                                           const TableInfo &tableInfo)
{
    if (!pageBuffer ||
        !begin ||
		!end)
    {
        return false;
    }

    unsigned char *backBegin = begin;
    unsigned char *dataBegin = NULL;
    bool isFind = false;
    bool isFindOne = false;

    int noneZeroCounts = 0;
    int minCounts = (int)tableInfo.columnInfos.size();

    unsigned short offset = 0;

    for (; begin < end; ++begin)
    {
        if (*begin)
        {
            ++noneZeroCounts;
        }
    }

    if (noneZeroCounts <= minCounts)
    {
        return false;
    }

    begin = backBegin;
    for (; begin < end; )
    {
        offset = begin - pageBuffer;
        dataBegin = NULL;

        if (freeBlockOffsetMap.end() == freeBlockOffsetMap.find(offset))
        {
            isFind = CheckRecoveryDataStep1(tableInfo, begin, end, dataBegin, pageBuffer);

            if (isFind && dataBegin)
            {
                isFindOne = true;
                begin = dataBegin;
            }
            else
            {
                begin = ++backBegin;
            }
        }
        else
        {
            if (begin + 4 < end)
            {
                isFind = CheckRecoveryDataStep1(tableInfo, begin + 4, end, dataBegin, pageBuffer, true);
            }

            if (isFind && dataBegin)
            {
                isFindOne = true;
                begin = dataBegin;
            }
            else
            {
                begin = backBegin + 4;
            }

            freeBlockOffsetMap.erase(offset);
        }
        
        backBegin = begin;
    }

    return isFindOne;
}

void SQLiteDataRecoveryInternal::UnusedSpaceDataRecovery(unsigned char *pageBuffer,
                                                         unsigned short cellCount,
                                                         const std::map<unsigned short, unsigned short> &cellOffsetMap,
                                                         std::map<unsigned short, unsigned short> &freeBlockOffsetMap,
                                                         const TableInfo &tableInfo)
{
    if (!pageBuffer)
    {
        return;
    }

    unsigned short dataSize = 0;
    unsigned short dataOffset = 0;
    unsigned short right = 0;

    if (cellOffsetMap.empty())
    {
        UnusedSpaceDataRecoveryEx(pageBuffer, pageBuffer, pageBuffer + m_pageSize, freeBlockOffsetMap, tableInfo);
    }
    else
    {
        std::map<unsigned short, unsigned short>::const_iterator iterBegin = cellOffsetMap.begin();
        std::map<unsigned short, unsigned short>::const_iterator iterNext = iterBegin;
        ++iterNext;

        dataOffset = 8 + 2 *cellCount;
        right = iterBegin->first;

        if (right > dataOffset)
        {
            UnusedSpaceDataRecoveryEx(pageBuffer, pageBuffer + dataOffset, pageBuffer + right, freeBlockOffsetMap, tableInfo);
		}

        for (; iterNext != cellOffsetMap.end(); ++iterBegin, ++iterNext)
        {
            dataOffset = iterBegin->first + iterBegin->second;
            right = iterNext->first;

            if (iterNext->first > dataOffset)
            {
                UnusedSpaceDataRecoveryEx(pageBuffer, pageBuffer + dataOffset, pageBuffer + right, freeBlockOffsetMap, tableInfo);
			}
        }

        dataOffset = cellOffsetMap.rbegin()->first + cellOffsetMap.rbegin()->second;
        right = m_pageSize;

        if (right > dataOffset)
        {
            UnusedSpaceDataRecoveryEx(pageBuffer, pageBuffer + dataOffset, pageBuffer + right, freeBlockOffsetMap, tableInfo);
		}
    }
}

void SQLiteDataRecoveryInternal::WalModeDataRecovery()
{
    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    std::string sql_;
    std::wstring attachDbName = L"B0C5F93C_6C1F_466d_A9D4_5EA77C378616";
    const wchar_t *errMsg = NULL;

    do 
    {
        if (!m_dbHandler)
        {
            break;
        }

        // 附加备份数据库
        sql = L"attach database \"" + m_dbPathBack + L"\" as " + attachDbName;
        int ret = sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL);
        if (SQLITE_OK != ret)
        {
            break;
        }

        if (SQLITE_DONE != sqlite3_step(stmt))
        {
            sqlite3_finalize(stmt);
            break;
        }
        
        sqlite3_finalize(stmt);

        for (std::list<TableInfo>::iterator iter = m_tableInfos.begin();
            iter != m_tableInfos.end();
            ++iter)
        {
            if (CreateNewTable(*iter))
            {
                sql = L"insert into " + iter->dstTableName + L" select * from " + attachDbName + L"." + iter->srcTableName + 
                    L" where not exists(select " + iter->srcTableName + L".rowid" + L" from " + iter->srcTableName + L" where " + attachDbName +
                    L"." + iter->srcTableName + L".rowid" + L" = " + iter->srcTableName + L".rowid)";
                if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
                {
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                }
                else
                {
                    errMsg = (const wchar_t *)sqlite3_errmsg16(m_dbHandler);
                }
            }
        }

        // 分离数据库
        sql = L"detach database " + attachDbName;
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    } while (false);
}

void SQLiteDataRecoveryInternal::AndroidWeixinFTS5MetaMessageRecovery(const TableInfo &tableInfo)
{
    if (!m_dbHandler)
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;

    if (!CreateNewTable(tableInfo))
    {
        return;
    }

    sql = L"insert into " + tableInfo.dstTableName + L" select * from " + tableInfo.srcTableName + L" where status = -1";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void SQLiteDataRecoveryInternal::AndroidWeixinFTS5IndexMessageRecovery(const TableInfo &tableInfo)
{
    if (!m_dbHandler)
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    std::list<std::wstring> docIdList;
    const wchar_t *data = NULL;
    const wchar_t *errorMsg = NULL;

    sql = L"select docid from FTS5MetaMessage where status = -1";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        while (SQLITE_ROW == sqlite3_step(stmt))
        {
            data = (const wchar_t *)sqlite3_column_text16(stmt, 0);
            docIdList.push_back(data ? std::wstring(data) : L"");
        }

        sqlite3_finalize(stmt);
    }

    if (docIdList.empty())
    {
        return;
    }

    if (!CreateNewTable(tableInfo))
    {
        return;
    }

    for (std::list<std::wstring>::const_iterator iter = docIdList.begin();
        iter != docIdList.end();
        ++iter)
    {
        sql = L"insert into " + tableInfo.dstTableName + L" select * from " + tableInfo.srcTableName + L" where id = " + *iter;
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            if (SQLITE_DONE != sqlite3_step(stmt))
            {
                errorMsg = (const wchar_t *)sqlite3_errmsg16(m_dbHandler);
                sqlite3_finalize(stmt);
                break;
            }

            sqlite3_finalize(stmt);
        }
    }
}

void SQLiteDataRecoveryInternal::AndroidWeixinRecoveryFromFTSDb(const TableInfo &tableInfo)
{
    std::wstring weixinFTSDbPath;
    sqlite3_stmt *stmt = NULL;
    size_t pos = std::wstring::npos;
    std::wstring sql;
    std::wstring attachDbName = L"D7F91961_F80A_4c14_A27E_B7738F6BE8D8";
    const wchar_t *data1 = NULL;
    const wchar_t *data2 = NULL;
    const wchar_t *errorMsg = NULL;

    // <key, value> = <rowid, msgid>
    std::map<std::wstring, std::wstring> msgIdMap;
    std::list<RecordInfo> recordInfos;
    int msgIdIndex = -1;
    int isSendIndex = -1;
    int createTimeIndex = -1;
    int talkerIndex = -1;
    int contentIndex = -1;
    int index = 0;
    bool isAttachDatabase = false;

    do 
    {
        pos = m_dbPath.rfind(L'\\');
        if (std::wstring::npos == pos)
        {
            break;
        }

        weixinFTSDbPath = m_dbPath.substr(0, pos) + L"\\FTS5IndexMicroMsg.db_back";
        if (!PathFileExistsW(weixinFTSDbPath.c_str()))
        {
            break;
        }

        // 附加数据库
        sql = L"attach database \"" + weixinFTSDbPath + L"\" as " + attachDbName;
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            if (SQLITE_DONE != sqlite3_step(stmt))
            {
                sqlite3_finalize(stmt);
                break;
            }

            sqlite3_finalize(stmt);
            isAttachDatabase = true;
        }

        sql = L"select FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39.rowid, FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39.docid from " +
            attachDbName + L".FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39 inner join " +
            attachDbName + L".FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39 where \
             FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39.rowid = FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39.rowid";
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            while (SQLITE_ROW == sqlite3_step(stmt))
            {
                data1 = (const wchar_t*)(sqlite3_column_text16(stmt, 0));
                data2 = (const wchar_t*)(sqlite3_column_text16(stmt, 1));
                msgIdMap.insert(std::make_pair(data1 ? std::wstring(data1) : L"", data2 ? std::wstring(data2) : L""));
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            errorMsg = (const wchar_t *)sqlite3_errmsg16(m_dbHandler);
        }

        for (std::map<std::wstring, std::wstring>::const_iterator iter = msgIdMap.begin();
            iter != msgIdMap.end();
            ++iter)
        {
            sql = L"update " + attachDbName + L".FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39 set id = " + iter->second +
            L" where " + attachDbName + L".FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39.rowid = " + iter->first;
            if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
            {
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }

        for (std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
            iterColumn != tableInfo.columnInfos.end();
            ++iterColumn, ++index)
        {
            RecordInfo recordInfo;
            recordInfo.blobData = NULL;
            recordInfo.dataSize = 0;
            recordInfo.dataType = iterColumn->dataType;
            recordInfo.doubleData = 0.0;
            recordInfo.integerData = 0;
            recordInfo.stringData.clear();

            if (L"msgId" == iterColumn->columnName)
            {
                msgIdIndex = index;
            }
            else if (L"isSend" == iterColumn->columnName)
            {
                isSendIndex = index;
            }
            else if (L"createTime" == iterColumn->columnName)
            {
                createTimeIndex = index;
            }
            else if (L"talker" == iterColumn->columnName)
            {
                talkerIndex = index;
            }
            else if (L"content" == iterColumn->columnName)
            {
                contentIndex = index;
            }

            recordInfos.push_back(recordInfo);
        }

        sql = L"select FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39.id, "
               L"FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39.c0, "
               L"(FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39.talker == '" +
               m_androidWeixinNumber + L"') as isSend, "
               L"FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39.timestamp, "
               L"FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39.aux_index from " +
               attachDbName + L".FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39 inner join " +
               attachDbName + L".FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39 where "
               L"FTS5MetaMessage_694F69B9_93B9_480a_B23C_F6D221EEAF39.docid = FTS5IndexMessage_content_694F69B9_93B9_480a_B23C_F6D221EEAF39.id";
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            while (SQLITE_ROW == sqlite3_step(stmt))
            {
                index = 0;
                for (std::list<RecordInfo>::iterator iterRecord = recordInfos.begin();
                    iterRecord != recordInfos.end();
                    ++iterRecord, ++index)
                {
                    if (index == msgIdIndex)
                    {
                        iterRecord->integerData = sqlite3_column_int64(stmt, 0);
                    }
                    else if (index == contentIndex)
                    {
                        const char *data = (const char *)sqlite3_column_text(stmt, 1);
                        iterRecord->stringData = data ? std::string(data) : "";
                    }
                    else if (index == isSendIndex)
                    {
                        iterRecord->integerData = sqlite3_column_int64(stmt, 2);
                    }
                    else if (index == createTimeIndex)
                    {
                        iterRecord->integerData = sqlite3_column_int64(stmt, 3);
                    }
                    else if (index == talkerIndex)
                    {
                        const char *data = (const char *)sqlite3_column_text(stmt, 4);
                        iterRecord->stringData = data ? std::string(data) : "";
                    }
                }

                m_recoveryData.insert(std::make_pair(tableInfo.srcTableName, recordInfos));
            }

            sqlite3_finalize(stmt);
        }
                
    } while (false);

    if (isAttachDatabase)
    {
        // 分离数据库
        sql = L"detach database " + attachDbName;
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
}

void SQLiteDataRecoveryInternal::IosWeixinRecoveryFromFTSDb(const TableInfo &tableInfo)
{
    std::wstring weixinFTSDbPath;
    sqlite3_stmt *stmt = NULL;
    size_t pos = std::wstring::npos;
    std::wstring sql;
    std::wstring attachDbName = L"D7F91961_F80A_4c14_A27E_B7738F6BE8D8";
    const wchar_t *data = NULL;
    bool isAttachDatabase = false;
    std::wstring userNameID;
    std::list<std::wstring> ftsMessageTables;
    std::list<RecordInfo> recordInfos;
    int msgIdIndex = -1;
    int createTimeIndex = -1;
    int messageIndex = -1;
    int index = 0;
    const wchar_t *errorMsg = NULL;

    do 
    {
        pos = m_dbPath.rfind(L'\\');
        if (std::wstring::npos == pos)
        {
            break;
        }

        weixinFTSDbPath = m_dbPath.substr(0, pos);
        if (weixinFTSDbPath.size() < 4)
        {
            break;
        }

        if (L'B' != weixinFTSDbPath[weixinFTSDbPath.size() - 1] ||
            L'D' != weixinFTSDbPath[weixinFTSDbPath.size() - 2] ||
            L'\\' != weixinFTSDbPath[weixinFTSDbPath.size() - 3])
        {
            break;
        }

        weixinFTSDbPath = weixinFTSDbPath.substr(0, weixinFTSDbPath.size() - 3) + L"\\fts\\fts_message.db_back";
        if (!PathFileExistsW(weixinFTSDbPath.c_str()))
        {
            break;
        }

        // 附加数据库
        sql = L"attach database \"" + weixinFTSDbPath + L"\" as " + attachDbName;
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            if (SQLITE_DONE != sqlite3_step(stmt))
            {
                sqlite3_finalize(stmt);
                break;
            }

            sqlite3_finalize(stmt);
            isAttachDatabase = true;
        }

        sql = L"select usernameid from " + attachDbName + L".fts_username_id where UsrName = '" + StringConvertor::Utf8ToUnicode(tableInfo.chatAccount) + L"'";
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            if (SQLITE_ROW == sqlite3_step(stmt))
            {
                data = (const wchar_t*)(sqlite3_column_text16(stmt, 0));
                userNameID = data ? std::wstring(data) : L"";
            }
            sqlite3_finalize(stmt);
        }

        if (userNameID.empty())
        {
            sql = L"select usernameid from " + attachDbName + L".fts_username_id_694F69B9_93B9_480a_B23C_F6D221EEAF39 where UsrName = '" + StringConvertor::Utf8ToUnicode(tableInfo.chatAccount) + L"'";
            if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
            {
                if (SQLITE_ROW == sqlite3_step(stmt))
                {
                    data = (const wchar_t*)(sqlite3_column_text16(stmt, 0));
                    userNameID = data ? std::wstring(data) : L"";
                }
                sqlite3_finalize(stmt);
            }
        }

        if (userNameID.empty())
        {
            break;
        }

        sql = L"select tbl_name from " + attachDbName + L".sqlite_master where tbl_name like 'fts_message_table_%_content_694F69B9_93B9_480a_B23C_F6D221EEAF39'";
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            while (SQLITE_ROW == sqlite3_step(stmt))
            {
                data = (const wchar_t*)(sqlite3_column_text16(stmt, 0));
                ftsMessageTables.push_back(data ? std::wstring(data) : L"");
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            errorMsg = (const wchar_t *)sqlite3_errmsg16(m_dbHandler);
        }

        for (std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
            iterColumn != tableInfo.columnInfos.end();
            ++iterColumn, ++index)
        {
            RecordInfo recordInfo;
            recordInfo.blobData = NULL;
            recordInfo.dataSize = 0;
            recordInfo.dataType = iterColumn->dataType;
            recordInfo.doubleData = 0.0;
            recordInfo.integerData = 0;
            recordInfo.stringData.clear();

            if (L"MesLocalID" == iterColumn->columnName)
            {
                msgIdIndex = index;
            }
            else if (L"CreateTime" == iterColumn->columnName)
            {
                createTimeIndex = index;
            }
            else if (L"Message" == iterColumn->columnName)
            {
                messageIndex = index;
            }

            recordInfos.push_back(recordInfo);
        }

        for (std::list<std::wstring>::const_iterator iter = ftsMessageTables.begin();
            iter != ftsMessageTables.end();
            ++iter)
        {
            sql = L"select c1MesLocalID, c2CreateTime, c3Message from " + attachDbName + L"." + *iter +
                L" where c0usernameid = " + userNameID;
            if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
            {
                while (SQLITE_ROW == sqlite3_step(stmt))
                {
                    index = 0;
                    for (std::list<RecordInfo>::iterator iterRecord = recordInfos.begin();
                        iterRecord != recordInfos.end();
                        ++iterRecord, ++index)
                    {
                        if (index == msgIdIndex)
                        {
                            iterRecord->integerData = sqlite3_column_int64(stmt, 0);
                        }
                        else if (index == createTimeIndex)
                        {
                            iterRecord->integerData = sqlite3_column_int64(stmt, 1);
                        }
                        else if (index == messageIndex)
                        {
                            const char *data = (const char *)sqlite3_column_text(stmt, 2);
                            iterRecord->stringData = data ? std::string(data) : "";
                        }
                    }

                    m_recoveryData.insert(std::make_pair(tableInfo.srcTableName, recordInfos));
                }
                sqlite3_finalize(stmt);
            }
        }

    } while (false);

    if (isAttachDatabase)
    {
        // 分离数据库
        sql = L"detach database " + attachDbName;
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
}

bool SQLiteDataRecoveryInternal::WriteToNewTable()
{
    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    size_t columnCount;
    size_t columnIndex;
    const wchar_t *error = NULL;
    int code = -1;

    do 
    {
        if (!m_dbHandler)
        {
            break;
        }

        for (std::list<TableInfo>::const_iterator iter = m_tableInfos.begin();
            iter != m_tableInfos.end();
            ++iter)
        {
            std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
                std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(iter->srcTableName);

            // 空
            if (findPair.first == findPair.second)
            {
                continue;
            }

            if (!CreateNewTable(*iter))
            {
                continue;
            }

            sql = L"insert into " + iter->dstTableName + L" values(";
            columnCount = iter->columnInfos.size();
            for (size_t i = 0; i != columnCount - 1; ++i)
            {
                sql += L"?,";
            }
            sql += L"?)";

            if (SQLITE_OK != sqlite3_exec(m_dbHandler, "BEGIN", NULL, NULL, NULL))
            {
                continue;
            }

            if (SQLITE_OK != sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
            {
                continue;
            }

            while (findPair.first != findPair.second)
            {
                if (!CheckRecoveryDataStep6(findPair.first->second, *iter))
                {
                    columnIndex = 1;
                    for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
                        iter2 != findPair.first->second.end();
                        ++iter2, ++columnIndex)
                    {
                        switch (iter2->dataType)
                        {
                        case 0:
                            {
                                sqlite3_bind_null(stmt, columnIndex);
                            }
                            break;
                        case 1:
                            {
                                sqlite3_bind_int64(stmt, columnIndex, iter2->integerData);
                            }
                            break;
                        case 2:
                            {
                                sqlite3_bind_double(stmt, columnIndex, iter2->doubleData);
                            }
                            break;
                        case 3:
                            {
                                if (iter2->blobData)
                                {
                                    sqlite3_bind_blob(stmt, columnIndex, iter2->blobData, iter2->dataSize, BlobDestructor);
                                }
                                else
                                {
                                    sqlite3_bind_null(stmt, columnIndex);
                                }
                            }
                            break;
                        case 4:
                            {
                                sqlite3_bind_text(stmt, columnIndex, iter2->stringData.c_str(), -1, NULL);
                            }
                            break;
                        default:
                            break;
                        }
                    }

                    if (SQLITE_ERROR == sqlite3_step(stmt))
                    {
                        error = (wchar_t *)sqlite3_errmsg16(m_dbHandler);
                    }
                    sqlite3_reset(stmt);
                }
                
                ++findPair.first;
            }

            sqlite3_finalize(stmt);

            code = sqlite3_exec(m_dbHandler, "COMMIT", NULL, NULL, NULL);
            if (SQLITE_OK != code)
            {
                 code = sqlite3_exec(m_dbHandler, "ROLLBACK", NULL, NULL, NULL);
            }
        }

    } while (false);

    if (SQLITE_OK == code)
    {
        m_recoveryData.clear();
    }

    return SQLITE_OK == code;
}

void SQLiteDataRecoveryInternal::BlobDestructor(void *p)
{
    if (p)
    {
        delete [] p;
        p = NULL;
    }
}

void SQLiteDataRecoveryInternal::GetAndroidContactAccountIDs()
{
    m_androidContactAccountIDs.clear();

    if (!m_dbHandler)
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;

    sql = L"select _id from accounts";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        while (SQLITE_ROW == sqlite3_step(stmt))
        {
            m_androidContactAccountIDs.insert(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
}

void SQLiteDataRecoveryInternal::GetAndroidDeleteContactIDs(const TableInfo &tableInfo)
{
    m_androidDeleteContactIDMap.clear();

    long long contactID = 0;
    std::string displayName;
    
    std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
        std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

    while (findPair.first != findPair.second)
    {
        std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
        for (std::list<RecordInfo>::const_iterator iter2 = findPair.first->second.begin();
            iter2 != findPair.first->second.end();
            ++iter2, ++iterColumn)
        {
            if (L"raw_contact_id" == iterColumn->columnName)
            {
                contactID = iter2->integerData;
            }
            else if (L"data1" == iterColumn->columnName)
            {
                displayName = iter2->stringData;
                m_androidDeleteContactIDMap.insert(std::make_pair(contactID, displayName));
                break;
            }
        }

        ++findPair.first;
    }
}

void SQLiteDataRecoveryInternal::GetAndroidMimeTypeIDs()
{
    m_androidMimeTypeIDs.clear();

    if (!m_dbHandler)
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;

    sql = L"select _id from mimetypes";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        while (SQLITE_ROW == sqlite3_step(stmt))
        {
            m_androidMimeTypeIDs.insert(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
}

void SQLiteDataRecoveryInternal::GetAndroidWeixinMsgIDMapExist()
{
    if (!m_dbHandler)
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    const wchar_t *data = NULL;
    long long docid = 0;
    long long timestamp = 0;

    sql = L"select docid, timestamp from FTS5MetaMessage;";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        while (SQLITE_ROW == sqlite3_step(stmt))
        {
            docid = sqlite3_column_int64(stmt, 0);
            timestamp = sqlite3_column_int64(stmt, 1);
            m_androidWeixinMsgIDMap.insert(std::make_pair(timestamp, docid));
        }
        sqlite3_finalize(stmt);
    }
}

void SQLiteDataRecoveryInternal::GetAndroidWeixinMsgIDRecovery(const TableInfo &tableInfo)
{
    if (!m_dbHandler)
    {
        return;
    }

    int docidIndex = -1;
    int timestampIndex = -1;
    int index = 0;
    int indexBase = 0;
    long long docid = 0;
    long long docidBase = 0;
    long long timestamp = 0;
    std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::iterator,
        std::multimap<std::wstring, std::list<RecordInfo>>::iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

    // 空
    if (findPair.first == findPair.second)
    {
        return;
    }

    for (std::map<long long, long long>::const_iterator iterMsgID = m_androidWeixinMsgIDMap.begin();
        iterMsgID != m_androidWeixinMsgIDMap.end();
        ++iterMsgID, ++indexBase)
    {
        if (0 != iterMsgID->second)
        {
            docidBase = iterMsgID->second;
            break;
        }
    }

    if (0 >= docidBase)
    {
        for (std::map<long long, long long>::iterator iterMsgID = m_androidWeixinMsgIDMap.begin();
            iterMsgID != m_androidWeixinMsgIDMap.end();
            ++iterMsgID)
        {
            iterMsgID->second = docidBase;
            ++docidBase;
        }
    }
    else
    {
        for (std::map<long long, long long>::iterator iterMsgID = m_androidWeixinMsgIDMap.begin();
            iterMsgID != m_androidWeixinMsgIDMap.end();
            ++iterMsgID, ++index)
        {
            if (index < indexBase)
            {
                iterMsgID->second = docidBase - indexBase + index;
            }
            else if (index == indexBase)
            {
                
            }
            else if (index > indexBase)
            {
                if (0 == iterMsgID->second)
                {
                    iterMsgID->second = ++docidBase;
                }
                else
                {
                    docidBase = iterMsgID->second;
                }
            }
        }
    }

    index = 0;
    for (std::list<ColumnInfo>::const_iterator iterColumn = tableInfo.columnInfos.begin();
        iterColumn != tableInfo.columnInfos.end();
        ++iterColumn, ++index)
    {
        if (-1 != docidIndex && -1 != timestampIndex)
        {
            break;
        }

        if (L"docid" == iterColumn->columnName)
        {
            docidIndex = index;
        }
        else if (L"timestamp" == iterColumn->columnName)
        {
            timestampIndex = index;
        }
    }

    while (findPair.first != findPair.second)
    {
        index = 0;
        docid = 0;
        for (std::list<RecordInfo>::iterator iterRecord = findPair.first->second.begin();
            iterRecord != findPair.first->second.end();
            ++iterRecord, ++index)
        {
            if (docidIndex == index)
            {
                docid = iterRecord->integerData;
            }
            else if (timestampIndex == index)
            {
                if (0 == docid)
                {
                    if (m_androidWeixinMsgIDMap.end() != m_androidWeixinMsgIDMap.find(iterRecord->integerData))
                    {
                        index = 0;
                        for (std::list<RecordInfo>::iterator iterRecord2 = findPair.first->second.begin();
                            iterRecord2 != findPair.first->second.end();
                            ++iterRecord2, ++index)
                        {
                            if (docidIndex == index)
                            {
                                iterRecord2->dataType = 1;
                                iterRecord2->integerData = m_androidWeixinMsgIDMap[iterRecord->integerData];
                                break;
                            }
                        }
                    }
                }
            }
        }

        ++findPair.first;
    }
}

void SQLiteDataRecoveryInternal::GetAndroidWeixinNumber()
{
    m_androidWeixinNumber.clear();

    if (!m_dbHandler)
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    const wchar_t *data = NULL;

    sql = L"select value from userinfo where id = 2";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        if (SQLITE_ROW == sqlite3_step(stmt))
        {
            data = (const wchar_t*)(sqlite3_column_text16(stmt, 0));
            m_androidWeixinNumber = data ? std::wstring(data) : L"";
        }
        sqlite3_finalize(stmt);
    }
}

void SQLiteDataRecoveryInternal::GetIosContactIDMap(const TableInfo &tableInfo)
{
    m_iosContactIDMap.clear();

    if (!m_dbHandler)
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    const char *data = NULL;
    long long recordID = 0;
    int index = 1;
    bool isFind = false;

    std::pair<std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator,
        std::multimap<std::wstring, std::list<RecordInfo>>::const_iterator> findPair = m_recoveryData.equal_range(tableInfo.srcTableName);

    while (findPair.first != findPair.second)
    {
        index = 1;
        recordID = 0;
        isFind = false;

        for (std::list<RecordInfo>::const_iterator iter = findPair.first->second.begin();
            iter != findPair.first->second.end();
            ++iter, ++index)
        {
            if (2 == index)
            {
                recordID = iter->integerData;
            }
            else if (3 == index)
            {
                if (1 == iter->dataType && 3 == iter->integerData)
                {
                    isFind = true;
                }
            }
            else if (6 == index)
            {
                if (isFind)
                {
                    m_iosContactIDMap.insert(std::make_pair(recordID, iter->stringData));
                }
                break;
            }
        }

        ++findPair.first;
    }

    sql = L"select record_id, value from " + tableInfo.srcTableName + L" where property = 3";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        while (SQLITE_ROW == sqlite3_step(stmt))
        {
            recordID = sqlite3_column_int64(stmt, 0);
            data = (const char*)sqlite3_column_text(stmt, 1);
            if (data)
            {
                m_iosContactIDMap.insert(std::make_pair(recordID, data));
            }
        }
        sqlite3_finalize(stmt);
    }
}

void SQLiteDataRecoveryInternal::GetIosSmsRowID(const TableInfo &tableInfo)
{
    if (1 < m_currentPageRowIDMap.size())
    {
        unsigned long long RowID = 1;
        for (std::map<unsigned short, unsigned long long>::reverse_iterator iter = m_currentPageRowIDMap.rbegin();
            iter != m_currentPageRowIDMap.rend();
            ++iter, ++RowID)
        {
            if (0 == iter->second)
            {
                if (!m_currentPageRowIDIsZeroMap[iter->first].empty())
                {
                    std::list<RecordInfo>::iterator iter1 = m_currentPageRowIDIsZeroMap[iter->first].begin();
                    std::list<ColumnInfo>::const_iterator iter2 = tableInfo.columnInfos.begin();
                    for (; iter1 != m_currentPageRowIDIsZeroMap[iter->first].end(); ++iter1, ++iter2)
                    {
                        if (iter2->isAutoIncrement)
                        {
                            iter1->dataType = 1;
                            iter1->integerData = (long long)RowID;
                        }
                    }

                    m_recoveryData.insert(std::make_pair(tableInfo.srcTableName, m_currentPageRowIDIsZeroMap[iter->first]));
                }
            }
        }
    }
}

void SQLiteDataRecoveryInternal::GetIosFTSUsernameIds(const TableInfo &tableInfo)
{
    m_ftsUsernameIds.clear();
    m_maxFtsUsernameId = 0;

    if (!m_dbHandler)
    {
        return;
    }

    sqlite3_stmt *stmt = NULL;
    std::wstring sql;

    sql = L"select seq from sqlite_sequence where name = '" + tableInfo.srcTableName + L"'";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        if (SQLITE_ROW == sqlite3_step(stmt))
        {
            m_maxFtsUsernameId = sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    sql = L"select usernameid from fts_username_id";
    if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
    {
        while (SQLITE_ROW == sqlite3_step(stmt))
        {
            m_ftsUsernameIds.insert(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
}

void SQLiteDataRecoveryInternal::GetIosFTSUsernamesExist(const TableInfo &tableInfo)
{
    m_ftsUsernamesExist.clear();

    if (!m_dbHandler)
    {
        return;
    }


    std::wstring weixinContactDbPath;
    sqlite3_stmt *stmt = NULL;
    size_t pos = std::wstring::npos;
    std::wstring sql;
    std::wstring attachDbName = L"D7F91961_F80A_4c14_A27E_B7738F6BE8D8";
    const char *data = NULL;
    bool isAttachDatabase = false;
    const wchar_t *errorMsg = NULL;

    do 
    {
        pos = m_dbPath.rfind(L'\\');
        if (std::wstring::npos == pos)
        {
            break;
        }

        weixinContactDbPath = m_dbPath.substr(0, pos);
        if (weixinContactDbPath.size() < 5)
        {
            break;
        }

        if (L's' != weixinContactDbPath[weixinContactDbPath.size() - 1] ||
            L't' != weixinContactDbPath[weixinContactDbPath.size() - 2] ||
            L'f' != weixinContactDbPath[weixinContactDbPath.size() - 3] ||
            L'\\' != weixinContactDbPath[weixinContactDbPath.size() - 4])
        {
            break;
        }

        weixinContactDbPath = weixinContactDbPath.substr(0, weixinContactDbPath.size() - 4) + L"\\DB\\WCDB_Contact.sqlite_back";
        if (!PathFileExistsW(weixinContactDbPath.c_str()))
        {
            break;
        }

        // 附加数据库
        sql = L"attach database \"" + weixinContactDbPath + L"\" as " + attachDbName;
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            if (SQLITE_DONE != sqlite3_step(stmt))
            {
                sqlite3_finalize(stmt);
                break;
            }

            sqlite3_finalize(stmt);
            isAttachDatabase = true;
        }

        sql = L"select userName from " + attachDbName + L".Friend";
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            while (SQLITE_ROW == sqlite3_step(stmt))
            {
                data = (const char *)sqlite3_column_text(stmt, 0);
                m_ftsUsernamesExist.insert(data ? std::string(data) : "");
            }

            sqlite3_finalize(stmt);
        }

    } while (false);

    if (isAttachDatabase)
    {
        // 分离数据库
        sql = L"detach database " + attachDbName;
        if (SQLITE_OK == sqlite3_prepare16(m_dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
}
