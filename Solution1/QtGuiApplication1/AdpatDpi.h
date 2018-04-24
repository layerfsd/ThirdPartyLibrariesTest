#ifndef ADAPT_DPI_HH
#define ADAPT_DPI_HH

#define  MAXWITHORHEIGHT    16777215
class QSize;
class QRect;
class QString;
class QMargins;
class QWidget;
class QPoint;

void COMPUTE_DPI_MULTIPLE();
double GET_DPI_MULTIPLE();
int ADAPT_DPI_INT(int x);
double ADAPT_DPI_DOUBLE(double x);
QSize ADAPT_DPI_SIZE(QSize size);
QRect ADAPT_DPI_RECT(QRect rect);
QString ADAPT_DPI_STYLESHEET(QString styleSheet);
QString ADAPT_DPI_STYLESHEET(QString styleSheet, double rate);
QMargins ADAPT_DPI_CONTANTMARGINS(QMargins margins);
QPoint ADAPT_DPI_POINT(QPoint point);
int ADAPT_DPI_DIVIDE(int x);
void ADAPT_DPI_WIDGET(QWidget *widget);

#endif
