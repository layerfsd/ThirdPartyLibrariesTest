// ConsoleApplication1.cpp: 定义控制台应用程序的入口点。
//

#include <tchar.h>
#include <Windows.h>
#include <string>
#include <list>
#include "..\Utils\StringConvertor.h"


#include <WeixinParser.h>
#pragma comment(lib, "WeixinParser")

#include <zutil.h>
#pragma comment(lib, "zdll")

#include "..\Utils\sqlite3.h"
#pragma comment(lib, "sqlite3")


#include "mm_backup.h"

#include "DCL_List.h"
#include "SL_List.h"


void ZlibTest()
{
    wchar_t srcFile[] = L"D:\\test\\EnMicroMsg.db.bak-1";
    wchar_t dstFile[] = L"D:\\test\\EnMicroMsg.db.bak-1_extra";
    FILE *srcFileHandle = NULL;
    FILE *dstFileHandle = NULL;
    Byte *srcBuffer = NULL;
    Byte *dstBuffer = NULL;
    int ret = 0;
    long srcFileSize = 0;
    long dstFileSize = 0;

    do
    {
        ret = _wfopen_s(&srcFileHandle, srcFile, L"r");
        if (0 != ret)
        {
            break;
        }

        if (0 != fseek(srcFileHandle, 0, SEEK_END))
        {
            break;
        }

        srcFileSize = ftell(srcFileHandle);
        if (0 == srcFileSize)
        {
            break;
        }

        if (0 != fseek(srcFileHandle, 0, SEEK_SET))
        {
            break;
        }

        srcBuffer = (Byte *)calloc(srcFileSize, 1);
        if (!srcBuffer)
        {
            break;
        }

        size_t size = fread(srcBuffer, 1, srcFileSize, srcFileHandle);

        fclose(srcFileHandle);
        srcFileHandle = NULL;

        dstFileSize = 2 * srcFileSize;
        dstBuffer = (Byte *)calloc(dstFileSize, 1);
        if (!dstBuffer)
        {
            break;
        }

        ret = uncompress((Bytef*)dstBuffer, (uLongf*)&dstFileSize, (Bytef*)srcBuffer, (uLongf)srcFileSize);
        if (Z_OK != ret)
        {
            break;
        }

        ret = _wfopen_s(&dstFileHandle, dstFile, L"w");
        if (0 != ret)
        {
            break;
        }

        fwrite(dstBuffer, 1, dstFileSize, dstFileHandle);

    } while (0);

    if (srcBuffer)
    {
        free(srcBuffer);
    }

    if (dstBuffer)
    {
        free(dstBuffer);
    }

    if (srcFileHandle)
    {
        fclose(srcFileHandle);
    }

    if (dstFileHandle)
    {
        fclose(dstFileHandle);
    }
}

void WeixinParseTest()
{
    std::wstring dataPath = L"D:\\test";

    if (WEIXIN_InitEntry(dataPath.c_str(), 0))
    {
        char buffer[1024] = { 0 };

        std::list<std::string> accountList;
        std::string accountStr;

        WEIXIN_GetAccountList(buffer, 1024);

        accountStr = buffer;
        size_t pos1 = 0;
        size_t pos2 = accountStr.find('|');
        while (std::string::npos != pos2)
        {
            accountList.push_back(accountStr.substr(pos1, pos2 - pos1));
            pos1 = pos2 + 1;
            pos2 = accountStr.find('|', pos1);
        }

        accountList.push_back(accountStr.substr(pos1));

        for (std::list<std::string>::const_iterator iter = accountList.begin();
            iter != accountList.end();
            ++iter)
        {
            WEIXIN_GetAccoutInfo(*iter);
        }

        WEIXIN_Free();
    }
}

void WeixinBakFileParse(const std::wstring &bakFile, const std::wstring &dbPath)
{
    const unsigned char key[] = { 0x32, 0xdd, 0xa7, 0x11, 0xe7, 0xfc, 0x28, 0xb5, 0x11, 0xc3, 0xd3, 0xe3, 0x1a, 0x5c, 0x95, 0xfe };

    mm_recover_ctx *ctx = mm_recover_init(bakFile.c_str(), key, 16, NULL);
    if (ctx)
    {
        sqlite3 *db = NULL;
        std::string sql;
        if (SQLITE_OK == sqlite3_open16(dbPath.c_str(), &db))
        {
            sql = "create table message_back as select * from message;delete from message;";
            if (SQLITE_OK == sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL))
            {
                mm_recover_run(ctx, db, 1);
            }

            sqlite3_close(db);
        }
        mm_recover_finish(ctx);
    }
}

int _tmain(int argc, char**argv)
{
    /*DCL_List* dcl_list = NULL;
    for (int i = 0; i < 5; ++i)
    {
        DCL_List* node = new DCL_List();
        if (node)
        {
            node->data = i + 1;
            DCL_ListInsertTail(&dcl_list, node);
        }
    }

    DCL_ListTraver(dcl_list);
    DCL_ListReverse(&dcl_list);
    DCL_ListTraver(dcl_list);
    DCL_ListDestory(dcl_list);*/

    SL_List* sl_list = NULL;
    for (int i = 0; i < 5; ++i)
    {
        SL_List* node = new SL_List();
        if (node)
        {
            node->data = i + 1;
            SL_ListInsertTail(&sl_list, node);
        }
    }

    SL_ListTraver(sl_list);
    SL_ListReverse(&sl_list);
    SL_ListTraver(sl_list);
    SL_ListDestory(sl_list);


    //WeixinParseTest();

    /*std::wstring bakFile = L"d:\\test\\微信bak文件\\EnMicroMsg.db.bak";
    std::wstring dbPath = L"D:\\test\\微信bak文件\\EnMicroMsg_src.db";
    WeixinBakFileParse(bakFile, dbPath);*/

    //ZlibTest();

    /*sqlite3 *dbHandler = NULL;
    sqlite3_stmt *stmt = NULL;
    std::wstring sql;
    std::wstring dbPath = L"D:\\Project\\C&C++\\Solution1\\ConsoleApplication1\\1.db";

    do
    {
        if (SQLITE_OK != sqlite3_open16(dbPath.c_str(), &dbHandler))
        {
            break;
        }

        if (SQLITE_OK != sqlite3_exec(dbHandler, "BEGIN;", NULL, NULL, NULL))
        {
            break;
        }

        sql = L"delete from table1 where name = 'danjing'";
        if (SQLITE_OK != sqlite3_prepare16(dbHandler, sql.c_str(), -1, &stmt, NULL))
        {
            break;
        }

        if (SQLITE_DONE != sqlite3_step(stmt))
        {
            break;
        }

        if (SQLITE_OK != sqlite3_exec(dbHandler, "COMMIT;", NULL, NULL, NULL))
        {
            sqlite3_exec(dbHandler, "ROLLBACK;", NULL, NULL, NULL);
            break;
        }

    } while (0);

    if (dbHandler)
    {
        sqlite3_close(dbHandler);
    }*/


    return 0;
}

