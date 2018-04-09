#include "MTPInfo.h"
#include <Windows.h>
#include <Shlobj.h>
#include <strsafe.h>
#include <time.h>
#include <DebugLog.h>
#include <utils.h>
#include "..\Utils\StringConvertor.h"


#define NUM_OBJECTS_TO_REQUEST  10

#define CLIENT_NAME         L"WPD Sample Application"
#define CLIENT_MAJOR_VER    1
#define CLIENT_MINOR_VER    0
#define CLIENT_REVISION     2





// Helper class to convert a GUID to a string
class CGuidToString
{
private:        
    WCHAR _szGUID[64];

public:
    CGuidToString(REFGUID guid)
    {
        if (!::StringFromGUID2(guid,  _szGUID, 64))
        {
            _szGUID[0] = L'\0';
        }
    }

    operator PWSTR()
    {
        return _szGUID;
    }
};



MTPInfo::MTPInfo(const CB_ObjectInfo cbObjectInfo)
{
    m_cbObjectInfo = cbObjectInfo;
}

MTPInfo::~MTPInfo()
{
    for (std::map<std::wstring, ObjectInfo*>::iterator iter = m_mtpObjectInfoMap.begin();
        iter != m_mtpObjectInfoMap.end();
        ++iter)
    {
        if (iter->second)
        {
            delete iter->second;
        }
    }
}

void MTPInfo::StartTask(const std::wstring &outPutPath, const std::wstring &targetPath)
{
    m_outPutPath = outPutPath;

    PathInfo pathInfo;
    pathInfo.path = targetPath;
    pathInfo.hasNext = false;
    if (m_appPathMap.end() != m_appPathMap.find(3))
    {
        m_appPathMap[3].push_back(pathInfo);
    }
    else
    {
        std::list<PathInfo> pathInfos;
        pathInfos.push_back(pathInfo);
        m_appPathMap.insert(std::make_pair(3, pathInfos));
    }

    // Initialize COM for COINIT_MULTITHREADED
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        EnumerateAllContent();

        // Uninitialize COM
        CoUninitialize();
    }
}

int MTPInfo::CheckDeviceMode()
{
    ClearData();

    // Initialize COM for COINIT_MULTITHREADED
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        CComPtr<IPortableDevice> device;

        do 
        {
            if (!OpenDevice(&device) || !device)
            {
                break;
            }

        } while (false);
        
        // Uninitialize COM
        CoUninitialize();
        
        return m_deviceInfo.mode;
    }
    else
    {
        return 0;
    }
}

bool MTPInfo::IsDeviceConnectBreak()
{
    HRESULT hr = S_OK;
    CComPtr<IPortableDevice> device;
    PWSTR *deviceIDs = NULL;
    DWORD deviceCount = 0;
    bool ret = true;
    DeviceInfo oldDeviceInfo = m_deviceInfo;

    do 
    {
        if (!OpenDevice(&device) || !device)
        {
            break;
        }

        if (oldDeviceInfo.friendlyName != m_deviceInfo.friendlyName ||
            oldDeviceInfo.manufacturer != m_deviceInfo.manufacturer ||
            oldDeviceInfo.serialNumer != m_deviceInfo.serialNumer ||
            oldDeviceInfo.storageAvailableSize != m_deviceInfo.storageAvailableSize)
        {
            break;
        }

        ret = false;
    } while (false);

    return ret;
}

bool MTPInfo::OpenDevice(IPortableDevice **device)
{
    if (!device)
    {
        return false;
    }

    bool ret = false;
    HRESULT hr = S_OK;
    CComPtr<IPortableDeviceManager> deviceManager;
    CComPtr<IPortableDeviceValues> clientInformation;
    PWSTR *deviceIDs = NULL;
    DWORD deviceCount = 0;
    (*device) = NULL;

    do 
    {
        // Fill out information about your application, so the device knows
        // who they are speaking to.
        GetClientInformation(&clientInformation);

        hr = CoCreateInstance(CLSID_PortableDeviceManager,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&deviceManager));
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to CoCreateInstance CLSID_PortableDeviceManager, hr = 0x%lx\r\n", hr);
            break;
        }

        // First, pass NULL as the PWSTR array pointer to get the total number
        // of devices found on the system.
        //<SnippetDeviceEnum2>
        hr = deviceManager->GetDevices(NULL, &deviceCount);
        if (FAILED(hr) || !deviceCount)
        {
            DebugLog::LogW(L"! Failed to get number of devices on the system, hr = 0x%lx\r\n", hr);
            break;
        }

        deviceIDs = new (std::nothrow)PWSTR[deviceCount];
        if (!deviceIDs)
        {
            DebugLog::LogW(L"! Failed to allocate memory for PWSTR array\r\n");
            break;
        }

        hr = deviceManager->GetDevices(deviceIDs, &deviceCount);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to get the device list from the system, hr = 0x%lx\r\n", hr);
            break;
        }

        DWORD index = 0;
        for (; index < deviceCount; ++index)
        {
            hr = CoCreateInstance(CLSID_PortableDeviceFTM,
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(device));
            if (FAILED(hr) || !(*device))
            {
                DebugLog::LogW(L"! Failed to CoCreateInstance CLSID_PortableDeviceFTM, hr = 0x%lx\r\n", hr);
                break;
            }

            hr = (*device)->Open(deviceIDs[index], clientInformation);
            if (FAILED(hr))
            {
                if (hr == E_ACCESSDENIED)
                {
                    DebugLog::LogW(L"Failed to Open the device for Read Write access, will open it for Read-only access instead\r\n");

                    clientInformation->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS, GENERIC_READ);
                    hr = (*device)->Open(deviceIDs[index], clientInformation);
                    if (FAILED(hr))
                    {
                        DebugLog::LogW(L"! Failed to Open the device %u, hr = 0x%lx\r\n", index, hr);
                        // Release the IPortableDevice interface, because we cannot proceed
                        // with an unopen device.
                        (*device)->Release();
                        *device = NULL;
                        continue;
                    }
                }
                else
                {
                    DebugLog::LogW(L"! Failed to Open the device %u, hr = 0x%lx\r\n", index, hr);
                    // Release the IPortableDevice interface, because we cannot proceed
                    // with an unopen device.
                    (*device)->Release();
                    *device = NULL;
                    continue;
                }
            }

            if (NULL != (*device))
            {
                CComPtr<IPortableDeviceContent> content;
                hr = (*device)->Content(&content);
                if (SUCCEEDED(hr) || content)
                {
                    DeviceInfo deviceInfo;
                    deviceInfo.bJailBroken = FALSE;
                    deviceInfo.devType = (DEV_TYPE)-1;
                    deviceInfo.mode = 0;
                    deviceInfo.SDcardAvailableSize = 0;
                    deviceInfo.SDcardTotalSize = 0;
                    deviceInfo.storageAvailableSize = 0;
                    deviceInfo.storageTotalSize = 0;
                    GetDeviceInfo(content, deviceInfo);

                    // 成功打开设备
                    if ((DEV_TYPE)-1 != deviceInfo.devType)
                    {
                        //DebugLog::LogW(L"friendName = %s\r\nmanufacturer = %s\r\nmode = %d\r\n", deviceInfo.friendlyName.c_str(), deviceInfo.manufacturer.c_str(), deviceInfo.mode);
                        m_deviceInfo = deviceInfo;
                        break;
                    }
                }
            }

            (*device)->Release();
            *device = NULL;
        }

        if (index < deviceCount)
        {
            ret = true;
        }
    } while (FALSE);

    // Free all returned PnPDeviceID strings by using CoTaskMemFree.
    // NOTE: CoTaskMemFree can handle NULL pointers, so no NULL
    //       check is needed.
    if (deviceIDs)
    {
        for (DWORD dwIndex = 0; dwIndex < deviceCount; ++dwIndex)
        {
            if (deviceIDs[dwIndex])
            {
                CoTaskMemFree(deviceIDs[dwIndex]);
                deviceIDs[dwIndex] = NULL;
            }
        }

        // Delete the array of PWSTR pointers
        delete [] deviceIDs;
        deviceIDs = NULL;
    }

    return ret;
}

void MTPInfo::ClearData()
{
    m_layers = 1;
    m_stopAll = false;
    m_isAcqureOriginalImage = true;
    m_appPathMap.clear();
    m_folderList.clear();
    m_outPutPath.clear();

    m_deviceInfo.bJailBroken = FALSE;
    m_deviceInfo.mode = 0;
    m_deviceInfo.SDcardAvailableSize = 0;
    m_deviceInfo.SDcardTotalSize = 0;
    m_deviceInfo.storageAvailableSize = 0;
    m_deviceInfo.storageTotalSize = 0;
    m_deviceInfo.devType = (DEV_TYPE)-1;

    for (std::map<std::wstring, ObjectInfo*>::iterator iter = m_mtpObjectInfoMap.begin();
        iter != m_mtpObjectInfoMap.end();
        ++iter)
    {
        if (iter->second)
        {
            delete iter->second;
            iter->second = NULL;
        }
    }

    m_mtpObjectInfoMap.clear();
}

int MTPInfo::IsTargetPath(const std::wstring &path)
{
    int ret = 99;
    std::map<int, std::list<PathInfo>>::iterator iter = m_appPathMap.end();

    if (m_appPathMap.empty())
    {
        return -1;
    }

    iter = m_appPathMap.find(m_layers);
    if (m_appPathMap.end() == iter)
    {
        return -2;
    }

    std::list<PathInfo>::iterator iter2 = iter->second.begin();
    for (;
        iter2 != iter->second.end();
        ++iter2)
    {
        if (!_wcsicmp(path.c_str(), iter2->path.c_str()))
        {
            if (iter2->hasNext)
            {
                ret = 1;
            }
            else
            {
                ret = 0;
            }
        }
    }

    if (iter2 != iter->second.end())
    {
        iter->second.erase(iter2);
    }
   
    if (iter->second.empty())
    {
        m_appPathMap.erase(iter);
    }

    return ret;
}

// Creates and populates an IPortableDeviceValues with information about
// this application.  The IPortableDeviceValues is used as a parameter
// when calling the IPortableDevice::Open() method.
void MTPInfo::GetClientInformation(IPortableDeviceValues **ppClientInformation)
{
    // Client information is optional.  The client can choose to identify itself, or
    // to remain unknown to the driver.  It is beneficial to identify yourself because
    // drivers may be able to optimize their behavior for known clients. (e.g. An
    // IHV may want their bundled driver to perform differently when connected to their
    // bundled software.)

    // CoCreate an IPortableDeviceValues interface to hold the client information.
    HRESULT hr = CoCreateInstance(CLSID_PortableDeviceValues,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(ppClientInformation));
    if (SUCCEEDED(hr) && ppClientInformation)
    {
        // Attempt to set all bits of client information
        hr = (*ppClientInformation)->SetStringValue(WPD_CLIENT_NAME, CLIENT_NAME);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to set WPD_CLIENT_NAME, hr = 0x%lx\r\n", hr);
        }

        hr = (*ppClientInformation)->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, CLIENT_MAJOR_VER);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to set WPD_CLIENT_MAJOR_VERSION, hr = 0x%lx\r\n",hr);
        }

        hr = (*ppClientInformation)->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, CLIENT_MINOR_VER);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to set WPD_CLIENT_MINOR_VERSION, hr = 0x%lx\r\n", hr);
        }

        hr = (*ppClientInformation)->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, CLIENT_REVISION);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to set WPD_CLIENT_REVISION, hr = 0x%lx\r\n", hr);
        }

        //  Some device drivers need to impersonate the caller in order to function correctly.  Since our application does not
        //  need to restrict its identity, specify SECURITY_IMPERSONATION so that we work with all devices.
        hr = (*ppClientInformation)->SetUnsignedIntegerValue(WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to set WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, hr = 0x%lx\n",hr);
        }
    }
    else
    {
        DebugLog::LogW(L"! Failed to CoCreateInstance CLSID_PortableDeviceValues, hr = 0x%lx\r\n", hr);
    }
}

ULONGLONG MTPInfo::GetTimeInfo(PWSTR timeStr)
{
    if (!timeStr || 30 < wcslen(timeStr))
    {
        return 0;
    }

    WCHAR buffer1[64] = {0};
    WCHAR buffer2[64] = {0};
    WCHAR seps1[] = L"/";
    WCHAR seps2[] = L":";
    WCHAR *token = NULL;
    WCHAR *next_token = NULL;
    int index = 0;
    int value = 0;
    struct tm timeInfo = { 0 };

    WCHAR *ptr = wcschr(timeStr, L' ');
    if (ptr)
    {
        *ptr = L'\0';
        ++ptr;
        StringCbCopyW(buffer1, sizeof(buffer1), timeStr);
        StringCbCopyW(buffer2, sizeof(buffer2), ptr);
    }
    else
    {
        ptr = NULL;
        ptr = wcschr(timeStr, L':');
        if (ptr)
        {
            *ptr = L'\0';
            ++ptr;
            StringCbCopyW(buffer1, sizeof(buffer1), timeStr);
            StringCbCopyW(buffer2, sizeof(buffer2), ptr);
        }
        else
        {
            return 0;
        }
    }

    // Establish string and get the first token:  
    token = wcstok_s(buffer1, seps1, &next_token);

    while (token)
    {
        value = _wtoi(token);
        switch (index)
        {
        case 0:
            timeInfo.tm_year = value - 1900;
            break;
        case 1:
            timeInfo.tm_mon = value - 1;
            break;
        case 2:
            timeInfo.tm_mday = value;
        default:
            break;
        }

        // Get next token:  
        token = wcstok_s(NULL, seps1, &next_token);
        ++index;
    }

    // Establish string and get the first token:
    token = NULL;
    next_token = NULL;
    token = wcstok_s(buffer2, seps2, &next_token);

    while (token)
    {
        value = _wtoi(token);
        switch (index)
        {
        case 3:
            timeInfo.tm_hour = value - 1;
            break;
        case 4:
            timeInfo.tm_min = value - 1;
        case 5:
            timeInfo.tm_sec = value - 1;
        default:
            break;
        }

        // Get next token:  
        token = wcstok_s(NULL, seps2, &next_token);
        ++index;
    }

    return mktime(&timeInfo);
}

void MTPInfo::GetParentObjectInfo(IPortableDeviceContent *content,
                                  PCWSTR parentObjectID,
                                  ObjectInfo **parentObjectInfo)
{
    if (!content ||
        !parentObjectID ||
        !parentObjectInfo)
    {
        return;
    }

    HRESULT									hr = S_OK;
    CComPtr<IPortableDeviceProperties>		properties = NULL;
    CComPtr<IPortableDeviceValues>			objectProperties = NULL;
    CComPtr<IPortableDeviceKeyCollection>	propertiesToRead = NULL;
    PWSTR                                   objectUniqueID = NULL;

    do
    {
        // 1) Get an IPortableDeviceProperties interface from the IPortableDeviceContent interface
        // to access the property-specific methods.
        hr = content->Properties(&properties);
        if (FAILED(hr) || !properties)
        {
            DebugLog::LogW(L"! Failed to get IPortableDeviceProperties from IPortableDevice, hr = 0x%lx\r\n", hr);
            break;
        }

        // 2) CoCreate an IPortableDeviceKeyCollection interface to hold the the property keys
        // we wish to read.
        hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&propertiesToRead));
        if (FAILED(hr) || !propertiesToRead)
        {
            break;
        }

        hr = propertiesToRead->Add(WPD_OBJECT_PERSISTENT_UNIQUE_ID);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_OBJECT_PERSISTENT_UNIQUE_ID to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            break;
        }

        // 3) Call GetValues() passing the collection of specified PROPERTYKEYs.
        hr = properties->GetValues(parentObjectID,  // The object whose properties we are reading
            propertiesToRead,     // The properties we want to read
            &objectProperties);         // Driver supplied property values for the specified object
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to get all properties for object '%s', hr= 0x%lx\n", parentObjectID, hr);
            break;
        }

        // 4) Get object unique Id
        hr = objectProperties->GetStringValue(WPD_OBJECT_PERSISTENT_UNIQUE_ID, &objectUniqueID);
        if (SUCCEEDED(hr) && objectUniqueID)
        {
            std::map<std::wstring, ObjectInfo*>::iterator iter = m_mtpObjectInfoMap.find(objectUniqueID);
            CoTaskMemFree(objectUniqueID);
            objectUniqueID = NULL;

            if (m_mtpObjectInfoMap.end() != iter)
            {
                *parentObjectInfo = iter->second;
            }
        }
       
    } while (false);
}

void MTPInfo::GetObjectInfo(IPortableDeviceContent *content,
                            PCWSTR objectID,
                            ObjectInfo *mtpObjectInfo)
{
    if (!mtpObjectInfo)
    {
        return;
    }

    mtpObjectInfo->isFolder = false;
    mtpObjectInfo->objectID = objectID ? objectID : L"";
    mtpObjectInfo->duration = 0;
    mtpObjectInfo->lastModifiedTime = 0;
    mtpObjectInfo->size = 0;

    if (!content || !objectID)
    {
        return;
    }

    HRESULT									hr = S_OK;
    CComPtr<IPortableDeviceProperties>		properties = NULL;
    CComPtr<IPortableDeviceValues>			objectProperties = NULL;
    CComPtr<IPortableDeviceKeyCollection>	propertiesToRead = NULL;
    PWSTR                                   displayName = NULL;
    PWSTR                                   originalName = NULL;
    PWSTR                                   parentID = NULL;
    PWSTR                                   time = NULL;
    PWSTR                                   objectUniqueID = NULL;
    bool                                    appendPath = true;
    ObjectInfo                              *objectParentInfo = NULL; 
    size_t                                  pos = std::wstring::npos;
    std::wstring                            name;

    do
    {
        // 1) Get an IPortableDeviceProperties interface from the IPortableDeviceContent interface
        // to access the property-specific methods.
        hr = content->Properties(&properties);
        if (FAILED(hr) || !properties)
        {
            DebugLog::LogW(L"! Failed to get IPortableDeviceProperties from IPortableDevice, hr = 0x%lx\r\n", hr);
            break;
        }

        // 2) CoCreate an IPortableDeviceKeyCollection interface to hold the the property keys
        // we wish to read.
        hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&propertiesToRead));
        if (FAILED(hr) || !propertiesToRead)
        {
            break;
        }

        // 3) Populate the IPortableDeviceKeyCollection with the keys we wish to read.
        // NOTE: We are not handling any special error cases here so we can proceed with
        // adding as many of the target properties as we can.
        hr = propertiesToRead->Add(WPD_OBJECT_CONTENT_TYPE);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_OBJECT_CONTENT_TYPE to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            break;
        }

        hr = propertiesToRead->Add(WPD_OBJECT_NAME);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_OBJECT_NAME to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            break;
        }

        hr = propertiesToRead->Add(WPD_OBJECT_ORIGINAL_FILE_NAME);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_OBJECT_ORIGINAL_FILE_NAME to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            break;
        }

        hr = propertiesToRead->Add(WPD_OBJECT_PARENT_ID);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_OBJECT_PARENT_ID to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            break;
        }

        hr = propertiesToRead->Add(WPD_OBJECT_PERSISTENT_UNIQUE_ID);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_OBJECT_PERSISTENT_UNIQUE_ID to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            break;
        }

        hr = propertiesToRead->Add(WPD_OBJECT_DATE_MODIFIED);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_OBJECT_DATE_MODIFIED to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
        }

        hr = propertiesToRead->Add(WPD_MEDIA_DURATION);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_MEDIA_DURATION to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
        }

        hr = propertiesToRead->Add(WPD_OBJECT_SIZE);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to add WPD_OBJECT_SIZE to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
        }

        // 4) Call GetValues() passing the collection of specified PROPERTYKEYs.
        hr = properties->GetValues(objectID,                // The object whose properties we are reading
            propertiesToRead,   // The properties we want to read
            &objectProperties);       // Driver supplied property values for the specified object
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to get all properties for object '%s', hr= 0x%lx\n", objectID, hr);
            break;
        }

        // 5) Get object unique Id
        hr = objectProperties->GetStringValue(WPD_OBJECT_PERSISTENT_UNIQUE_ID, &objectUniqueID);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to get object unique ID for object '%s', hr= 0x%lx\n", objectID, hr);
            break;
        }

        if (objectUniqueID)
        {
            mtpObjectInfo->uniqueID = objectUniqueID;
        }
        else
        {
            mtpObjectInfo->uniqueID = objectID;
        }

        CoTaskMemFree(objectUniqueID);
        objectUniqueID = NULL;

        // 6) Get object parent Id
        hr = objectProperties->GetStringValue(WPD_OBJECT_PARENT_ID, &parentID);
        if (FAILED(hr) || !parentID)
        {
            DebugLog::LogW(L"! Failed to get object parent ID for object '%s', hr= 0x%lx\n", objectID, hr);
            break;
        }
        
        mtpObjectInfo->parentID = parentID;
        if (DEV_TYPE_IOS == m_deviceInfo.devType && L"o10001" == mtpObjectInfo->parentID)
        {
            mtpObjectInfo->parentID = L"s10001";
        }
        CoTaskMemFree(parentID);
        parentID = NULL;

        // 7) To determines whether the object is a folder
        GUID value = GUID_NULL;
        hr = objectProperties->GetGuidValue(WPD_OBJECT_CONTENT_TYPE, &value);
        if (SUCCEEDED(hr))
        {
            mtpObjectInfo->contentType = value;
            mtpObjectInfo->isFolder = !wcscmp((PCWSTR)CGuidToString(value), (PCWSTR)CGuidToString(WPD_CONTENT_TYPE_FOLDER));
        }
       
        if (L"" == mtpObjectInfo->parentID)
        {
            appendPath = false;
            mtpObjectInfo->isFolder = true;
        }

        if (WPD_DEVICE_OBJECT_ID == mtpObjectInfo->parentID)
        {
            mtpObjectInfo->isFolder = true;
        }

        // 8) Get original file name
        hr = objectProperties->GetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME, &originalName);
        if (SUCCEEDED(hr) && originalName)
        {
            name = originalName;
        }

        CoTaskMemFree(originalName);
        originalName = NULL;

        // 9) Get object display name
        hr = objectProperties->GetStringValue(WPD_OBJECT_NAME, &displayName);
        if (SUCCEEDED(hr) && displayName)
        {
            if (L"" == name)
            {
                name = displayName;
            }
        }

        CoTaskMemFree(displayName);
        displayName = NULL;

        mtpObjectInfo->name = name;
        if (!mtpObjectInfo->isFolder)
        {
            pos = mtpObjectInfo->name.rfind(L'.');
            if (std::wstring::npos != pos)
            {
                mtpObjectInfo->name = name.substr(0, pos);
                mtpObjectInfo->suffix = name.substr(pos);
            }
        }

        // 10) Get object path
        GetParentObjectInfo(content, mtpObjectInfo->parentID.c_str(), &objectParentInfo);
        if (objectParentInfo && appendPath)
        {
            mtpObjectInfo->deviceDir = objectParentInfo->deviceDir + objectParentInfo->name;
            mtpObjectInfo->localDir = objectParentInfo->localDir + objectParentInfo->name;

            if (objectParentInfo->isFolder)
            {
                if (!mtpObjectInfo->deviceDir.empty())
                {
                    mtpObjectInfo->deviceDir += L"/";
                }

                if (!mtpObjectInfo->localDir.empty())
                {
                    mtpObjectInfo->localDir += L"\\";
                }
            }

            for (size_t i = 0; i < mtpObjectInfo->localDir.size(); ++i)
            {
                if (L'/' == mtpObjectInfo->localDir[i])
                {
                    mtpObjectInfo->localDir[i] = L'\\';
                }
            }
        }
        else
        {
            mtpObjectInfo->localDir = m_outPutPath;
            mtpObjectInfo->deviceDir = L"/";
        }

        // 11) Get object last modified time
        hr = objectProperties->GetStringValue(WPD_OBJECT_DATE_MODIFIED, &time);
        if (SUCCEEDED(hr) && time)
        {
            mtpObjectInfo->lastModifiedTime = GetTimeInfo(time);
        }

        CoTaskMemFree(time);
        time = NULL;

        // 12) Get media object duration time
        hr = objectProperties->GetUnsignedLargeIntegerValue(WPD_MEDIA_DURATION, &mtpObjectInfo->duration);

        // 13) Get object size
        hr = objectProperties->GetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE, &mtpObjectInfo->size);

    } while (false);
}

void MTPInfo::GetDeviceInfo(IPortableDeviceContent *content,
                            DeviceInfo &deviceInfo)
{
    if (!content)
    {
        return;
    }

    HRESULT hr = S_OK;
    CComPtr<IEnumPortableDeviceObjectIDs> enumObjectIDs;
    DWORD  numFetched = 0;
    PWSTR  objectIDArray[NUM_OBJECTS_TO_REQUEST] = { 0 };  

    do
    {
        GetDeviceInfoEx(content, WPD_DEVICE_OBJECT_ID, deviceInfo);
       
        // Get an IEnumPortableDeviceObjectIDs interface by calling EnumObjects with the
        // specified parent object identifier.
        hr = content->EnumObjects(0,   // Flags are unused
            WPD_DEVICE_OBJECT_ID,     // Starting from the passed in object
            NULL,            // Filter is unused
            &enumObjectIDs);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to get IEnumPortableDeviceObjectIDs from IPortableDeviceContent, hr = 0x%lx\n", hr);
            break;
        }

        // Loop calling Next() while S_OK is being returned.
        while(hr == S_OK)
        {
            numFetched = 0;
            memset(objectIDArray, 0, sizeof(objectIDArray));

            hr = enumObjectIDs->Next(NUM_OBJECTS_TO_REQUEST,    // Number of objects to request on each NEXT call
                objectIDArray,             // Array of PWSTR array which will be populated on each NEXT call
                &numFetched);              // Number of objects written to the PWSTR array
            if (SUCCEEDED(hr))
            {
                if ((DEV_TYPE)-1 == deviceInfo.devType)
                {
                    // 目前无法区分U盘/HUB与大容量存储模式的手机，暂时屏蔽此功能
                    if (MTP_MODE == deviceInfo.mode/* ||
                        (0 == numFetched && USB_MODE == deviceInfo.mode)*/)
                    {
                        deviceInfo.devType = DEV_TYPE_ANDROID;
                    }
                }

                // Traverse the results of the Next() operation and recursively enumerate
                // Remember to free all returned object identifiers using CoTaskMemFree()
                for (DWORD index = 0; (index < numFetched) && (objectIDArray[index] != NULL); ++index)
                {
                    GetDeviceInfoEx(content, objectIDArray[index], deviceInfo);

                    // Free allocated PWSTRs after the recursive enumeration call has completed.
                    CoTaskMemFree(objectIDArray[index]);
                    objectIDArray[index] = NULL;
                }
            }
        }

    } while (false);
}

void MTPInfo::GetDeviceInfoEx(IPortableDeviceContent *content,
                              PCWSTR objectID,
                              DeviceInfo &deviceInfo)
{
    if (!content || !objectID)
    {
        return;
    }

    CComPtr<IPortableDeviceProperties> properties = NULL;
    CComPtr<IPortableDeviceValues> objectProperties = NULL;
    CComPtr<IPortableDeviceKeyCollection> propertiesToRead = NULL;
    PWSTR manufacturer = NULL;
    PWSTR friendlyName = NULL;
    PWSTR parentID = NULL;
    PWSTR protocol = NULL;
    ULONGLONG storageTotalSize = 0;
    ULONGLONG storageAvailableSize = 0;
    HRESULT hr = S_OK;
    bool isDeviceID = !wcscmp(WPD_DEVICE_OBJECT_ID, objectID);

    do 
    {
        // 1) Get an IPortableDeviceProperties interface from the IPortableDeviceContent interface
        // to access the property-specific methods.
        hr = content->Properties(&properties);
        if (FAILED(hr) || !properties)
        {
            DebugLog::LogW(L"! Failed to get IPortableDeviceProperties from IPortableDevice, hr = 0x%lx\r\n", hr);
            break;
        }

        // 2) CoCreate an IPortableDeviceKeyCollection interface to hold the the property keys
        // we wish to read.
        hr = CoCreateInstance(CLSID_PortableDeviceKeyCollection,
            NULL,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&propertiesToRead));
        if (FAILED(hr) || !propertiesToRead)
        {
            break;
        }

        if (isDeviceID)
        {
            hr = propertiesToRead->Add(WPD_DEVICE_MANUFACTURER);
            if (FAILED(hr))
            {
                DebugLog::LogW(L"! Failed to add WPD_DEVICE_MANUFACTURER to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            }

            hr = propertiesToRead->Add(WPD_DEVICE_FRIENDLY_NAME);
            if (FAILED(hr))
            {
                DebugLog::LogW(L"! Failed to add WPD_DEVICE_FRIENDLY_NAME to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            }

            hr = propertiesToRead->Add(WPD_DEVICE_PROTOCOL);
            if (FAILED(hr))
            {
                DebugLog::LogW(L"! Failed to add WPD_DEVICE_PROTOCOL to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            }
        }
        else
        {
            hr = propertiesToRead->Add(WPD_OBJECT_PARENT_ID);
            if (FAILED(hr))
            {
                DebugLog::LogW(L"! Failed to add WPD_OBJECT_PARENT_ID to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            }

            hr = propertiesToRead->Add(WPD_STORAGE_CAPACITY);
            if (FAILED(hr))
            {
                DebugLog::LogW(L"! Failed to add WPD_STORAGE_CAPACITY to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            }

            hr = propertiesToRead->Add(WPD_STORAGE_FREE_SPACE_IN_BYTES);
            if (FAILED(hr))
            {
                DebugLog::LogW(L"! Failed to add WPD_STORAGE_FREE_SPACE_IN_BYTES to IPortableDeviceKeyCollection, hr= 0x%lx\n", hr);
            }
        }
       
        // 4) Call GetValues() passing the collection of specified PROPERTYKEYs.
        hr = properties->GetValues(objectID,                // The object whose properties we are reading
            propertiesToRead,   // The properties we want to read
            &objectProperties);       // Driver supplied property values for the specified object
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to get all properties for object '%s', hr= 0x%lx\n", objectID, hr);
            break;
        }

        if (isDeviceID)
        {
            // Get device manufacturer
            hr = objectProperties->GetStringValue(WPD_DEVICE_MANUFACTURER, &manufacturer);
            if (SUCCEEDED(hr) && manufacturer)
            {
                deviceInfo.manufacturer = manufacturer;
            }

            if (std::wstring::npos != deviceInfo.manufacturer.find(L"Apple Inc."))
            {
                deviceInfo.devType = DEV_TYPE_IOS;
            }

            CoTaskMemFree(manufacturer);
            manufacturer = NULL;

            // Get device friendlyName
            hr = objectProperties->GetStringValue(WPD_DEVICE_FRIENDLY_NAME, &friendlyName);
            if (SUCCEEDED(hr) && friendlyName)
            {
                deviceInfo.friendlyName = friendlyName;
            }

            CoTaskMemFree(friendlyName);
            friendlyName = NULL;

            // Get device protocol
            hr = objectProperties->GetStringValue(WPD_DEVICE_PROTOCOL, &protocol);
            if (SUCCEEDED(hr) && protocol)
            {
                if (wcsstr(protocol, L"MTP") ||
                    wcsstr(protocol, L"PTP"))
                {
                    if (0 == deviceInfo.mode)
                    {
                        deviceInfo.mode = MTP_MODE;
                    }
                    else
                    {
                        deviceInfo.mode |= MTP_MODE;
                    }
                }
                else if (wcsstr(protocol, L"MSC"))
                {
                    if (0 == deviceInfo.mode)
                    {
                        deviceInfo.mode = USB_MODE;
                    }
                    else
                    {
                        deviceInfo.mode |= USB_MODE;
                    }
                }
            }

            CoTaskMemFree(friendlyName);
            friendlyName = NULL;
        }
        else
        {
            // Get object parent Id
            hr = objectProperties->GetStringValue(WPD_OBJECT_PARENT_ID, &parentID);
            if (FAILED(hr) || !parentID)
            {
                DebugLog::LogW(L"! Failed to get object parent ID for object '%s', hr= 0x%lx\n", objectID, hr);
                break;
            }

            // If object is storage or sdcard, get storage total size and available size
            if (0 == wcscmp(WPD_DEVICE_OBJECT_ID, parentID))
            {
                hr = objectProperties->GetUnsignedLargeIntegerValue(WPD_STORAGE_CAPACITY, &storageTotalSize);
                hr = objectProperties->GetUnsignedLargeIntegerValue(WPD_STORAGE_FREE_SPACE_IN_BYTES, &storageAvailableSize);

                if (!wcscmp(L"s10001", objectID) || !wcscmp(L"s10002", objectID))
                {
                    deviceInfo.storageAvailableSize = storageAvailableSize;
                    deviceInfo.storageTotalSize = storageTotalSize;
                }

                if (!wcscmp(L"s20002", objectID) || L':' == objectID[wcslen(objectID) - 1])
                {
                    deviceInfo.SDcardAvailableSize = storageAvailableSize;
                    deviceInfo.SDcardTotalSize = storageTotalSize;
                }
            }

            CoTaskMemFree(parentID);
            parentID = NULL;
        }

    } while (false);
}

// Recursively called function which enumerates using the specified
// object identifier as the parent.
void MTPInfo::RecursiveEnumerateFolder(IPortableDeviceContent *content, PCWSTR objectID)
{
    if (10 < m_layers)
    {
        m_layers = 0;
        return;
    }

    ++m_layers;

    if (m_stopAll || !content || !objectID)
    {
        return;
    }

	HRESULT hr = S_OK;
	CComPtr<IEnumPortableDeviceObjectIDs> enumObjectIDs = NULL;
    DWORD  numFetched = 0;
    PWSTR  objectIDArray[NUM_OBJECTS_TO_REQUEST] = { 0 };
    FolderInfo folderInfo;
    int flag = -1;

    // Get an IEnumPortableDeviceObjectIDs interface by calling EnumObjects with the
    // specified parent object identifier.
    hr = content->EnumObjects(0,   // Flags are unused
        objectID,     // Starting from the passed in object
        NULL,            // Filter is unused
        &enumObjectIDs);
    if (FAILED(hr) || !enumObjectIDs)
    {
        DebugLog::LogW(L"! Failed to get IEnumPortableDeviceObjectIDs from IPortableDeviceContent, hr = 0x%lx\n", hr);
        return;
    }

    // Loop calling Next() while S_OK is being returned.
    while(hr == S_OK)
    {
        numFetched = 0;
        memset(objectIDArray, 0, sizeof(objectIDArray));

        hr = enumObjectIDs->Next(NUM_OBJECTS_TO_REQUEST,    // Number of objects to request on each NEXT call
                                 objectIDArray,             // Array of PWSTR array which will be populated on each NEXT call
                                 &numFetched);              // Number of objects written to the PWSTR array
        if (SUCCEEDED(hr))
        {
            // Traverse the results of the Next() operation and recursively enumerate
            // Remember to free all returned object identifiers using CoTaskMemFree()
            for (DWORD index = 0; (index < numFetched) && (objectIDArray[index] != NULL); ++index)
            {
                ObjectInfo *mtpObjectInfo = new ObjectInfo();
                if (!mtpObjectInfo)
                {
                    break;
                }

                GetObjectInfo(content, objectIDArray[index], mtpObjectInfo);

                if (mtpObjectInfo->isFolder)
                {
                    flag = IsTargetPath(mtpObjectInfo->name);

                    if (WPD_DEVICE_OBJECT_ID == mtpObjectInfo->parentID)
                    {
                        m_mtpObjectInfoMap.insert(std::make_pair(mtpObjectInfo->uniqueID, mtpObjectInfo));

                        RecursiveEnumerateFolder(content, objectIDArray[index]);
                    }
                    else if (1 == flag)
                    {
                        m_mtpObjectInfoMap.insert(std::make_pair(mtpObjectInfo->uniqueID, mtpObjectInfo));

                        RecursiveEnumerateFolder(content, objectIDArray[index]);
                    }
                    else if (0 == flag)
                    {
                        mtpObjectInfo->deviceDir = L"/";
                        mtpObjectInfo->localDir = m_outPutPath;
                        m_mtpObjectInfoMap.insert(std::make_pair(mtpObjectInfo->uniqueID, mtpObjectInfo));

                        folderInfo.ID = mtpObjectInfo->objectID;
                        folderInfo.uniqueID = mtpObjectInfo->uniqueID;

                        m_folderList.push_back(folderInfo);
                    }
                    // 所有目标路径都已找到
                    else if (-1 == flag)
                    {
                        delete mtpObjectInfo;
                        
                        // Free allocated PWSTRs after the recursive enumeration call has completed.
                        CoTaskMemFree(objectIDArray[index]);
                        objectIDArray[index] = NULL;
                        break;
                    }
                    else
                    {
                        delete mtpObjectInfo;
                    }
                }

                // Free allocated PWSTRs after the recursive enumeration call has completed.
                CoTaskMemFree(objectIDArray[index]);
                objectIDArray[index] = NULL;
            }
        }
    }

    if (1 < m_layers)
    {
        --m_layers;
    }
    else
    {
        m_layers = 1;
    }
}

void MTPInfo::RecursiveEnumerateFile(IPortableDeviceContent *content,
                                     const FolderInfo &folderInfo)
{
    if (m_stopAll || 
        !content ||
        !m_cbObjectInfo)
    {
        return;
    }

    CComPtr<IEnumPortableDeviceObjectIDs> enumObjectIDs;
    HRESULT hr = S_OK;
    DWORD  numFetched = 0;
    PWSTR  objectIDArray[NUM_OBJECTS_TO_REQUEST] = { 0 };
    FolderInfo currentFolderInfo;
    bool ret = false;
    std::wstring targetFilePath;
    FILETIME fileTime = {0};
    HANDLE fileHandle = INVALID_HANDLE_VALUE;
    std::wstring tempPath;
    
    // Get an IEnumPortableDeviceObjectIDs interface by calling EnumObjects with the
    // specified parent object identifier.
    hr = content->EnumObjects(0,   // Flags are unused
        folderInfo.ID.c_str(),     // Starting from the passed in object
        NULL,            // Filter is unused
        &enumObjectIDs);
    if (FAILED(hr))
    {
        DebugLog::LogW(L"! Failed to get IEnumPortableDeviceObjectIDs from IPortableDeviceContent, hr = 0x%lx\n", hr);
        return;
    }

    // Loop calling Next() while S_OK is being returned.
    while(hr == S_OK)
    {
        numFetched = 0;
        memset(objectIDArray, 0, sizeof(objectIDArray));

        hr = enumObjectIDs->Next(NUM_OBJECTS_TO_REQUEST,    // Number of objects to request on each NEXT call
                                 objectIDArray,             // Array of PWSTR array which will be populated on each NEXT call
                                 &numFetched);              // Number of objects written to the PWSTR array
        if (SUCCEEDED(hr))
        {
            // Traverse the results of the Next() operation and recursively enumerate
            // Remember to free all returned object identifiers using CoTaskMemFree()
            for (DWORD index = 0; (index < numFetched) && (objectIDArray[index] != NULL); ++index)
            {
                ObjectInfo *mtpObjectInfo = new ObjectInfo();
                if (!mtpObjectInfo)
                {
                    break;
                }

                GetObjectInfo(content, objectIDArray[index], mtpObjectInfo);

                if (!mtpObjectInfo->isFolder)
                {
                    do 
                    {
                        if (0 == mtpObjectInfo->size)
                        {
                            delete mtpObjectInfo;
                            break;
                        }

                        bool result = AcquireObject(content, mtpObjectInfo, targetFilePath);
                        if (!result)
                        {
                            delete mtpObjectInfo;
                            break;
                        }

                        memset(&fileTime, 0, sizeof(FILETIME));
                        fileHandle = INVALID_HANDLE_VALUE;
                        // Set file time to real file time
                        if (!targetFilePath.empty())
                        {
                            fileTime = utils::UnixTimeToFileTime(mtpObjectInfo->lastModifiedTime);
                            fileHandle = CreateFileW(targetFilePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
                            if (INVALID_HANDLE_VALUE != fileHandle)
                            {
                                SetFileTime(fileHandle, NULL, NULL, &fileTime);
                                CloseHandle(fileHandle);
                            }
                        }

                        // 上抛
                        if (m_cbObjectInfo)
                        {
                            m_cbObjectInfo(mtpObjectInfo);
                        }
                        delete mtpObjectInfo;


                    }while (false);
                }
                else if (mtpObjectInfo->isFolder)
                {
                    m_mtpObjectInfoMap.insert(std::make_pair(mtpObjectInfo->uniqueID, mtpObjectInfo));

                    currentFolderInfo.ID = objectIDArray[index];
                    currentFolderInfo.uniqueID = mtpObjectInfo->uniqueID;
                    RecursiveEnumerateFile(content, currentFolderInfo);
                }

                // Free allocated PWSTRs after the recursive enumeration call has completed.
                CoTaskMemFree(objectIDArray[index]);
                objectIDArray[index] = NULL;
            }
        }
    }
}

void MTPInfo::AcquireTargetFolder(const FolderInfo &folderInfo)
{
    CComPtr<IPortableDevice> device;
    CComPtr<IPortableDeviceContent> content;
    HRESULT hr = S_OK;

    if (!OpenDevice(&device) || !device)
    {
        return;
    }

    hr = device->Content(&content);
    if (FAILED(hr) || !content)
    {
        return;
    }

    RecursiveEnumerateFile(content, folderInfo);
}

// Enumerate all content on the device starting with the
// "DEVICE" object
void MTPInfo::EnumerateAllContent()
{
	CComPtr<IPortableDevice> device;
	CComPtr<IPortableDeviceContent> content;
	HRESULT hr = S_OK;
    
	if (!OpenDevice(&device) || !device)
	{
		return;
	}

	hr = device->Content(&content);
	if (FAILED(hr) || !content)
	{
		return;
	}

    // Enumerate content starting from the "DEVICE" object.
    ObjectInfo *mtpObjectInfo = new ObjectInfo();
    if (mtpObjectInfo)
    {
        GetObjectInfo(content, WPD_DEVICE_OBJECT_ID, mtpObjectInfo);

        m_mtpObjectInfoMap.insert(std::make_pair(mtpObjectInfo->uniqueID, mtpObjectInfo));

        m_layers = 1;
        RecursiveEnumerateFolder(content, WPD_DEVICE_OBJECT_ID);
    }

    // 下面开始获取
    for (std::list<FolderInfo>::iterator iter = m_folderList.begin();
        iter != m_folderList.end();
        ++iter)
    {
        AcquireTargetFolder(*iter);
    }
}

// Copies data from a source stream to a destination stream using the
// specified cbTransferSize as the temporary buffer size.
HRESULT MTPInfo::StreamCopy(IStream *pDestStream,
                            IStream *pSourceStream,
                            DWORD cbTransferSize,
                            DWORD *pcbWritten)
{
    if (!pDestStream || !pSourceStream || !pcbWritten)
    {
        return S_FALSE;
    }

    HRESULT hr = S_OK;

    // Allocate a temporary buffer (of Optimal transfer size) for the read results to
    // be written to.
    BYTE *pObjectData = new (std::nothrow) BYTE[cbTransferSize];
    if (pObjectData != NULL)
    {
        DWORD cbTotalBytesRead    = 0;
        DWORD cbTotalBytesWritten = 0;

        DWORD cbBytesRead    = 0;
        DWORD cbBytesWritten = 0;

        // Read until the number of bytes returned from the source stream is 0, or
        // an error occured during transfer.
        do
        {
            if (m_stopAll)
            {
                break;
            }

            // Read object data from the source stream
            hr = pSourceStream->Read(pObjectData, cbTransferSize, &cbBytesRead);
            if (FAILED(hr))
            {
                //DebugLog::LogW(L"! Failed to read %d bytes from the source stream, hr = 0x%lx\n",cbTransferSize, hr);
            }

            // Write object data to the destination stream
            if (SUCCEEDED(hr))
            {
                cbTotalBytesRead += cbBytesRead; // Calculating total bytes read from device for debugging purposes only

                hr = pDestStream->Write(pObjectData, cbBytesRead, &cbBytesWritten);
                if (FAILED(hr))
                {
                    //DebugLog::LogW(L"! Failed to write %d bytes of object data to the destination stream, hr = 0x%lx\n",cbBytesRead, hr);
                }

                if (SUCCEEDED(hr))
                {
                    cbTotalBytesWritten += cbBytesWritten; // Calculating total bytes written to the file for debugging purposes only
                }
            }

            // Output Read/Write operation information only if we have received data and if no error has occured so far.
            if (SUCCEEDED(hr) && (cbBytesRead > 0))
            {
                //DebugLog::LogW(L"Read %d bytes from the source stream...Wrote %d bytes to the destination stream...\n", cbBytesRead, cbBytesWritten);
            }

        } while (SUCCEEDED(hr) && (cbBytesRead > 0));

        // If the caller supplied a pcbWritten parameter and we
        // and we are successful, set it to cbTotalBytesWritten
        // before exiting.
        if ((SUCCEEDED(hr)) && (pcbWritten != NULL))
        {
            *pcbWritten = cbTotalBytesWritten;
        }

        // Remember to delete the temporary transfer buffer
        delete [] pObjectData;
        pObjectData = NULL;
    }
    else
    {
        DebugLog::LogW(L"! Failed to allocate %d bytes for the temporary transfer buffer.\n", cbTransferSize);
    }

    return hr;
}

bool MTPInfo::AcquireObject(IPortableDeviceContent *content,
                            const ObjectInfo *mtpObjectInfo,
                            std::wstring &targetFilePath)
{
    if (!content || !mtpObjectInfo)
    {
        return false;
    }

    HRESULT                                 hr = S_OK;
    CComPtr<IPortableDeviceResources>       resources;
    CComPtr<IStream>                        objectDataStream;
    CComPtr<IStream>                        finalFileStream;
    DWORD                                   totalBytesWritten = 0;
    DWORD                                   optimalTransferSizeBytes = 0;
    bool                                    ret = false;

    do 
    {
        // 1) Get an IPortableDeviceResources interface from the IPortableDeviceContent to
        // access the resource-specific  methods.
        hr = content->Transfer(&resources);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to get IPortableDeviceResources from IPortableDeviceContent, hr = 0x%lx\n", hr);
            break;
        }

        targetFilePath = mtpObjectInfo->localDir + mtpObjectInfo->name + mtpObjectInfo->suffix;

        hr = resources->GetStream(
            mtpObjectInfo->objectID.c_str(), // Identifier of the object we want to transfer
            WPD_RESOURCE_DEFAULT, // We are transferring the default resource (which is the entire object's data)
            STGM_READ, // Opening a stream in READ mode, because we are reading data from the device.
            &optimalTransferSizeBytes, // Driver supplied optimal transfer size
            &objectDataStream);
        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to get object WPD_RESOURCE_DEFAULT from IPortableDeviceResources, hr = 0x%lx\n", hr);
            break;
        }
        
        if (targetFilePath.empty())
        {
            break;
        }

        SHCreateDirectoryExW(NULL, mtpObjectInfo->localDir.c_str(), NULL);
        if (!PathFileExistsW(mtpObjectInfo->localDir.c_str()))
        {
            DebugLog::LogW(L"! Failed to create dir %s\n", mtpObjectInfo->localDir.c_str());
            break;
        }

        // 3) Create a destination for the data to be written to.  In this example we are
        // creating a temporary file which is named the same as the object identifier string.
        hr = SHCreateStreamOnFileEx(targetFilePath.c_str(), STGM_CREATE | STGM_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, NULL, &finalFileStream);
        if (FAILED(hr) || !PathFileExistsW(targetFilePath.c_str()))
        {
            DebugLog::LogW(L"! Failed to create stream on file %s, hr = 0x%lx\n", targetFilePath.c_str(), hr);
            break;
        }

        // 4) Read on the object's data stream and write to the final file's data stream using the
        // driver supplied optimal transfer buffer size.
        // Since we have IStream-compatible interfaces, call our helper function
        // that copies the contents of a source stream into a destination stream.
        hr = StreamCopy(finalFileStream,            // Destination (The Final File to transfer to)
            objectDataStream,           // Source (The Object's data to transfer from)
            optimalTransferSizeBytes,   // The driver specified optimal transfer buffer size
            &totalBytesWritten);        // The total number of bytes transferred from device to the finished file

        if (FAILED(hr))
        {
            DebugLog::LogW(L"! Failed to transfer object '%s', hr = 0x%lx\n", (mtpObjectInfo->deviceDir + mtpObjectInfo->name).c_str(), hr);
            break;
        }

        ret = true;
        break;
    } while (false);

    return ret;
}