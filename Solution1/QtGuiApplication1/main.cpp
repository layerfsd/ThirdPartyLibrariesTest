#include "QtGuiApplication1.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString file = QString("%1/common_style.qss").arg(QApplication::applicationDirPath());
    QFile qss(file);
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&qss);
        a.setStyleSheet(in.readAll());
    }

    QtGuiApplication1 w;
    w.show();
    return a.exec();
}
