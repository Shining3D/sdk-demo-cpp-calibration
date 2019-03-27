#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

	//int x = -1;
	//char bufx[100] = { 0 };
	//memcpy(bufx, &x, 4);

	//int y = 0;
	//QByteArray mm(bufx);
	//memcpy(&y, mm.constData(), mm.size());

    w.show();

    return a.exec();
}
