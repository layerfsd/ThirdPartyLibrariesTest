//本文件为UTF-8编码格式
#include <Windows.h>
#include <Windowsx.h>

#include <QSize>
#include <QRect>
#include <QString>
#include <QMargins>
#include <QWidget>
#include <QPoint>
#include <QFile>
#include <QLayout>

#include "AdpatDpi.h"

static double sg_nDpiZoopValue = 1.0;
static bool sg_bDpiInited = false;

//计算dpi值，在CWinUtil中对win32api统一封装
void COMPUTE_DPI_MULTIPLE()
{
	QSysInfo::WinVersion eVersion = QSysInfo::windowsVersion();
	if (eVersion > QSysInfo::WV_VISTA)
 	{
 		HMODULE hModule = NULL;  
 		typedef int (*Func)();  
 
 		//   动态加载  DLL  文件
 		QString strPath = "C:\\Windows\\System32\\user32.dll";		
 		if (QFile::exists(strPath))
 		{
 			hModule = LoadLibrary((wchar_t*)(strPath.utf16()));
 
 			// 获取SetProcessDPIAware  函数地址   
 			Func fSetProcessDPIAware = (Func)GetProcAddress(hModule, "SetProcessDPIAware" );  
 			if (NULL != fSetProcessDPIAware)
 			{
 				fSetProcessDPIAware();//最低 vista 可以调用
 			}
 
 			//   最后记得要释放指针   
 			FreeLibrary(hModule); 
 		}		
 	}
 
 	HDC hdc = ::GetWindowDC(::GetDesktopWindow());
 
 	int dpi = GetDeviceCaps(hdc, LOGPIXELSX);

	double nZoopValue = (double)dpi / 120;//缩放倍数

	if(nZoopValue < 1.1999999999)
	{
		nZoopValue =  1.0;
	}
	else if(nZoopValue < 1.4999999999)
	{
		nZoopValue =  1.2;
	}
	else if(nZoopValue < 1.9999999999)
	{
		nZoopValue = 1.5;
	}
	else if(nZoopValue < 2.4999999999)
	{
		nZoopValue = 2.0;
	}
	else if(nZoopValue < 2.9999999999)
	{
		nZoopValue = 2.5;
	}
	else if(nZoopValue < 3.4999999999)
	{
		nZoopValue = 3.0;
	}
	else if(nZoopValue < 3.9999999999)
	{
		nZoopValue = 3.5;
	}
	else
	{
		nZoopValue =  4.0;
	}

	sg_nDpiZoopValue = nZoopValue;
//	sg_nDpiZoopValue = 1.5;
}

double GET_DPI_MULTIPLE()
{
	double nZoopValue = 1.0;
	if (!sg_bDpiInited)
	{
		COMPUTE_DPI_MULTIPLE();
		sg_bDpiInited = true;
		nZoopValue = sg_nDpiZoopValue;
	}
	else
	{
		nZoopValue = sg_nDpiZoopValue;
	}
	return nZoopValue;
}

int ADAPT_DPI_INT(int x) 
{ 
	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return x;
	}
	return GET_DPI_MULTIPLE() * x;
}

double ADAPT_DPI_DOUBLE(double x)
{
	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return x;
	}
	return GET_DPI_MULTIPLE() * x;
}

QSize ADAPT_DPI_SIZE(QSize size)
{
	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return size;
	}

	QSizeF sizeF = size;

	if(size.width() >= MAXWITHORHEIGHT && size.height() < MAXWITHORHEIGHT)
	{
		sizeF = QSizeF(sizeF.width(), ADAPT_DPI_DOUBLE(sizeF.height()));
	}
	else if(size.width() < MAXWITHORHEIGHT && size.height() >= MAXWITHORHEIGHT)
	{
		sizeF = QSizeF(ADAPT_DPI_DOUBLE(size.width()), sizeF.height());
	}
	else
	{
		sizeF = sizeF * GET_DPI_MULTIPLE();
	}

	return sizeF.toSize();
}

QRect ADAPT_DPI_RECT(QRect rect)
{
	// 	QRectF rectF = rect;
	// 	QDesktopWidget* deskwidget = QApplication::desktop();
	// 	QSize size =  ADAPT_DPI_SIZE(rect.size());
	// 	QPoint topleft = QPoint((deskwidget->width()-size.width())/2, (deskwidget->height()-size.height())/2);
	// 
	// 	rectF = QRectF(topleft,size);

	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return rect;
	}

	QRectF rectF = rect;

	rectF = QRectF(rectF.topLeft(), ADAPT_DPI_SIZE(rect.size()));

	return rectF.toRect();
}

QString ADAPT_DPI_STYLESHEET(QString styleSheet)
{
	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return styleSheet;
	}

	if(styleSheet.isEmpty() || styleSheet.contains("px", Qt::CaseInsensitive) == false)
	{
		return styleSheet;
	}

	QString sourceStr = styleSheet;
	QString destStr, temp;

	for(int i = 0; i < sourceStr.length(); i++)
	{
		QChar character  = sourceStr.at(i);

		if(character.isDigit())
		{
			temp.append(character);
		}
		else if(character.toLower() == 'p' && temp.length() > 0 && temp.at(0).isDigit())
		{
			temp.append(character);
		}
		else if(character.toLower() == 'x' && temp.length() > 1 && temp.at(0).isDigit() && temp.right(1) == "p")
		{
			temp.append(character);

			if(temp.length() > 2)
			{
				QString pxValueStr = temp.left(temp.length() - 2);
				int pxValue = pxValueStr.toInt();
				int dpiPxValue = ADAPT_DPI_INT(pxValue);
				QString newPxValueStr = QString::number(dpiPxValue) + "px";

				destStr.append(newPxValueStr);

				temp.clear();
			}
		}
		else if(character.toLower() == 't' && temp.length() > 1 && temp.at(0).isDigit() && temp.right(1) == "p")
		{
			temp.append(character);

			if(temp.length() > 2)
			{
				QString pxValueStr = temp.left(temp.length() - 2);
				int pxValue = pxValueStr.toInt();
				int dpiPxValue = ADAPT_DPI_INT(pxValue)*4/3 ;
				QString newPxValueStr = QString::number(dpiPxValue) + "px";

				destStr.append(newPxValueStr);

				temp.clear();
			}
		}
		else
		{
			temp.append(character);
			destStr.append(temp);
			temp.clear();
		}
	}

	if(temp.isEmpty() == false)
	{
		destStr.append(temp);
		temp.clear();
	}

	return destStr;
}

QString ADAPT_DPI_STYLESHEET(QString styleSheet, double dbRate)
{
    if (dbRate == 1.0)
    {
        return styleSheet;
    }

    if(styleSheet.isEmpty() || styleSheet.contains("px", Qt::CaseInsensitive) == false)
    {
        return styleSheet;
    }

    QString sourceStr = styleSheet;
    QString destStr, temp;

    for(int i = 0; i < sourceStr.length(); i++)
    {
        QChar character  = sourceStr.at(i);

        if(character.isDigit())
        {
            temp.append(character);
        }
        else if(character.toLower() == 'p' && temp.length() > 0 && temp.at(0).isDigit())
        {
            temp.append(character);
        }
        else if(character.toLower() == 'x' && temp.length() > 1 && temp.at(0).isDigit() && temp.right(1) == "p")
        {
            temp.append(character);

            if(temp.length() > 2)
            {
                QString pxValueStr = temp.left(temp.length() - 2);
                int pxValue = pxValueStr.toInt();
                int dpiPxValue = pxValue * dbRate;
                QString newPxValueStr = QString::number(dpiPxValue) + "px";

                destStr.append(newPxValueStr);

                temp.clear();
            }
        }
        else if(character.toLower() == 't' && temp.length() > 1 && temp.at(0).isDigit() && temp.right(1) == "p")
        {
            temp.append(character);

            if(temp.length() > 2)
            {
                QString pxValueStr = temp.left(temp.length() - 2);
                int pxValue = pxValueStr.toInt();
                int dpiPxValue = pxValue*dbRate*4/3 ;
                QString newPxValueStr = QString::number(dpiPxValue) + "px";

                destStr.append(newPxValueStr);

                temp.clear();
            }
        }
        else
        {
            temp.append(character);
            destStr.append(temp);
            temp.clear();
        }
    }

    if(temp.isEmpty() == false)
    {
        destStr.append(temp);
        temp.clear();
    }

    return destStr;
}

QMargins ADAPT_DPI_CONTANTMARGINS(QMargins margins)
{
	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return margins;
	}

	int nleft = ADAPT_DPI_INT(margins.left());
	int nright = ADAPT_DPI_INT(margins.right());
	int ntop = ADAPT_DPI_INT(margins.top());
	int nbottom = ADAPT_DPI_INT(margins.bottom());
	return QMargins(nleft, ntop, nright, nbottom);
}

int ADAPT_DPI_DIVIDE(int x)
{
	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return x;
	}
	return x/GET_DPI_MULTIPLE();
}

void ADAPT_DPI_WIDGET(QWidget *widget)
{
	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return;
	}

	if(widget != NULL)
	{
		QString strWidgetName = widget->objectName();
		QLayout* pLayout = widget->layout();
		if (pLayout && widget->objectName() != QString("widget_border"))
		{
			QMargins contentMargins = ADAPT_DPI_CONTANTMARGINS(pLayout->contentsMargins());
			pLayout->setContentsMargins(contentMargins);
			pLayout->setSpacing(ADAPT_DPI_INT(pLayout->spacing()));
			widget->setLayout(pLayout);
		}
		widget->setMinimumSize(ADAPT_DPI_SIZE(widget->minimumSize()));
		widget->setMaximumSize(ADAPT_DPI_SIZE(widget->maximumSize()));
		// 		QRect rect = widget->geometry();
		// 		widget->setGeometry(ADAPT_DPI_RECT(widget->geometry()));
		widget->setStyleSheet(ADAPT_DPI_STYLESHEET(widget->styleSheet()));

		QObjectList list = widget->children();
		for(int i = 0; i < list.size(); i++)
		{
			QWidget *widget = static_cast<QWidget*>(list.at(i));
			if(widget != NULL)
			{
				QString objectName = widget->objectName();

				if(objectName.isEmpty() == false && objectName.left(3) != "qt_" && widget->isWidgetType())
				{
					ADAPT_DPI_WIDGET(widget);
				}

			}
		}
	}
}

QPoint ADAPT_DPI_POINT( QPoint point )
{
	if (GET_DPI_MULTIPLE() == 1.0)
	{
		return point;
	}

	QPoint pointa = point;

	pointa = QPoint(ADAPT_DPI_INT(point.x()), ADAPT_DPI_INT(point.y()));

	return pointa;
}
