#ifndef _SQLITE3_HELP_H
#define _SQLITE3_HELP_H

#include "..\Utils\sqlite3.h"
//#include <processthread.h>

typedef int (*fnsqlite3_prepare)(
	sqlite3 *,             
	const char *,        
	int,              
	sqlite3_stmt **,  
	const char **       
	);

typedef int (* fnsqlite3_prepare_v2)(
	sqlite3* ,
	const char *,
	int,
	sqlite3_stmt **,
	const char **
	);

typedef sqlite3_int64 (* fnsqlite3_column_int64)(
	sqlite3_stmt*, 
	int);

typedef int (* fnsqlite3_finalize) (
	sqlite3_stmt*
	);

typedef int (*fnsqlite3_step)(
	sqlite3_stmt*
	);

typedef int (*fnsqlite3_open)(
	const char *,
	sqlite3 **
	);

typedef int (*fnsqlite3_close)(
	sqlite3 *
	);

typedef const unsigned char * (*fnsqlite3_column_text)(
	sqlite3_stmt*,
	int iCol
	);

typedef const void *(*fnsqlite3_column_text16)(
    sqlite3_stmt*,
    int iCol
    );

typedef const void * (*fnsqlite3_column_blob)(
	sqlite3_stmt*, 
	int iCol
	);

typedef int (*fnsqlite3_column_bytes)(
	sqlite3_stmt*,
	int iCol
	);

typedef double (*fnsqlite3_column_double)(
    sqlite3_stmt*,
    int iCol
    );

typedef int (*fnsqlite3_close)(
	sqlite3 *
	);

typedef int (*fnsqlite3_get_table)(
	sqlite3 *,          
	const char *,    
	char ***,    
	int *,            
	int *,         
	char ** 
	);

typedef void (*fnsqlite3_free_table)(
	char **
	);

typedef int (*fnsqlite3_open16)(
	const void *,    
	sqlite3 ** 
	);

typedef int (*fnsqlite3_exec)(
		sqlite3*, 
		const char *, 
		int (*callback)(void*,int,char**,char**), 
		void *,  
		char **);

typedef int (*fnsqlite3_bind_blob)(
		sqlite3_stmt*,
		int, 
		const void*, 
		int n, 
		void(*)(void*));



typedef struct _SQLITE3_HELPER 
{
	fnsqlite3_open16	sqlite3_open16;
	fnsqlite3_prepare	sqlite3_prepare;
	fnsqlite3_step		sqlite3_step; 
	fnsqlite3_open		sqlite3_open; 
	fnsqlite3_close		sqlite3_close;
	fnsqlite3_get_table		sqlite3_get_table;
	fnsqlite3_free_table	sqlite3_free_table;
	fnsqlite3_column_text	sqlite3_column_text;
    fnsqlite3_column_text16 sqlite3_column_text16;
	fnsqlite3_column_blob	sqlite3_column_blob; 
	fnsqlite3_column_bytes	sqlite3_column_bytes; 
	fnsqlite3_prepare_v2	sqlite3_prepare_v2;
	fnsqlite3_column_int64	sqlite3_column_int64;
    fnsqlite3_column_double sqlite3_column_double;
	fnsqlite3_finalize		sqlite3_finalize;
	fnsqlite3_exec          sqlite3_exec;
	fnsqlite3_bind_blob     sqlite3_bind_blob;
}SQLITE3_HELPER, *PSQLITE3_HELPER;

extern	SQLITE3_HELPER Sqlite3Helper;

inline
BOOL
WINAPI
InitialSqlite3Helper()
{
	HMODULE hSqlite3 = LoadLibraryW(L"sqlite3.dll");
	if (!hSqlite3)
	{
		return FALSE;
	}

	Sqlite3Helper.sqlite3_open16		= (fnsqlite3_open16)GetProcAddress(hSqlite3, "sqlite3_open16");
	Sqlite3Helper.sqlite3_get_table		= (fnsqlite3_get_table)GetProcAddress(hSqlite3, "sqlite3_get_table");
	Sqlite3Helper.sqlite3_free_table	= (fnsqlite3_free_table)GetProcAddress(hSqlite3, "sqlite3_free_table");
	Sqlite3Helper.sqlite3_open			= (fnsqlite3_open)GetProcAddress(hSqlite3, "sqlite3_open");
	Sqlite3Helper.sqlite3_close			= (fnsqlite3_close)GetProcAddress(hSqlite3, "sqlite3_close");
	Sqlite3Helper.sqlite3_prepare		= (fnsqlite3_prepare)GetProcAddress(hSqlite3, "sqlite3_prepare");
	Sqlite3Helper.sqlite3_step			= (fnsqlite3_step)GetProcAddress(hSqlite3, "sqlite3_step");
    Sqlite3Helper.sqlite3_column_text	= (fnsqlite3_column_text)GetProcAddress(hSqlite3, "sqlite3_column_text");
    Sqlite3Helper.sqlite3_column_text16	= (fnsqlite3_column_text16)GetProcAddress(hSqlite3, "sqlite3_column_text16");
	Sqlite3Helper.sqlite3_column_blob	= (fnsqlite3_column_blob)GetProcAddress(hSqlite3, "sqlite3_column_blob");
	Sqlite3Helper.sqlite3_column_bytes	= (fnsqlite3_column_bytes)GetProcAddress(hSqlite3, "sqlite3_column_bytes");
	Sqlite3Helper.sqlite3_prepare_v2	= (fnsqlite3_prepare_v2)GetProcAddress(hSqlite3, "sqlite3_prepare_v2");
    Sqlite3Helper.sqlite3_column_int64	= (fnsqlite3_column_int64)GetProcAddress(hSqlite3, "sqlite3_column_int64");
    Sqlite3Helper.sqlite3_column_double	= (fnsqlite3_column_double)GetProcAddress(hSqlite3, "sqlite3_column_double");
	Sqlite3Helper.sqlite3_finalize		= (fnsqlite3_finalize)GetProcAddress(hSqlite3, "sqlite3_finalize");
	Sqlite3Helper.sqlite3_exec          = (fnsqlite3_exec)GetProcAddress(hSqlite3, "sqlite3_exec");
	Sqlite3Helper.sqlite3_bind_blob     = (fnsqlite3_bind_blob)GetProcAddress(hSqlite3, "sqlite3_bind_blob");

	return TRUE;
}

#endif