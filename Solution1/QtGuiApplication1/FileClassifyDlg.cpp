#include <QStyledItemDelegate>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>
#include <QScrollBar>
#include "FileClassifyDlg.h"
#include "FileClassifyModel.h"
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
	m_dataPath.clear();
	m_stop = false;
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
	QDir dir(m_dataPath);
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
			if (m_stop)
			{
				return;
			}

			curFile=fileList[i];
			if(curFile.isFile())//file
			{
				if (!m_stop)
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
	m_dataPath = dataPath;
}

void WorkThread::setStop(bool stop)
{
	m_stop = stop;
}

/////////////////////////////////////////////////////////////////
//窗体
//
FileClassifyDlg::FileClassifyDlg(QWidget *parent /* = 0 */)
: QFrame(parent)
{
	ui.setupUi(this);

	m_currentItem = NULL;
	m_currentRowCount = 0;

	QStringList header;
	header << QString::fromUtf16(L"文件名") << QString::fromUtf16(L"修改时间") << QString::fromUtf16(L"大小") << QString::fromUtf16(L"路径");

    FileClassifyModel *model = new FileClassifyModel;
    if (model)
    {
        model->setHorizontalHeaderLabels(header);
        m_modelMap.insert("*.*", model);
        ui.tableView->setModel(model);
    }

	ui.tableView->setColumnWidth(0, 220);
	ui.tableView->setColumnWidth(1, 130);
	ui.tableView->setColumnWidth(2, 80);
	ui.tableView->setItemDelegate(new NoFocusDelegate);
	ui.tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	ui.tableWidget->setItemDelegate(new NoFocusDelegate);
	ui.tableWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	/*connect(ui.tableView->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slot_sliderValueChanged(int)));
	connect(&m_thread, SIGNAL(signal_addFile(const QString &)), this, SLOT(slot_addFile(const QString &)));*/

    connect(ui.tableView->verticalScrollBar(), &QScrollBar::valueChanged, this, &FileClassifyDlg::slot_sliderValueChanged);
    connect(&m_thread, &WorkThread::signal_addFile, this, &FileClassifyDlg::slot_addFile);
}

FileClassifyDlg::~FileClassifyDlg()
{
	m_thread.setStop(true);
	m_thread.exit();
	m_thread.wait();
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

void FileClassifyDlg::slot_addFile(const QString &file)
{
	if (!QFile::exists(file))
	{
		return;
	}

	QFileInfo fileInfo(file);
	quint64 fileSize = fileInfo.size();

	//get file suffix
	FileInfoStruct fileInfoStruct;
	fileInfoStruct.strFileNameNoPath = fileInfo.fileName();
	fileInfoStruct.strFileNameWithPath = fileInfo.absoluteFilePath().replace('/', '\\');
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
        fileInfoStruct.strFileSuffix = QString::fromUtf16(L"无后缀名");
    }

	fileInfoStruct.strFileSize = formatFileSize(fileSize);
	fileInfoStruct.strFileLastModifiedTime = fileInfo.lastModified().toString("yyyy/MM/dd hh:mm:ss");

    //model of total files
    FileClassifyModel *model = NULL;
    int rowCount = -1;
    if (m_modelMap.end() != m_modelMap.find("*.*"))
    {
        model = m_modelMap["*.*"];
        if (model)
        {
            rowCount = model->rowCount();
            model->setItem(rowCount, 0, fileInfoStruct.strFileNameNoPath, GetFileDetail::GetFileIconFromFileName(fileInfoStruct.strFileNameWithPath));
            model->setItem(rowCount, 1, fileInfoStruct.strFileLastModifiedTime);
            model->setItem(rowCount, 2, fileInfoStruct.strFileSize);
            model->setItem(rowCount, 3, fileInfoStruct.strFileNameWithPath);
        }
    }

    //model of one type files
    model = NULL;
    rowCount = -1;
    if (m_modelMap.end() != m_modelMap.find(fileInfoStruct.strFileSuffix))
    {
        model = m_modelMap[fileInfoStruct.strFileSuffix];
    }
    else
    {
        QTableWidgetItem *item = new QTableWidgetItem(fileInfoStruct.strFileSuffix);
        item->setData(Qt::UserRole + 1, fileInfoStruct.strFileSuffix);
        item->setIcon(GetFileDetail::GetFileIconFromFileName(fileInfoStruct.strFileNameWithPath));
        rowCount = ui.tableWidget->rowCount();
        ui.tableWidget->insertRow(rowCount);
        ui.tableWidget->setItem(rowCount, 0, item);

        QStringList header;
        header << QString::fromUtf16(L"文件名") << QString::fromUtf16(L"修改时间") << QString::fromUtf16(L"大小") << QString::fromUtf16(L"路径");
        model = new FileClassifyModel;
        if (model)
        {
            model->setHorizontalHeaderLabels(header);
            m_modelMap.insert(fileInfoStruct.strFileSuffix, model);
        }
    }

    if (model)
    {
        rowCount = model->rowCount();
        model->setItem(rowCount, 0, fileInfoStruct.strFileNameNoPath, GetFileDetail::GetFileIconFromFileName(fileInfoStruct.strFileNameWithPath));
        model->setItem(rowCount, 1, fileInfoStruct.strFileLastModifiedTime);
        model->setItem(rowCount, 2, fileInfoStruct.strFileSize);
        model->setItem(rowCount, 3, fileInfoStruct.strFileNameWithPath);
    }


	//update total files num
	QTableWidgetItem *item_type = ui.tableWidget->horizontalHeaderItem(0);
	QString text;
	quint32 num = 0;
	if (item_type)
	{
		text = item_type->text();
	}

	QRegExp rx("\\((\\d+)\\)");
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

	item_type->setText(text);
	ui.tableWidget->setHorizontalHeaderItem(0, item_type);

	//update one type files num
	for (int index = 0; index < ui.tableWidget->rowCount(); ++index)
	{
		item_type = ui.tableWidget->item(index, 0);
		if (0 == QString::compare(item_type->data(Qt::UserRole + 1).toString(), fileInfoStruct.strFileSuffix))
		{
			num = 0;
			text.clear();
			if (item_type)
			{
				text = item_type->text();
			}

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

			item_type->setText(text);
		}
	}
    
}

void FileClassifyDlg::on_tableWidget_itemClicked(QTableWidgetItem *item)
{
	if (item == m_currentItem || !item)
	{
		return;
	}

	m_currentRowCount = 0;
	m_currentItem = item;
    FileClassifyModel *model = NULL;
	QString fileType = item->data(Qt::UserRole + 1).toString();
	if (m_modelMap.end() != m_modelMap.find(fileType))
	{
		model = m_modelMap[fileType];
	}

	if (model)
	{
		ui.tableView->setModel(model);
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
	
    FileClassifyModel *current_model = (FileClassifyModel*)(ui.tableView->model());
	if (!current_model)
	{
		return;
	}

    FileItem *item = current_model->item(index.row(), 3);
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
		QMessageBox::information(0, QString::fromUtf16(L"提示"), QString::fromUtf16(L"文件不存在！"));
	}
}

void FileClassifyDlg::Update()
{
	m_thread.setStop(true);
	m_thread.exit();
	m_thread.wait();

    for(QMap<QString, FileClassifyModel*>::iterator iter = m_modelMap.begin();
        iter != m_modelMap.end();
        ++iter)
    {
        FileClassifyModel *model = iter.value();
        if (model)
        {
            model->clear();

            if (0 != QString::compare("*.*", iter.key()))
            {
                delete model;
            }
        }
    }

	ui.tableView->setFont(QString::fromUtf16(L"微软雅黑"));
	QString dataPath = QString::fromUtf16(L"D:\\software");
	m_thread.setStop(false);
	m_thread.setDataPath(dataPath);
	m_thread.start();
}

void FileClassifyDlg::slot_sliderValueChanged(int value)
{
    FileClassifyModel *current_model = static_cast<FileClassifyModel*>(ui.tableView->model());
	if (!current_model)
	{
		return;
	}

	if (current_model == m_modelMap["*.*"])
	{
		return;
	}

	//滚动至每页倒数50行时加载下一页
	if (m_currentRowCount > 50)
	{
		if (value <= m_currentRowCount - 50)
		{
			return;
		}
	}

	int totalRowCount = current_model->rowCount();	
	if (totalRowCount == m_currentRowCount)
	{
		return;
	}

	qDebug() << QString("total row : %1	current row : %2").arg(totalRowCount).arg(value);

	int count = 0;
	if (totalRowCount - m_currentRowCount > 100)
	{
		count = m_currentRowCount + 100; 
	}
	else
	{
		count = totalRowCount;
	}

	m_currentRowCount = count;
}