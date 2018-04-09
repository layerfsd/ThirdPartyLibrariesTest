#ifndef _DEBUG_LOG_H
#define _DEBUG_LOG_H


#include <Windows.h>
#include <Strsafe.h>
#include <assert.h>


namespace DebugLog
{
    inline void LogA(const char *format, ...)
    {	
        if (!format)
        {
            return;
        }

        char buffer[4096] = {0};

        // time
        SYSTEMTIME sysTime = {0};
        GetSystemTime(&sysTime);
        StringCbPrintfA(buffer, sizeof(buffer), "[%04u/%02u/%02u %02u:%02u:%02u]\r\n", sysTime.wYear,
            sysTime.wMonth, sysTime.wDay, sysTime.wHour + 8, sysTime.wMinute, sysTime.wSecond);

#ifdef _CONSOLE
        printf(buffer);
#else
        OutputDebugStringA(buffer);
#endif

        // log
        memset(buffer, 0, sizeof(buffer));
        va_list marker;
        va_start(marker, format);
        StringCbVPrintfA(buffer, sizeof(buffer), format, marker);
        va_end(marker);
        StringCbCatA(buffer, sizeof(buffer), "\r\n");

#ifdef _CONSOLE
        printf(buffer);
#else
        OutputDebugStringA(buffer);
#endif
    }

    inline void LogW(const wchar_t *format, ...)
    {	
        if (!format)
        {
            return;
        }

        wchar_t buffer[4096] = {0};

        // time
        SYSTEMTIME sysTime = {0};
        GetSystemTime(&sysTime);
        StringCbPrintfW(buffer, sizeof(buffer), L"[%04u/%02u/%02u %02u:%02u:%02u]\r\n", sysTime.wYear,
            sysTime.wMonth, sysTime.wDay, sysTime.wHour + 8, sysTime.wMinute, sysTime.wSecond);

#ifdef _CONSOLE
        wprintf(buffer);
#else
        OutputDebugStringW(buffer);
#endif

        // log
        memset(buffer, 0, sizeof(buffer));
        va_list marker;
        va_start(marker, format);
        StringCbVPrintfW(buffer, sizeof(buffer), format, marker);
        va_end(marker);
        StringCbCatW(buffer, sizeof(buffer), L"\r\n");

#ifdef _CONSOLE
        wprintf(buffer);
#else
        OutputDebugStringW(buffer);
#endif
    }

    inline void LogToFileA(const char *logFile, const char *format, ...)
    {	
        if (!logFile || !format)
        {
            return;
        }

        DWORD writeBytes = 0;
        HANDLE fileHandle = CreateFileA(logFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            return;
        }

        SetFilePointer(fileHandle, 0, 0, FILE_END);
        char buffer[4096] = {0};

        // time
        SYSTEMTIME sysTime = {0};
        GetSystemTime(&sysTime);
        StringCbPrintfA(buffer, sizeof(buffer), "[%04u/%02u/%02u %02u:%02u:%02u]\r\n", sysTime.wYear,
            sysTime.wMonth, sysTime.wDay, sysTime.wHour + 8, sysTime.wMinute, sysTime.wSecond);
        WriteFile(fileHandle, buffer, strlen(buffer), &writeBytes, NULL);

        // log
        memset(buffer, 0, sizeof(buffer));
        va_list marker;
        va_start(marker, format);
        StringCbVPrintfA(buffer, sizeof(buffer), format, marker);
        va_end(marker);
        StringCbCatA(buffer, sizeof(buffer), "\r\n");
        WriteFile(fileHandle, buffer, strlen(buffer), &writeBytes, NULL);

        CloseHandle(fileHandle);
    }

    inline void LogToFileW(const wchar_t *logFile, const wchar_t *format, ...)
    {	
        if (!logFile || !format)
        {
            return;
        }

        unsigned char head[] = {0xFF, 0xFE};
        DWORD writeBytes = 0;
        HANDLE fileHandle = CreateFileW(logFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            return;
        }
        else
        {
            // 首次创建文件写入UTF16 BOM头
            if (0 == ::GetLastError())
            {
                ::WriteFile(fileHandle, head, sizeof(head), &writeBytes, NULL);
            }
        }
        
        SetFilePointer(fileHandle, 0, 0, FILE_END);
        wchar_t buffer[4096] = {0};

        // time
        SYSTEMTIME sysTime = {0};
        GetSystemTime(&sysTime);
        StringCbPrintfW(buffer, sizeof(buffer), L"[%04u/%02u/%02u %02u:%02u:%02u]\r\n", sysTime.wYear,
            sysTime.wMonth, sysTime.wDay, sysTime.wHour + 8, sysTime.wMinute, sysTime.wSecond);
        WriteFile(fileHandle, buffer, wcslen(buffer) * sizeof(buffer[0]), &writeBytes, NULL);

        // log
        memset(buffer, 0, sizeof(buffer));
        va_list marker;
        va_start(marker, format);
        StringCbVPrintfW(buffer, sizeof(buffer), format, marker);
        va_end(marker);
        StringCbCatW(buffer, sizeof(buffer), L"\r\n");
        WriteFile(fileHandle, buffer, wcslen(buffer) * sizeof(buffer[0]), &writeBytes, NULL);

        CloseHandle(fileHandle);
    }
}


#endif //_DEBUG_LOG_H