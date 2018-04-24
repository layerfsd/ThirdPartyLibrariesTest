#include "QtGuiApplication1.h"

QtGuiApplication1::QtGuiApplication1(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    m_fileClassifyDlg = new FileClassifyDlg(this);
    ui.stackedWidget->addWidget(m_fileClassifyDlg);
}

QtGuiApplication1::~QtGuiApplication1()
{

}

void QtGuiApplication1::on_pushButton_start_clicked()
{
    if (m_fileClassifyDlg)
    {
        m_fileClassifyDlg->Update();
        ui.stackedWidget->setCurrentWidget(m_fileClassifyDlg);
    }
}
