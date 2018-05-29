#ifndef FILECLASSIFYFLG_H
#define FILECLASSIFYFLG_H


#include <QThread>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QMap>
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
    bool _stop;
    QString _dataPath;
    void travelFile();

signals:
    void signal_addFile(const QString &file);
};

/*文件信息结构*/
struct FileInfoStruct 
{
    QString strFileNameNoPath;
    QString strFileSuffix;
    QString strFileLastModifiedTime;
    QString strFileSize;
    QString strFileNameWithPath;
};

//窗体
class FileInfoModel;
class FileTypeModel;
class FileClassifyDlg : public QFrame
{
    Q_OBJECT

public:
    FileClassifyDlg(QWidget *parent = 0);
    ~FileClassifyDlg();
    void Update();

private:
    Ui::FileClassifyDlg ui;
    WorkThread _thread;
    int _currentRowCount;
    QModelIndex _currentIndex;
    
    // <key, value> = <file suffix, file info model>
    QMap<QString, FileInfoModel*> _fileInfoModelMap;
    FileTypeModel *_fileTypeModel;

    // <key, value> = <file suffix, file type>
    // eg. <.txt, 文本文件>
    QMap<QString, QString> _fileTypeMap;

    // <key, value> = <file type, row>
    // eg.<文本文件, 0>
    QMap<QString, int> _fileTypeRowMap;

private:
    QString formatFileSize(__int64 size);
    void initFileTypeMap();

public slots:
    void slot_addFile(const QString &file);
    void slot_pushButton_clicked();
    void on_treeView_clicked(const QModelIndex &index);
    void on_tableView_doubleClicked(const QModelIndex &index);
    void slot_sliderValueChanged(int value);
};

#endif //FILECLASSIFYFLG_H
