#include <QFile>

// Qt5
#include <QtWin>

#include "GetFileDetail.h"


BOOL GetFileDetail::FindIconFromCache(const QString &strKey, QIcon &icon)
{
	QPixmap pixMap;

	if(QPixmapCache::find(strKey, &pixMap))
	{
		icon.addPixmap(pixMap);
		return TRUE;
	}

	return FALSE;
}

BOOL GetFileDetail::InsertIconToCache(const QString &strKey, QIcon icon)
{
	return QPixmapCache::insert(strKey, icon.pixmap(QSize(32, 32)));
}

//根据文件名获取文件缩略图
QIcon GetFileDetail::GetFileIconFromFileName(const QString &strFileName)
{
	QIcon file_icon(":/Images/file.png");
	QString strSuffix;//eg. .txt .png
	int nIndex = -1;

	do 
	{
		if (strFileName.isEmpty())
		{
			break;
		}

		nIndex = strFileName.lastIndexOf('.');
		if (-1 == nIndex)
		{
			break;
		}

		if (strFileName.endsWith("tar.gz"))
		{
			strSuffix = ".tar.gz";
		}
		else
		{
			strSuffix = strFileName.mid(nIndex);
		}

		if (strSuffix.isEmpty())
		{
			break;
		}

		if(FindIconFromCache(strSuffix, file_icon))
		{
			break;
		}

		SHFILEINFOW info = {0};
		if(SHGetFileInfoW(strSuffix.utf16(),
			FILE_ATTRIBUTE_NORMAL,
			&info,
			sizeof(info),
			SHGFI_SYSICONINDEX | SHGFI_ICON | SHGFI_USEFILEATTRIBUTES))
		{
			HICON icon = info.hIcon;
			if (icon)
			{
                file_icon = QIcon(QtWin::fromHICON(icon));

                // Qt4
                //file_icon = QIcon(QPixmap::fromWinHICON(icon));
				InsertIconToCache(strSuffix, file_icon);
			}
			DestroyIcon(icon);
		}

	} while (false);

	return file_icon;
}

//从dll或exe中获取文件图标
//eg. d:\Program Files\Sublime Text 3103 x64\sublime_text.exe,0
QIcon GetFileDetail::GetFileIconFromResource(const QString &strFileName)
{
	QIcon file_icon(":/Images/file.png");
	
	do 
	{
		if (strFileName.isEmpty() || !QFile::exists(strFileName))
		{
			break;
		}

		int nIndexTemp = strFileName.lastIndexOf(',');
		if (-1 == nIndexTemp)
		{
			if(FindIconFromCache(strFileName, file_icon))
			{
				break;
			}

			SHFILEINFOW info = {0};
			if(SHGetFileInfoW(strFileName.utf16(),
				FILE_ATTRIBUTE_NORMAL,
				&info,
				sizeof(info),
				SHGFI_SYSICONINDEX | SHGFI_ICON | SHGFI_USEFILEATTRIBUTES))
			{
				HICON icon = info.hIcon;
				if (icon)
				{
                    file_icon = QIcon(QtWin::fromHICON(icon));

                    // Qt4
                    //file_icon = QIcon(QPixmap::fromWinHICON(icon));
					InsertIconToCache(strFileName, file_icon);
				}
				DestroyIcon(icon);
			}

			break;
		}

		QString strResourceFile = QString("");
		int nIndex = -1;
		strResourceFile = strFileName.left(nIndexTemp - 1);
		nIndex = strFileName.right(strFileName.length() - nIndexTemp).toInt();
		if (strResourceFile.isEmpty() || (-1 == nIndex))
		{
			break;
		}

		if(FindIconFromCache(strResourceFile, file_icon))
		{
			break;
		}

		HICON icon = ExtractIconW(NULL, strResourceFile.utf16(), nIndex);
		if (!icon)
		{
            file_icon = QIcon(QtWin::fromHICON(icon));
            
            // Qt4
            //file_icon = QIcon(QPixmap::fromWinHICON(icon));
			InsertIconToCache(strResourceFile, file_icon);
		}
		DestroyIcon(icon);

	} while (false);
	
	return file_icon;
}

//根据文件名获取文件属性描述
//eg. GetFileDetail.cpp--CPP文件  CustomCheckUI.ui--UI文件
QString GetFileDetail::GetFileType(const QString &strFileName)
{
	QString file_type = QString::fromUtf16(L"文件");
	QString strSuffix;//eg. .txt .png
	int nIndex = -1;

	do 
	{
		if(strFileName.isEmpty())
		{
			break;
		}

		nIndex = strFileName.lastIndexOf('.');
		if (-1 == nIndex)
		{
			break;
		}

		if (strFileName.endsWith("tar.gz"))
		{
			strSuffix = ".tar.gz";
		}
		else
		{
			strSuffix = strFileName.mid(nIndex);
		}

		if (strSuffix.isEmpty())
		{
			break;
		}

		SHFILEINFOW info = {0};
		if(SHGetFileInfoW(strSuffix.utf16(),
			FILE_ATTRIBUTE_NORMAL,
			&info,
			sizeof(info),
			SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES))
		{
			if (info.szTypeName[0])
			{
				file_type = QString::fromUtf16(info.szTypeName);
			}
		}

	} while (false);
	
	return file_type;
}