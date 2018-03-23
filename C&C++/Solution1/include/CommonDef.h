#ifndef COMMON_DEF_H
#define COMMON_DEF_H

#include <Windows.h>
#include <plist/plist.h>

#define GUID_USB		{0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}}   //����USB��Ϣ
#define GUID_ANDROID    {0xF72FE0D4, 0xCBCB, 0x407d, {0x88, 0x14, 0x9E, 0xD6, 0x73, 0xD0, 0xDD, 0x6B}}   //������׿�豸��Ϣ


//�ϴζϵ�״̬
#define BREAK_POINT_APP     1
#define BREAK_POINT_BACKUP  2
#define BREAK_POINT_IMAGE   4

//ϵͳӦ�ö�Ӧ��־λ
//
#define SYSTEM_APP_SMS			0X00000001	//����
#define SYSTEM_APP_CONTACT		0X00000002  //ͨѶ¼
#define SYSTEM_APP_CALENDAR		0X00000004	//����
#define SYSTEM_APP_CALLHISTORY	0X00000008	//ͨ����¼
#define SYSTEM_APP_EXPLORER		0X00000010	//�����
#define SYSTEM_APP_NOTES		0X00000020	//����¼
#define SYSTEM_APP_RECORDINGS	0X00000040	//��������¼
#define SYSTEM_APP_MAIL         0X00000080	//�ʼ�
#define SYSTEM_APP_MAP			0X00000100	//��ͼ
#define SYSTEM_APP_KCHAIN       0X00000200	//������Ϣ
#define SYSTEM_APP_SETTINGS		0X00000400	//����
#define SYSTEM_APP_PHOTOS		0X00000800	//��Ƭ
#define SYSTEM_APP_KEYBOARD		0X00001000	//���̼�¼
#define SYSTEM_APP_RECORD       0X00002000	//¼����
#define SYSTEM_APP_CAMERA       0X00004000	//���
#define SYSTEM_APP_WIFI         0X00008000	//WIFI
#define SYSTEM_APP_BLUETOOTH    0X00010000	//����
#define SYSTEM_APP_DOWNLOAD     0X00020000	//���ع���

#define LARGE_SIZE			1024 * 1024 * 1024
#define UNKNOWN_SIZE        -1
#define IMAGE_BLK_SIZE		1048576

#define INTERNAL_TOOLS_PATH		L""				//����ʹ�õ��������ߵ����·��
#define IMAGE_ADBTOOLS_PATH		    L"BackUpAdb_8K\\"			//����adb·��
#define BACKUP_ADBTOOLS_PATH_7K		L"BackUpAdb_7K\\"			//����ȡ֤adb·��
#define BACKUP_ADBTOOLS_PATH_8K		L"BackUpAdb_8K\\"			//����ȡ֤adb·��
#define MAX_DEV_COUNT			1				//����豸������
#define BACKUP_PASS				"1"				//��������
#define MAX_ADB_OUTPUT			10240			//adb�����󳤶�
#define MAX_EXT_FILTER_LEN		10240			//��׺�����˳���

#define MAX_PACKET_SIZE	4194304		//����ȡ֤ÿ�η��͵����ݰ�����С
#define MAX_BLK_SIZE    16384		//����ȡ֤ÿ�η��͵����ݿ������С
#define SERVER_PORT		8080		//����ȡ֤����˶˿�
#define MAX_CONNECTION  50			//����ȡ֤��������������

#define MAX_FILE_PATH       512				
#define MAX_DEV_LEN			100
#define FILE_READ_SIZE		1024 * 1024 * 10
 
//
//Ӳ��ʹ�����
typedef struct _DISK_USAGE
{
	UINT64   ui64SysUsed;   //ϵͳ��������
	UINT64	 ui64SysTtl;    //ϵͳ��������
	UINT64	 ui64UserUsed;  //�û���������
	UINT64   ui64UserTtl;   //�û���������
}DISK_USAGE, *LPDISK_USAGE;

typedef struct _LAST_STATUS
{
	BOOL	  bPassRemain;              //IOS������������
	int		  iLastStaus;               //�ϴ�ȡ֤ģʽ
	char	  szRemainArc[MAX_DEV_LEN]; //�鵵�����ļ�
	int		  iAppMode;                 //����ȡ֤ģʽ������ȡ֤���鵵ȡ֤
}LAST_STATUS, *LPLAST_STATUS;

typedef struct _RootToolInfo 
{
	WCHAR wzName[100];
	WCHAR wzPath[MAX_PATH];
}RootToolInfo, *LPRootToolInfo;

enum DEV_TYPE
{
	DEV_TYPE_IOS = 1,
	DEV_TYPE_ANDROID
};

typedef struct _BASE_DEV_INFO 
{
	DEV_TYPE	devType;                        //�豸���ͣ�IOS,ANDROID
	char		szDevId[MAX_DEV_LEN];			//ios:udid  android:adb devices�г��ı�ʶ
	char		szDevName[MAX_DEV_LEN];			//�豸����
	char		szDevModel[MAX_DEV_LEN];        //�豸�ͺ�
	char		szNumber[MAX_DEV_LEN];          //�豸����
	char		szOSVersion[MAX_DEV_LEN];       //ϵͳ�汾
	char		szSerialNum[MAX_DEV_LEN];       //�豸IMEI
	BOOL		bJailBroken;					//�Ƿ�Խ������ROOT
	char		szCpuArc[MAX_DEV_LEN];          //cpu�ܹ�
	DISK_USAGE	diskUsage;                      //�洢ʹ�����
}BASE_DEV_INFO, *LPBASE_DEV_INFO;

//ios�豸��Ϣ
//
typedef struct _IOS_DEV_INFO
{
	char				szColor[MAX_DEV_LEN];     //�豸��ɫ
	BOOL				bBackupPass;              //�Ƿ��б�������
}IOS_DEV_INFO, *LPIOS_DEV_INFO;


//��׿�豸��Ϣ
//
typedef struct _ANDROID_DEV_INFO
{
	char				szManu[MAX_DEV_LEN];	  //���쳧��
	char				szRamSize[MAX_DEV_LEN];   //�ڴ��С
	char				szPixel[MAX_DEV_LEN];     //��Ļ�ֱ��� ���磺1920 * 1080�����޸�Ϊ��ʾ��׿�豸���к�
	char                szSDPath0[MAX_FILE_PATH];
	char                szSDPath1[MAX_FILE_PATH];
	char                szBackPath[MAX_FILE_PATH];

	char                szPhoto[MAX_FILE_PATH];//���
	char                szRecord[MAX_FILE_PATH];//¼��
	char                szBluetooth[MAX_FILE_PATH];//����
	char                szDownload[MAX_FILE_PATH];//���ع���
}ANDROD_DEV_INFO, *LPANDROID_DEV_INFO;

typedef  struct DEV_INFO
{
	BASE_DEV_INFO	baseDevInfo;
	union
	{
		IOS_DEV_INFO	iosDevInfo;
		ANDROD_DEV_INFO androidDevInfo;
	}unDevInfo;
}DEV_INFO, *LPDEV_INFO;

//������Ϣ
//
typedef struct _CASE_INFO 
{
	WCHAR wzDirPath[MAX_PATH];       //����·��

	char  szName[MAX_DEV_LEN];       //��������
	char  szDevModel[MAX_DEV_LEN];   //�豸�ͺ�
	char  szAddress[MAX_DEV_LEN];    //ȡ֤�ص�
	char  szTime[MAX_DEV_LEN];       //ȡ֤ʱ��
	char  szDevNum[MAX_DEV_LEN];     //�豸����
	char  szId[MAX_DEV_LEN];         //���֤��
	char  szRemark[MAX_DEV_LEN];     //��ע
}CASE_INFO, *LPCASE_INFO;

//
//������Ӧ����Ϣ
#define APP_UNFIN      0
#define APP_BIN_START  1
#define APP_BIN_FIN    2
#define APP_DATA_START 3
#define APP_DATA_FIN   4
#define APP_EXTERNAL_START 5
#define APP_EXTERNAL_FIN   6

#define APP_BIN_UNFIN          (-1)//apkδ��ȡ
#define APP_DATA_UNFIN         (-2)//Ӧ�ð�δ��ȡ
#define APP_EXTERNAL_UNFIN     (-3)//����δ��ȡ��ȫ

// add in 2017.09.26
// ���ӡ������ݡ�״̬
#define APP_NO_DATA (-99) // ������

//
//Ӧ�ø�������
#define APP_EXTERNALPATHS_CNT 50

typedef struct _ROLLBACK_APP_INFO
{
    char		szAppID[50];				//Ӧ��ID
    char		szAppName[30];				//Ӧ������
    char		szAppVersion[30];			//Ӧ�ð汾
    char		szAppDir[MAX_DEV_LEN * 2];  //Ӧ��·��
    int         iErrorCode;
}ROLLBACK_APP_INFO, *LPROLLBACK_APP_INFO;

enum PRIORITY{
    PRIORITY_SYSTEMAPP,
    PRIORITY_COMMONAPP,
    PRIORITY_OTHERAPP,
    PRIORITY_FILETYPE,
    PRIORITY_BACKUP,
    PRIORITY_BACKUPAPP,         //�Դ�����
    PRIORITY_ATTACHMENT,
    PRIORITY_ROLLBACKAPP
};

typedef struct _APP_INFO 
{
	int			iPriority;           //���ȼ��� 0��ϵͳӦ�� 1����ҪӦ�� 2������Ӧ�� 3:�ļ����� 4:���� 
	int			iSubPriority;        //����ϵͳӦ�ã���ϵͳӦ�ñ�־λ��������ҪӦ�ã�����ҪӦ�õ������ȼ�
	bool        bSelected;           //�ѹ�ѡ
	int			iState;				 //0-δ��ɣ�1-Ӧ�ó�������ɣ� 2-���������

	char		szAppID[50];				//Ӧ��ID
	char		szAppName[30];				//Ӧ������
	char		szAppVersion[30];			//Ӧ�ð汾
	char		szMain[MAX_DEV_LEN];        //�������
	char		szDataDir[MAX_DEV_LEN];		//����·��
	bool        bPackageGet;
	char		szAppDir[MAX_DEV_LEN * 2];  //Ӧ��·��
	bool        bApkGet;
	char        szAccount[50];				//�����ʺ�
	char		szBuyDate[30];				//��������
	INT64		i64DocSize;					//�ĵ���С
	INT64		i64AppSize;					//Ӧ�ô�С

	char		*lpszIconData;              //ͼ������
	UINT64		u64IconSize;                //ͼ�����ݴ�С

	INT64		i64ExternalSize;			//�����ļ���С
	char        pszExternalPaths[APP_EXTERNALPATHS_CNT][MAX_PATH];  //��ظ�����·��(���ܴ��ڶ��·��)
	bool        bExternalsGet[APP_EXTERNALPATHS_CNT];//ÿ��������Ӧ��״̬
	bool        bAllExternalGet;//���и����Ƿ��ȡ���
}APP_INFO, *LPAPP_INFO;

//
//�ļ���������
typedef struct _APP_COPY_FILTER
{
	bool		bCopyApp;           //����Ӧ��
	bool        bLargeFileFilter;   //���ļ���С����
	UINT64		u64LargeSize;       //����ȡ�ļ�
	bool        bExtFilter;         //����չ�����͹���
	char		szExt[MAX_EXT_FILTER_LEN]; //��չ�����͡����磺*.txt;*.doc;
}APP_COPY_FILTER, *LPAPP_COPY_FILTER;

//
//�ļ���Ϣ
typedef struct _FILE_INFO   
{
	char	 szAppID[MAX_DEV_LEN];			//�ļ�����Ӧ��ID
	char	 szAppName[MAX_DEV_LEN];		//�ļ�����Ӧ������
	char	 szFilePath[MAX_FILE_PATH];		//�ļ�·��
	WCHAR	 wzDstFile[MAX_FILE_PATH];		//Ŀ���ļ�·��
	int		 iPermission;					//��׿�ļ�����Ȩ��
	UINT64   u64Size;						//�ļ���С
	time_t   modifyTime;					//�ļ�����޸�����
    WIN32_FIND_DATAW  fileData;             //��ϸ��Ϣ
}FILE_INFO, *LPFILE_INFO;


//
//���˳����ļ���Ϣ
typedef struct _FILTERED_FILE_IFNO 
{
	FILE_INFO		fileInfo;       //�ļ�������Ϣ     
	int				iType;			//0��ʾ���ļ���1��ʾ��չ���ļ�
	int				iRow;           //���ļ��ڽ���tablewidget�ж�Ӧ������
	bool			bSelected;      //�Ƿ�ѡ
	bool			bComplete;      //�Ƿ��ȡ���
}FILTERED_FILE_IFNO, *LPFILTERED_FILE_IFNO;

//
//���ݺ�׺���˳���SD���ļ���Ϣ
typedef	struct _STORAGE_FILE_INFO
{
	char szExt[8];          //��׺
	char szPath[MAX_PATH];  //ȫ·��
	INT64 i64Size;          //��С;
	time_t time;            //����޸�ʱ��
}STORAGE_FILE_INFO, *PSTORAGE_FILE_INFO;


//
//������Ϣ
#define IMAGE_NOT_SELECTED  0   //δѡ��
#define IMAGE_FINISHED		1   //�����
#define IMAGE_BREAKPOINT    2	//�ϵ�
#define IMAGE_NOT_START     3	//ѡ��δ��ʼ
#define IMAGE_ERROR         -1  //ʧ��

typedef struct _VOLUME_INFO
{
	char	szDirPath[MAX_DEV_LEN];   //Ŀ¼·��
	char	szDevPath[MAX_DEV_LEN];   //�豸·��
	char	szType[MAX_DEV_LEN];	  //�ļ�ϵͳ����
	UINT64	u64TtlSize;				  //�����ܴ�С
	UINT64  u64UsedSize;			  //������ʹ�ô�С

	int     iState;                   //����״̬
	int     iRow;
	WCHAR	wzImgPath[MAX_FILE_PATH]; //�����ļ�·��
}VOLUME_INFO, *LPVOLUME_INFO;


//
//�ص�����

//�����豸������Ϣ�������ơ��ͺŵ�
typedef void (WINAPI *LPFNDISPLAYBASEINFO)(DEV_INFO devInfo);

//����Ӧ�û�ȡ״̬
typedef void(WINAPI *LPFNUPDATEACQUIRESTATE)(LPWSTR lpwzCurApp, DWORD dwType);

//����ϵͳӦ����Ϣ��������
typedef void (WINAPI *LPFNADDSYSAPP)(APP_INFO appInfo, char* iconPath);

//���ݵ�����Ӧ����Ϣ��������
typedef void (WINAPI *LPFNADDTHIRDAPP)(APP_INFO appInfo);

//���¹鵵����״̬
typedef void (WINAPI *LPFNSETARCREMAIN)(char *lpszAppId);

//���¹鵵����
typedef void (*LPFNUPDATEARCSTATUS) (const char *operation, plist_t status, void *user_data);

//����ȡ֤�ص� 
typedef void (WINAPI *LPFNUPDATEAMOUNT)(UINT64);
typedef void (WINAPI *LPFNADDFILTEREDFILE)(FILTERED_FILE_IFNO);
typedef void (WINAPI *LPFNADDCOPYFILE)(FILE_INFO);
typedef void (WINAPI *LPFNADDFAILFILE)(FILE_INFO);
typedef void (WINAPI *LPFNUPDATERATE)(double dRate);

//�ش���������SD���ļ�
typedef void (WINAPI *LPFNUADDSEARCHEDSDFILE)(STORAGE_FILE_INFO storageFileInfo);

//�ش�����
typedef void (WINAPI *LPFUNCADDEXTERNALINFO)(const char* szAppId, const char* szPath, INT64 i64Size);

typedef struct _APP_CALLBACK
{
	LPFNUPDATEAMOUNT	lpfnUpdateAmount;        //���¿���������
	LPFNADDFILTEREDFILE lpfnAddFilterFile;       //���ֹ����ļ�
	LPFNADDCOPYFILE		lpfnAddCopyFile;         //���ڿ������ļ�
	LPFNADDFAILFILE		lpfnAddFailFile;         //�ļ�����ʧ��
	LPFNUPDATEARCSTATUS lpfnUpdateArcState;      //���¹鵵״̬

	LPFNUADDSEARCHEDSDFILE lpfnAddSearchedSDFile; //�ش���������sd���ļ���Ϣ
	LPFUNCADDEXTERNALINFO lpfnAddExternalInfo;//�ش�������Ϣ(����ȡ֤֮����Ҫ��ȡ)
}APP_CALLBACK, *LPAPP_CALLBACK;


//�����豸������Ϣ
typedef void (WINAPI *LPFNADDVOLUME)(VOLUME_INFO);
typedef void (WINAPI *LPFNSETPROCESSHANDLE)(HANDLE hProcess);


//linux��ĳЩ�ַ���windows�ļ����в��ܳ��֣��޸��滻
//
inline void AdjustString(LPWSTR lpwzString)
{
	WCHAR  wNotSppChar[] = {L':', L'*', L'?', L'\"', L'<', L'>', L'|'};
	WCHAR  wSppChar[]    = {L'��', L'��', L'��', L'��', L'��', L'��', L'l'};

	if (lpwzString[0] == L'.')
	{
		lpwzString[0] = L'��';
	}

	if (lpwzString[lstrlenW(lpwzString) - 1] == L'.')
	{
		lpwzString[lstrlenW(lpwzString) - 1] = L'��';
	}

	for (int i=0; i<lstrlenW(lpwzString); i++)
	{
		for (int j=0; j<sizeof(wNotSppChar)/sizeof(WCHAR); j++)
		{
			if (lpwzString[i] == wNotSppChar[j])
			{
				lpwzString[i] = wSppChar[j];
				break;
			}
		}
	}
}

//��64λ�ļ���СΪ�ַ�����ʽ    10240 ===>>  10.0 KB
//
inline void ConvertSizeToString(UINT64 u64Size, LPWSTR lpwzString)
{
	double              dSize;

	if(0 == u64Size)
	{
		swprintf(lpwzString, L"%d B", u64Size);
	}
	else if (u64Size < 1024)
	{
		swprintf(lpwzString, L"%d B", u64Size);
	}
	else if (u64Size < 1024 * 1024)
	{
		dSize = (double)u64Size / 1024;
		swprintf(lpwzString, L"%.1f KB", dSize);
	}
	else if (u64Size < 1024 * 1024 * 1024)
	{
		dSize = (double)u64Size / (1024 * 1024);
		swprintf(lpwzString, L"%.1f MB", dSize);
	}
	else
	{
		dSize = (double)u64Size / (1024 * 1024 * 1024);
		swprintf(lpwzString, L"%.1f GB", dSize);
	}
}

#endif