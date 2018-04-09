// test_packet.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"

#include "FileTools.h"
#include "DebugTools.h"
#include "ResourceMgr.h"
#include "LzmaHelper.h"

bool AddFile(ResourceMgr& mgr, LzmaHelper& lzma, LPCWSTR wzFileName)
{
	WCHAR wzFilePath[MAX_PATH] = {0};
	FileTools::GetExePath(wzFilePath);
	wcscat(wzFilePath, wzFileName);

	WCHAR wzCompFilePath[MAX_PATH] = {0};
	wsprintf(wzCompFilePath, L"%sc", wzFilePath);

	/*// ѹ��
	// 1.dll -> 1.dllc
	lzma.CompressFile(wzFilePath, wzCompFilePath);

	printf("%ws\n", wzCompFilePath);*/

	if ( mgr.AddFile(wzFilePath) )
	{
		DebugTools::OutputDebugPrintfW(L"Packet %s Success\r\n", wzFilePath);
		Sleep(100);
		return true;
	}
	else
	{
		DebugTools::OutputDebugPrintfW(L"Packet %s Failed\r\n", wzFilePath);
		return false;
	}

	// ɾ����ʱ�ļ�
	//DeleteFile(wzCompFilePath);
}


int _tmain(int argc, _TCHAR* argv[])
{

	if ( 2 != argc )
	{
		return 0;
	}

	WCHAR wzExePath[MAX_PATH] = {0};
	FileTools::GetExePath(wzExePath);
	wcscat(wzExePath, L"\\inject.exe");

	WCHAR wzOutPutPath[MAX_PATH] = {0};
	wsprintf(wzOutPutPath, L"%s", argv[1]);

	CopyFileW(wzExePath, wzOutPutPath, FALSE);

	Sleep(1000);

	DebugTools::OutputDebugPrintfW(L"Src : %s Dst: %s\r\n", wzExePath, wzOutPutPath);

	ResourceMgr mgr(wzOutPutPath);
	LzmaHelper lzma;

	bool bRet = TRUE;

	if ( !AddFile(mgr, lzma, L"\\libiconv.dll") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\libimobiledevice.dll") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\libxml2.dll") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\1.dat") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\2.dat") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\4.dat") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\detect.deb") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\dtl.dat") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\glp.uin") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\plugin.dat") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\mobilesubstrate.deb") )
	{
		goto PACKET_ERROR;
	}

	if ( !AddFile(mgr, lzma, L"\\Plugin\\MobileSafetyDetect.dylib") )
	{
		goto PACKET_ERROR;
	}
	
	if ( mgr.Packet() )
	{
		MessageBoxA(NULL, "�ɹ�����Ŀ���!", "��ʾ", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

PACKET_ERROR:
	MessageBoxA(NULL, "����ʧ�ܣ���ر�ɱ�������!", "����", MB_OK);
	return 0;

}

