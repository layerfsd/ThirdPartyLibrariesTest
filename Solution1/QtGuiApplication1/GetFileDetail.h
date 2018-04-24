#ifndef GETFILEDETAIL_H
#define GETFILEDETAIL_H


#include <QString>
#include <QIcon>
#include <QPixmapCache>
#include <Windows.h>


class GetFileDetail
{
public:
	static QIcon GetFileIconFromFileName(const QString &strFileName);
	static QIcon GetFileIconFromResource(const QString &strFileName);
	static QString GetFileType(const QString &strFileName);

private:
	static BOOL FindIconFromCache(const QString &strKey, QIcon &icon);
	static BOOL InsertIconToCache(const QString &strKey, QIcon icon);
};


#endif//GETFILEDETAIL_H