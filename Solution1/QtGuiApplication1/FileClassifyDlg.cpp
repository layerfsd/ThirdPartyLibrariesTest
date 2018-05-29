#include <QStyledItemDelegate>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>
#include <QScrollBar>
#include <QtXml/QDomDocument>
#include "FileClassifyDlg.h"
#include "FileInfoModel.h"
#include "FileTypeModel.h"
#include "ObjectItem.h"
#include "GetFileDetail.h"

//**********************************************************
//用于实现QTableWidget, QTableView点击无虚线
//**********************************************************
class NoFocusDelegate : public QStyledItemDelegate
{
protected:
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index)const
	{
		QStyleOptionViewItem itemOption(option);
		if (itemOption.state & QStyle::State_HasFocus)
		{
			itemOption.state = itemOption.state ^ QStyle::State_HasFocus;
		}
		QStyledItemDelegate::paint(painter, itemOption, index);
	}
};

/////////////////////////////////////////////////////////////////
//线程
//
WorkThread::WorkThread(QObject *parent /* = 0 */)
:QThread(parent)
{
	_dataPath.clear();
	_stop = false;
}

WorkThread::~WorkThread()
{

}

void WorkThread::run()
{	
	travelFile();
}

void WorkThread::travelFile()
{
	QDir dir(_dataPath);
	QFileInfoList fileList;
	QFileInfo curFile;
	if(!dir.exists())
	{
		return;
	}

	fileList = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot);
	while(fileList.size() > 0)
	{
		int infoNum = fileList.size();
		for(int i = infoNum-1; i >= 0; --i)
		{
			if (_stop)
			{
				return;
			}

			curFile=fileList[i];
			if(curFile.isFile())//file
			{
				if (!_stop)
				{
					emit signal_addFile(curFile.absoluteFilePath());
					msleep(100);
				}
			}
			else if(curFile.isDir())//dir
			{
				QDir dirTemp(curFile.filePath());
				QFileInfoList fileList1 = dirTemp.entryInfoList(QDir::Dirs | QDir::Files | QDir::Readable | QDir::Hidden | QDir::NoDotAndDotDot);
				for(int j = 0; j < fileList1.size(); ++j)
				{
					if(!(fileList.contains(fileList1[j])))
					{
						fileList.append(fileList1[j]);
					}
				}
			}

			//关键步骤
			fileList.removeAt(i);
		}
	}
}

void WorkThread::setDataPath(const QString &dataPath)
{
	_dataPath = dataPath;
}

void WorkThread::setStop(bool stop)
{
	_stop = stop;
}

/////////////////////////////////////////////////////////////////
//窗体
//
FileClassifyDlg::FileClassifyDlg(QWidget *parent /* = 0 */)
: QFrame(parent)
{
	ui.setupUi(this);

	_currentIndex = QModelIndex();
	_currentRowCount = 0;

    initFileTypeMap();

	QStringList header;
	header << QString::fromLocal8Bit("文件名") << QString::fromLocal8Bit("修改时间") << QString::fromLocal8Bit("大小") << QString::fromLocal8Bit("路径");

    FileInfoModel *model = new FileInfoModel;
    if (model)
    {
        model->setHorizontalHeaderLabels(header);
        _fileInfoModelMap.insert("*.*", model);
        ui.tableView->setModel(model);
    }

    header.clear();
    header << QString::fromLocal8Bit("类型");

    _fileTypeModel = new FileTypeModel();
    if (_fileTypeModel)
    {
        _fileTypeModel->setHorizontalHeaderLabels(header);
        ui.treeView->setModel(_fileTypeModel);
    }

	ui.tableView->setColumnWidth(0, 220);
	ui.tableView->setColumnWidth(1, 130);
	ui.tableView->setColumnWidth(2, 80);
	ui.tableView->setItemDelegate(new NoFocusDelegate);
	ui.tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	ui.treeView->setItemDelegate(new NoFocusDelegate);
	ui.treeView->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	connect(ui.tableView->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slot_sliderValueChanged(int)));
	connect(&_thread, SIGNAL(signal_addFile(const QString &)), this, SLOT(slot_addFile(const QString &)));
}

FileClassifyDlg::~FileClassifyDlg()
{
	_thread.setStop(true);
	_thread.exit();
	_thread.wait();
}

QString FileClassifyDlg::formatFileSize(__int64 size)
{
	double dFileSize = size;
	wchar_t wzFileSize[MAX_PATH] = {0};

	if (dFileSize)
	{
		if (dFileSize < 1024)
		{
			StringCbPrintfW(wzFileSize, sizeof(wzFileSize), L"%.2lfB", dFileSize);
		}
		else if(dFileSize < 1024 * 1024)
		{
			StringCbPrintfW(wzFileSize, sizeof(wzFileSize), L"%.2lfKB", dFileSize / 1024);
		}
		else if(dFileSize < 1024 * 1024 * 1024)
		{
			StringCbPrintfW(wzFileSize, sizeof(wzFileSize), L"%.2lfMB", dFileSize / 1024 / 1024);
		}
		else
		{
			StringCbPrintfW(wzFileSize, sizeof(wzFileSize), L"%.2lfGB", dFileSize / 1024 / 1024 / 1024);
		}
	}

	return QString::fromUtf16(wzFileSize);
}

void FileClassifyDlg::initFileTypeMap()
{
    _fileTypeMap.clear();

    QString xmlPath = QString("%1/FileType.xml").arg(QApplication::applicationDirPath());
    QFile xml(xmlPath);
    if ( !xml.open(QIODevice::ReadOnly | QIODevice::Text) )
        return;

    QDomDocument doc;
    if (!doc.setContent(&xml, false))
    {
        xml.close();
        return;
    }

    xml.close();

    QDomNodeList fileTypes = doc.documentElement().childNodes();
    for (int i = 0; i != fileTypes.count(); ++i)
    {
        QDomElement type = fileTypes.at(i).toElement();
        QList<QString> types = type.attribute("value").split(";");

        for (int index = 0; index < types.size(); ++index)
        {
            _fileTypeMap.insert(types.at(index), type.attribute("name"));
        }

        _fileTypeRowMap.insert(type.attribute("name"), i);
    }

    _fileTypeRowMap.insert(QString::fromLocal8Bit("其它文件"), _fileTypeRowMap.size());
}

void FileClassifyDlg::slot_addFile(const QString &file)
{
	if (!QFile::exists(file))
	{
		return;
	}

    QString type, compareType, text;
    ObjectItem *item = NULL;
    ObjectItem *childItem = NULL;
    FileInfoModel *fileInfoModel = NULL;
    int rowCount = -1;
    quint32 num = 0;
    QRegExp rx("\\((\\d+)\\)");

	QFileInfo fileInfo(file);
	quint64 fileSize = fileInfo.size();

	//get file suffix
	FileInfoStruct fileInfoStruct;
	fileInfoStruct.strFileNameNoPath = fileInfo.fileName();
	fileInfoStruct.strFileNameWithPath = fileInfo.absoluteFilePath().replace(QChar('/'), QChar('\\'));
	if (fileInfoStruct.strFileNameWithPath.endsWith("tar.gz"))
	{
		fileInfoStruct.strFileSuffix = fileInfo.completeSuffix();
	}
	else
	{
		fileInfoStruct.strFileSuffix = fileInfo.suffix();
	}

    if (fileInfoStruct.strFileSuffix.isEmpty())
    {
        fileInfoStruct.strFileSuffix = QString::fromLocal8Bit("无后缀名");
    }

	fileInfoStruct.strFileSize = formatFileSize(fileSize);
	fileInfoStruct.strFileLastModifiedTime = fileInfo.lastModified().toString("yyyy/MM/dd hh:mm:ss");

    //model of total files
    if (_fileInfoModelMap.end() != _fileInfoModelMap.find("*.*"))
    {
        fileInfoModel = _fileInfoModelMap["*.*"];
        if (fileInfoModel)
        {
            rowCount = fileInfoModel->rowCount();
            fileInfoModel->setItem(rowCount, 0, fileInfoStruct.strFileNameNoPath, GetFileDetail::GetFileIconFromFileName(fileInfoStruct.strFileNameWithPath));
            fileInfoModel->setItem(rowCount, 1, fileInfoStruct.strFileLastModifiedTime);
            fileInfoModel->setItem(rowCount, 2, fileInfoStruct.strFileSize);
            fileInfoModel->setItem(rowCount, 3, fileInfoStruct.strFileNameWithPath);
        }
    }

    //fileInfoModel of one type files
    fileInfoModel = NULL;
    rowCount = -1;
    if (_fileInfoModelMap.end() != _fileInfoModelMap.find(fileInfoStruct.strFileSuffix))
    {
        fileInfoModel = _fileInfoModelMap[fileInfoStruct.strFileSuffix];
    }
    else
    {
        if (_fileTypeModel)
        {
            type = _fileTypeMap.value(fileInfoStruct.strFileSuffix, QString::fromLocal8Bit("其它文件"));
            item = _fileTypeModel->findItem(type);
            if (!item)
            {
                item = new ObjectItem(_fileTypeModel);
                if (item)
                {
                    _fileTypeModel->addItem(item);
                    item->setText(type);
                    item->setData(type, Qt::UserRole + 1);
                    item->setIcon(GetFileDetail::GetFileIconFromFileName(fileInfoStruct.strFileNameWithPath));
                }
            }

            if (item)
            {
                childItem = new ObjectItem(_fileTypeModel);
                if (childItem)
                {
                    childItem->setText(fileInfoStruct.strFileSuffix);
                    childItem->setData(fileInfoStruct.strFileSuffix, Qt::UserRole + 1);
                    childItem->setIcon(GetFileDetail::GetFileIconFromFileName(fileInfoStruct.strFileNameWithPath));
                    item->addChild(childItem);
                }
            }

            QStringList header;
            header << QString::fromLocal8Bit("文件名") << QString::fromLocal8Bit("修改时间") << QString::fromLocal8Bit("大小") << QString::fromLocal8Bit("路径");
            fileInfoModel = new FileInfoModel;
            if (fileInfoModel)
            {
                fileInfoModel->setHorizontalHeaderLabels(header);
                _fileInfoModelMap.insert(fileInfoStruct.strFileSuffix, fileInfoModel);
            }
        }
    }

    if (fileInfoModel)
    {
        rowCount = fileInfoModel->rowCount();
        fileInfoModel->setItem(rowCount, 0, fileInfoStruct.strFileNameNoPath, GetFileDetail::GetFileIconFromFileName(fileInfoStruct.strFileNameWithPath));
        fileInfoModel->setItem(rowCount, 1, fileInfoStruct.strFileLastModifiedTime);
        fileInfoModel->setItem(rowCount, 2, fileInfoStruct.strFileSize);
        fileInfoModel->setItem(rowCount, 3, fileInfoStruct.strFileNameWithPath);
    }


    if (_fileTypeModel)
    {
        //update total files num
        text = _fileTypeModel->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString();

        if(-1 != rx.indexIn(text))
        {
            num = rx.cap(1).toUInt();
            ++num;
            text.replace(rx, "(" + QString::number(num) + ")");
        }
        else
        {
            num = 1;
            text += QString("(%1)").arg(num);
        }

        _fileTypeModel->setHeaderData(0, Qt::Horizontal, text, Qt::DisplayRole);

        //update one type files num
        for (int i = 0; i < _fileTypeModel->rowCount(); ++i)
        {
            item = _fileTypeModel->item(i);
            if (item)
            {
                text = item->text();
                type = item->data(Qt::UserRole + 1).toString();
                compareType = _fileTypeMap.value(fileInfoStruct.strFileSuffix, QString::fromLocal8Bit("其它文件"));

                if (0 == type.compare(compareType, Qt::CaseSensitive))
                {
                    num = 0;

                    if(-1 != rx.indexIn(text))
                    {
                        num = rx.cap(1).toUInt();
                        ++num;
                        text.replace(rx, "(" + QString::number(num) + ")");
                    }
                    else
                    {
                        num = 1;
                        text += QString("(%1)").arg(num);
                    }

                    item->setText(text);

                    //update one suffix files num
                    for (int j = 0; j < item->childCount(); ++j)
                    {
                        childItem = item->child(j);
                        if (childItem)
                        {
                            text = childItem->text();
                            type = childItem->data(Qt::UserRole + 1).toString();
                            compareType = fileInfoStruct.strFileSuffix;

                            if (0 == type.compare(compareType, Qt::CaseSensitive))
                            {
                                num = 0;

                                if(-1 != rx.indexIn(text))
                                {
                                    num = rx.cap(1).toUInt();
                                    ++num;
                                    text.replace(rx, "(" + QString::number(num) + ")");
                                }
                                else
                                {
                                    num = 1;
                                    text += QString("(%1)").arg(num);
                                }

                                childItem->setText(text);
                                break;
                            }
                        }
                    }

                    break;
                }
            }
        }
    }
}

void FileClassifyDlg::on_treeView_clicked(const QModelIndex &index)
{
    if (index == _currentIndex)
    {
        return;
    }

    _currentRowCount = 0;
    _currentIndex = index;

    if (!_fileTypeModel)
        return;

    ObjectItem *item = _fileTypeModel->item(index);
    if (item)
    {
        FileInfoModel *model = NULL;
        QString fileType = item->data(Qt::UserRole + 1).toString();
        if (_fileInfoModelMap.end() != _fileInfoModelMap.find(fileType))
        {
            model = _fileInfoModelMap[fileType];
        }

        if (model)
        {
            ui.tableView->setModel(model);
        }
    }

    ui.tableView->verticalScrollBar()->setValue(0);
    slot_sliderValueChanged(0);
}

void FileClassifyDlg::on_tableView_doubleClicked(const QModelIndex &index)
{
	if (!index.isValid())
	{
		return;
	}
	
    FileInfoModel *current_model = (FileInfoModel*)(ui.tableView->model());
	if (!current_model)
	{
		return;
	}

    FileInfoItem *item = current_model->item(index.row(), 3);
	if (!item)
	{
		return;
	}

    QString filePath = item->text;
	if (QFile::exists(filePath))
	{
		filePath = "\"" + filePath + "\"";
		ShellExecuteW(NULL, L"open", filePath.utf16(), NULL, NULL, SW_NORMAL);
	}
	else
	{
		QMessageBox::information(0, QString::fromLocal8Bit("提示"), QString::fromLocal8Bit("文件不存在！"));
	}
}

void FileClassifyDlg::Update()
{
	_thread.setStop(true);
	_thread.exit();
	_thread.wait();

    for(QMap<QString, FileInfoModel*>::iterator iter = _fileInfoModelMap.begin();
        iter != _fileInfoModelMap.end();
        ++iter)
    {
        FileInfoModel *model = iter.value();
        if (model)
        {
            model->clear();

            if (0 != QString::compare("*.*", iter.key()))
            {
                delete model;
            }
        }
    }

	ui.tableView->setFont(QString::fromLocal8Bit("微软雅黑"));
	QString dataPath = "D:\\software";
	_thread.setStop(false);
	_thread.setDataPath(dataPath);
	_thread.start();
}

void FileClassifyDlg::slot_pushButton_clicked()
{
    FileInfoModel *current_model = static_cast<FileInfoModel*>(ui.tableView->model());
	if (!current_model)
	{
		return;
	}

	QPushButton *button = qobject_cast<QPushButton*>(sender());
	if (!button)
	{
		return;
	}

	int currentRow = ui.tableView->rowAt(button->y());
	if (-1 == currentRow)
	{
		return;
	}

	QModelIndex index = current_model->index(currentRow, 0, QModelIndex());
	ui.tableView->setCurrentIndex(index);

	if (!current_model->item(currentRow, 3))
	{
		return;
	}

	QString filePath = current_model->data(index).toString();
	if (QFile::exists(filePath))
	{
		QString cmd = "/select,\"" + filePath + "\"";
		ShellExecuteW(NULL, L"open", L"explorer.exe", cmd.utf16(), NULL, SW_SHOWNORMAL);
	}
	else
	{
		QMessageBox::information(0, QString::fromLocal8Bit("提示"), QString::fromLocal8Bit("文件不存在！"));
	}
}

void FileClassifyDlg::slot_sliderValueChanged(int value)
{
    FileInfoModel *current_model = static_cast<FileInfoModel*>(ui.tableView->model());
	if (!current_model)
	{
		return;
	}

	if (current_model == _fileInfoModelMap["*.*"])
	{
		return;
	}

	//滚动至每页倒数50行时加载下一页
	if (_currentRowCount > 50)
	{
		if (value <= _currentRowCount - 50)
		{
			return;
		}
	}

	int totalRowCount = current_model->rowCount();	
	if (totalRowCount == _currentRowCount)
	{
		return;
	}

	qDebug() << QString("total row : %1	current row : %2").arg(totalRowCount).arg(value);

	int count = 0;
	if (totalRowCount - _currentRowCount > 100)
	{
		count = _currentRowCount + 100; 
	}
	else
	{
		count = totalRowCount;
	}

	_currentRowCount = count;
}