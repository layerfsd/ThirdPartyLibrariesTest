#ifndef FILECLASSIFYFLG_H
#define FILECLASSIFYFLG_H


#include <QThread>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <Windows.h>
#include <strsafe.h>
#include "ui_FileClassifyDlg.h"


//线程
class WorkThread : public QThread
{
	Q_OBJECT

public:
	explicit WorkThread(QObject *parent = 0);
	~WorkThread();
	void setDataPath(const QString &dataPath);
	void setStop(bool stop);

protected:
	void run();

private:
	bool m_stop;
	QString m_dataPath;
	void travelFile();

signals:
	void signal_addFile(const QString &file);
};

/*文件信息结构*/
struct FileInfoStruct 
{
    QString	strFileNameNoPath;
    QString	strFileSuffix;
    QString	strFileLastModifiedTime;
    QString	strFileSize;
    QString	strFileNameWithPath;
};

//窗体
class FileClassifyModel;
class FileClassifyDlg : public QFrame
{
	Q_OBJECT

public:
	FileClassifyDlg(QWidget *parent = 0);
	~FileClassifyDlg();
	void Update();

private:
	Ui::FileClassifyDlg ui;
	WorkThread m_thread;
	int m_currentRowCount;
	QTableWidgetItem *m_currentItem;

	//<key=file suffix, value=file info model>
    QMap<QString, FileClassifyModel*>	m_modelMap;

private:
	QString formatFileSize(__int64 size);

public slots:
	void slot_addFile(const QString &file);
	void on_tableWidget_itemClicked(QTableWidgetItem *item);
	void on_tableView_doubleClicked(const QModelIndex &index);
	void slot_sliderValueChanged(int value);
};

#endif //FILECLASSIFYFLG_H
