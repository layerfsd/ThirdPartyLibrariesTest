#pragma once
#include <tchar.h>
#include <stdio.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString constructors will be explicit

#include <atlbase.h>
#include <atlstr.h>
#include <atlcoll.h>
#include <specstrings.h>
#include <commdlg.h>
#include <new>

#ifndef IID_PPV_ARGS
#define IID_PPV_ARGS(ppType) __uuidof(**(ppType)), (static_cast<IUnknown *>(*(ppType)),reinterpret_cast<void**>(ppType))
#endif

#include <PortableDeviceApi.h>      // Include this header for Windows Portable Device API interfaces
#include <PortableDevice.h>         // Include this header for Windows Portable Device definitions
#include <string>
#include <list>
#include <map>


// MTP模式
#define MTP_MODE 1

// USB大容量存储模式
#define USB_MODE 2

enum FILE_TYPE
{
    IMAGE_FILE = 1,     // 图片
    VIDEO_FILE,         // 视频
    AUDIO_FILE          // 音频
};

struct ObjectInfo
{
    std::wstring name;              // 文件名 eg. xxx
    std::wstring suffix;            // 后缀名 eg. .jpg
    std::wstring deviceDir;         // 文件设备路径 eg. /手机存储/DCIM
    std::wstring localDir;          // 文件本地路径 eg. E:\MobileData\Meizu[20170622_172059]\Temp\手机存储\DCIM\ 
    // std::wstring thumbPath;
    // 缩略图路径 = localDir + name + _thumb + suffix eg. E:\MobileData\Meizu[20170622_172059]\手机存储\DCIM\xxx_thumb.jpg
    // 视频帧路径 = localDir + name + _thumb + %02d + suffix eg. E:\MobileData\Meizu[20170622_172059]\手机存储\DCIM\xxx_thumb01.jpg
    ULONGLONG lastModifiedTime;     // 最后修改时间（秒数）
    ULONGLONG duration;             // 视频/音频时长（毫秒数）
    ULONGLONG size;                 // 文件大小（字节数，使用时以1000为单位进行换算）
    std::wstring objectID;          // 对象ID（MTP模式下使用）
    std::wstring parentID;
    std::wstring uniqueID;
    bool isFolder;
    GUID contentType;
};

enum DEV_TYPE
{
    DEV_TYPE_IOS = 1,
    DEV_TYPE_ANDROID
};

// 设备信息
typedef struct _DeviceInfo
{
    int mode;                           // 设备模式 eg. MTP_MODE/AFC_MODE
    BOOL bJailBroken;                   // 是否越狱或者ROOT
    DEV_TYPE devType;                   // 设备类型 eg. IOS or ANDROID
    std::string deviceId;               // 设备ID
    std::string serialNumer;            // 设备IMEI
    std::wstring manufacturer;          // 制造商
    std::wstring friendlyName;          // 设备名称
    std::string phoneNumber;            // 手机号码
    ULONGLONG storageTotalSize;         // 手机内部存储总大小 (字节数)
    ULONGLONG storageAvailableSize;     // 手机内部存储剩余大小 (字节数)
    ULONGLONG SDcardTotalSize;          // SD卡存储总大小 (字节数)
    ULONGLONG SDcardAvailableSize;      // SD卡剩余大小 (字节数)
}DeviceInfo;

struct FolderInfo
{
    std::wstring ID;
    std::wstring uniqueID;
};

struct PathInfo
{
    bool hasNext;
    std::wstring path;
};

typedef void (*CB_ObjectInfo)(const ObjectInfo *objectInfo);

class MTPInfo
{
public:
    MTPInfo(const CB_ObjectInfo cbObjectInfo = 0);
    ~MTPInfo();

    void StartTask(const std::wstring &outPutPath, const std::wstring &targetPath);
    int CheckDeviceMode();
    bool IsDeviceConnectBreak();

private:
    std::string m_exePathA;
    std::wstring m_exePathW;
    std::wstring m_outPutPath;
    std::wstring m_baseDir;
    DeviceInfo m_deviceInfo;
    bool m_stopAll;
    CB_ObjectInfo m_cbObjectInfo;
    bool m_isFileSizeLimit;

    int m_layers;

    bool m_isAcqureOriginalImage;

    // 需要遍历路径
    // <key, value> = <路径深度, 路径信息>
    std::map<int, std::list<PathInfo>> m_appPathMap;

    // <key, value> = <object unique ID, objectinfo>
    std::map<std::wstring, ObjectInfo*> m_mtpObjectInfoMap;

    std::list<FolderInfo> m_folderList;

private:
    void ClearData();

    bool OpenDevice(IPortableDevice **device);

    inline int IsTargetPath(const std::wstring &path);

    void GetClientInformation(IPortableDeviceValues **ppClientInformation);

    inline ULONGLONG GetTimeInfo(PWSTR timeStr);

    void GetParentObjectInfo(IPortableDeviceContent *content,
        PCWSTR parentObjectID,
        ObjectInfo **parentObjectInfo);

    void GetObjectInfo(IPortableDeviceContent *content,
        PCWSTR objectID,
        ObjectInfo *objectInfo);

    void GetDeviceInfo(IPortableDeviceContent *content,
        DeviceInfo &deviceInfo);
    void GetDeviceInfoEx(IPortableDeviceContent *content,
        PCWSTR objectID,
        DeviceInfo &deviceInfo);

	void RecursiveEnumerateFolder(IPortableDeviceContent *content,
		PCWSTR objectID);

    void RecursiveEnumerateFile(IPortableDeviceContent *content,
        const FolderInfo &folderInfo);

    void AcquireTargetFolder(const FolderInfo &folderInfo);

    void EnumerateAllContent();

    HRESULT StreamCopy(IStream *pDestStream,
        IStream *pSourceStream,
        DWORD cbTransferSize,
        DWORD *pcbWritten);

    bool AcquireObject(IPortableDeviceContent *content,
        const ObjectInfo *mtpObjectInfo,
        std::wstring &targetFilePath);
};
