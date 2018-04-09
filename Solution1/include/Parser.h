#pragma once
#include <QQ&WeixinCommon.h>


struct AccountInfo
{
	int avatarType;

	std::string		account;
    std::string     weixinNumber;
    std::string     nickName;
    std::wstring	dbPath;
    std::wstring	firentDbPath;
	std::string		key;		 //android qq:imei  android weixin:the key to decrypt db

	std::list<FriendInfo>		friendList;
	std::list<GroupInfo>		groupList;
	std::list<TroopInfo>		troopList;
	std::list<DiscInfo>			discussList;
	std::map<std::string,ChatInfo>  chatMap;

	std::list<FriendInfo>::iterator	 friendItera;
	std::list<GroupInfo>::iterator	 groupItera;
	std::list<TroopInfo>::iterator   troopItera;
	std::list<DiscInfo>::iterator    discussItera;

    std::list<FriendInfo>		recoveryFriendList;     // 已删除好友
    std::map<std::string, ChatInfo>  recoveryChatMap;   // 已删除聊天记录
}; 

/*
 *  查找解压之后 数据所在目录
*/
inline void FindDataPath(const wstring& dirPath, const wstring& dataName, wstring& dataPath)
{
	WIN32_FIND_DATAW findFileData;
	wstring srcFilePath(dirPath);
	srcFilePath.append(L"\\*.*");

	dataPath.clear();
	HANDLE fileHandle = ::FindFirstFileW(srcFilePath.c_str(), &findFileData);
	if (INVALID_HANDLE_VALUE != fileHandle)
	{
		while(true)
		{
			if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) 
			{
                if (_wcsicmp(L".", findFileData.cFileName) && _wcsicmp(L"..", findFileData.cFileName))
                {
                    wstring fileName = findFileData.cFileName;
                    wstring filePath = dirPath + L"\\" + fileName;
                    if (0 == fileName.compare(dataName))
                    {
                        dataPath = filePath;
                        break;
                    }
                    else
                    {
                        FindDataPath(filePath, dataName, dataPath);
                    }
                }
			}

			if (!FindNextFileW(fileHandle, &findFileData))
				break;
		}
		FindClose(fileHandle);
	}
}

//iType: 0-.tar.gz  1-.zip 
//
inline bool UnzipAndMove(const std::wstring &filepath, const std::wstring &dstPath, const std::wstring &appId, int iType)
{
	std::wstring   upperFile;
	std::wstring   dataPath;
	std::wstring   cmd;
	std::wstring   delPath;
	std::wstring   dirPath;
	bool   bRet = false;

	WCHAR  wzSrcPath[MAX_PATH] = {0};
	WCHAR  wzDstPath[MAX_PATH] = {0};
	WCHAR  wzDelPath[MAX_PATH] = {0};
	WCHAR  wzCmd[1024] = {0};

	STARTUPINFO		startupInfo = {sizeof(STARTUPINFO)};
	PROCESS_INFORMATION processInfo = {0};

	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_HIDE;

	do 
	{
		if (0 == iType)
		{
			dirPath  = filepath.substr(0, filepath.rfind(L'\\'));
			upperFile = filepath.substr(0, filepath.rfind(L'.'));
			cmd = L"7z.exe x \"" + filepath + L"\" -o\"" + dirPath + L"\" -y";
			//_wsystem(cmd.c_str());
			//ShellExecuteW(NULL, L"open", L"7z.exe", cmd.c_str(), NULL, SW_HIDE);
			//先解压为xxx.tar.gz->xxx.tar
			StringCbCopyW(wzCmd, sizeof(wzCmd), cmd.c_str());
			if (!CreateProcess(NULL,
				wzCmd,
				NULL,
				NULL,
				false,
				CREATE_NEW_CONSOLE, 
				NULL,
				NULL,
				&startupInfo,
				&processInfo))
			{
				break;
			}
			WaitForSingleObject(processInfo.hProcess, INFINITE);
			CloseHandle(processInfo.hThread);
			CloseHandle(processInfo.hProcess);

		}
		else
		{
			upperFile = filepath;
		}

		//type 0 xxx.tar   !0 xxx.zip?
		if (PathFileExistsW(upperFile.c_str()))
		{
			cmd = L"7z.exe x \"" + upperFile + L"\" -o\"" + dstPath + L"\" -y";
            StringCbCopyW(wzCmd, sizeof(wzCmd), cmd.c_str());
            if (!CreateProcess(NULL,
				wzCmd,
				NULL,
				NULL,
				false,
				CREATE_NEW_CONSOLE, 
				NULL,
				NULL,
				&startupInfo,
				&processInfo))
			{
				break;
			}
			WaitForSingleObject(processInfo.hProcess, INFINITE);
			CloseHandle(processInfo.hThread);
			CloseHandle(processInfo.hProcess);
		}

		if (0 == iType)
		{
			//tar -cf 压缩与 tar -zcf解压路径不同
			std::wstring tmpDstPath = dstPath;
			if (!PathFileExistsW(dstPath.c_str()))
			{
				tmpDstPath = dstPath.substr(0, dstPath.rfind('\\'));
			}

			dataPath = tmpDstPath + L"\\data\\data\\" + appId;
			if (!PathFileExistsW(dataPath.c_str()))
			{
				FindDataPath(tmpDstPath, appId, dataPath);
                if (dataPath.empty() || !PathFileExistsW(dataPath.c_str()))
                {
                    dataPath = tmpDstPath + L"\\data\\user\\0\\" + appId;
                    FindDataPath(tmpDstPath, appId, dataPath);
                }
			}
			delPath  = tmpDstPath + L"\\data";
		}
		else
		{
			dataPath = dstPath + L"\\Container";
			delPath  = dstPath + L"\\Container";
		}

        if (dstPath.empty() || dataPath.empty())
        {
            break;
        }

		std::wstring  fulPath;
		SHFILEOPSTRUCTW  fileOp = {0};

		//move files and dirs
		//
		fileOp.wFunc = FO_MOVE;
		StringCbPrintfW(wzDstPath, sizeof(wzDstPath), L"%s", dstPath.c_str());
		StringCbPrintfW(wzSrcPath, sizeof(wzSrcPath), L"%s\\*", dataPath.c_str());
		fileOp.pFrom = wzSrcPath;
		fileOp.pTo = wzDstPath;
		fileOp.fFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION;
		int iRes = SHFileOperationW(&fileOp);

		if (!PathFileExistsW(dstPath.c_str()))
		{
			break;
		}

		if (/*iRes == 0*/0)
		{
			//delete tmp dir
			//
			fileOp.wFunc = FO_DELETE;
			StringCbPrintfW(wzDelPath, sizeof(wzDelPath), L"%s", delPath.c_str());
			fileOp.pFrom = wzDelPath;
			fileOp.pTo = NULL;
			SHFileOperationW(&fileOp);
		}

		bRet = true;
	} while (false);

	if (0 == iType && PathFileExistsW(upperFile.c_str()))
	{
		DeleteFileW(upperFile.c_str());
	}
	return bRet;
}

inline bool ExtractAbFile(const std::wstring &abFile, const std::wstring &dstPath, const std::wstring appId)
{
	std::wstring	cmd;
	std::wstring	tarFile;
	bool			bRet = false;
	WCHAR	wzCmd[1024] = {0};

	STARTUPINFO		startupInfo = {sizeof(STARTUPINFO)};
	PROCESS_INFORMATION processInfo = {0};

	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_HIDE;

	do 
	{
		tarFile = abFile.substr(0, abFile.rfind(L'.'));
		tarFile += L".tar";

		cmd = L"java -jar abe.jar unpack \"" + abFile + L"\" \"" + tarFile + L"\"";
		//ShellExecuteW(NULL, L"open", L"cmd.exe", cmd.c_str(), NULL, SW_HIDE);
		//_wsystem(cmd.c_str());
        StringCbCopyW(wzCmd, sizeof(wzCmd), cmd.c_str());
        if (!CreateProcess(NULL,
			wzCmd,
			NULL,
			NULL,
			false,
			CREATE_NEW_CONSOLE, 
			NULL,
			NULL,
			&startupInfo,
			&processInfo))
		{
			break;
		}
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		if (!PathFileExistsW(tarFile.c_str()))
		{
			break;
		}

		cmd = L"7z.exe x \"" + tarFile + L"\" -o\"" + dstPath + L"\"";
		//ShellExecuteW(NULL, L"open", L"7z.exe", cmd.c_str(), NULL, SW_HIDE);
		//_wsystem(cmd.c_str());
        StringCbCopyW(wzCmd, sizeof(wzCmd), cmd.c_str());
        if (!CreateProcess(NULL,
			wzCmd,
			NULL,
			NULL,
			false,
			CREATE_NEW_CONSOLE, 
			NULL,
			NULL,
			&startupInfo,
			&processInfo))
		{
			break;
		}
		WaitForSingleObject(processInfo.hProcess, INFINITE);
		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		if (!PathFileExistsW(dstPath.c_str()))
		{
			break;
		}

		WIN32_FIND_DATAW	w32_Find = {0};
		WCHAR				wzDstPath[MAX_PATH] = {0};
		WCHAR				wzSrcPath[MAX_PATH] = {0};
		SHFILEOPSTRUCT		fileOp = {0};
		std::wstring		findStr = dstPath + L"\\apps\\" + appId + L"\\*";

		//move files and dirs 
		//
		StringCbPrintfW(wzDstPath, sizeof(wzDstPath), L"%s", dstPath.c_str());
		StringCbPrintfW(wzSrcPath, sizeof(wzSrcPath), L"%s\\apps\\%s\\*", dstPath.c_str(), appId.c_str());

		fileOp.wFunc = FO_MOVE;
		fileOp.fFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION;
		fileOp.pFrom = wzSrcPath;
		fileOp.pTo = wzDstPath;
		SHFileOperationW(&fileOp);

		memset(wzSrcPath, 0, sizeof(wzSrcPath));
		memset(wzDstPath, 0, sizeof(wzDstPath));
		StringCbPrintfW(wzDstPath, sizeof(wzDstPath), L"%s", dstPath.c_str());
		StringCbPrintfW(wzSrcPath, sizeof(wzSrcPath), L"%s\\r\\*", dstPath.c_str());
		fileOp.wFunc = FO_MOVE;
		fileOp.fFlags = FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR | FOF_NOCONFIRMATION;
		fileOp.pFrom = wzSrcPath;
		fileOp.pTo = wzDstPath;
		SHFileOperationW(&fileOp);

		//delete tmp dir
		//
		memset(wzSrcPath, 0, sizeof(wzSrcPath));
		StringCbPrintfW(wzSrcPath, sizeof(wzSrcPath), L"%s\\apps", dstPath.c_str());
		fileOp.wFunc = FO_DELETE;
		fileOp.pFrom = wzSrcPath;
		SHFileOperationW(&fileOp);

		memset(wzSrcPath, 0, sizeof(wzSrcPath));
		StringCbPrintfW(wzSrcPath, sizeof(wzSrcPath), L"%s\\r", dstPath.c_str());
		fileOp.wFunc = FO_DELETE;
		fileOp.pFrom = wzSrcPath;
		SHFileOperationW(&fileOp);

		//rename dir
		//
		fileOp.wFunc = FO_RENAME;

		memset(wzSrcPath, 0, sizeof(wzSrcPath));
		memset(wzDstPath, 0, sizeof(wzDstPath));
		StringCbPrintfW(wzSrcPath, sizeof(wzSrcPath), L"%s\\db", dstPath.c_str());
		StringCbPrintfW(wzDstPath, sizeof(wzDstPath), L"%s\\databases", dstPath.c_str());
		SHFileOperationW(&fileOp);
		if (!PathFileExistsW(wzDstPath))
		{
			break;
		}

		memset(wzSrcPath, 0, sizeof(wzSrcPath));
		memset(wzDstPath, 0, sizeof(wzDstPath));
		StringCbPrintfW(wzSrcPath, sizeof(wzSrcPath), L"%s\\f", dstPath.c_str());
		StringCbPrintfW(wzDstPath, sizeof(wzDstPath), L"%s\\files", dstPath.c_str());
		SHFileOperationW(&fileOp);
		if (!PathFileExistsW(wzDstPath))
		{
			break;
		}

		memset(wzSrcPath, 0, sizeof(wzSrcPath));
		memset(wzDstPath, 0, sizeof(wzDstPath));
		StringCbPrintfW(wzSrcPath, sizeof(wzSrcPath), L"%s\\sp", dstPath.c_str());
		StringCbPrintfW(wzDstPath, sizeof(wzDstPath), L"%s\\shared_prefs", dstPath.c_str());
		SHFileOperationW(&fileOp);
		if (!PathFileExistsW(wzDstPath))
		{
			break;
		}

		bRet = true;
	} while (false);

	if (PathFileExistsW(tarFile.c_str()))
	{
		DeleteFileW(tarFile.c_str());
	}
	return bRet;
}
