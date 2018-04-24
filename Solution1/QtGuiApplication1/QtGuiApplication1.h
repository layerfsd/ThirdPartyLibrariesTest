#pragma once

#include <QtWidgets/QMainWindow>
#include <QDebug>
#include <Windows.h>
#include "FileClassifyDlg.h"
#include "ui_QtGuiApplication1.h"

class QtGuiApplication1 : public QMainWindow
{
    Q_OBJECT

public:
    QtGuiApplication1(QWidget *parent = Q_NULLPTR);
    ~QtGuiApplication1();

private:
    Ui::QtGuiApplication1Class ui;
    FileClassifyDlg *m_fileClassifyDlg;

public slots:
    void on_pushButton_start_clicked();
};
