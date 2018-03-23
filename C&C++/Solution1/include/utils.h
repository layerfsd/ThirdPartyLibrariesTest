#pragma once

#include <Windows.h>
#include <strsafe.h>
#include <string>
#include <time.h>


namespace utils
{
    inline time_t FileTimeToUnixTime(FILETIME fileTime)
    {
        time_t interval = 116444736000000000;
        time_t time = fileTime.dwHighDateTime;
        time = ((time << 32 | fileTime.dwLowDateTime) - interval) / 10000000;
        return time;
    }

    inline FILETIME UnixTimeToFileTime(time_t UnixTime)
    {
        FILETIME time = {0};
        LONGLONG lYear = 116444736000000000; 
        LONGLONG ll = Int32x32To64(UnixTime, 10000000) + lYear;
        time.dwLowDateTime = (DWORD)ll;
        time.dwHighDateTime = (DWORD)(ll >>32);
        return time;
    }

    inline std::wstring FormatFileSize(ULONGLONG size)
    {
        double fileSize = (double)size;
        wchar_t buffer[128] = {0};

        if (fileSize)
        {
            if (fileSize < 1024.00)
            {
                StringCbPrintfW(buffer, sizeof(buffer), L"%.2lfB", fileSize);
            }
            else if(fileSize < 1024.00 * 1024.00)
            {
                StringCbPrintfW(buffer, sizeof(buffer), L"%.2lfKB", fileSize / 1024.00);
            }
            else if(fileSize < 1024.00 * 1024.00 * 1024.00)
            {
                StringCbPrintfW(buffer, sizeof(buffer), L"%.2lfMB", fileSize / 1024.00 / 1024.00);
            }
            else
            {
                StringCbPrintfW(buffer, sizeof(buffer), L"%.2lfGB", fileSize / 1024.00 / 1024.00 / 1024.00);
            }
        }

        return std::wstring(buffer);
    }
}
