#include <stdio.h>
#include <QApplication>

#include <QDialog>
#include "drivesynctestdialog.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    DriveSyncTestDialog d;

    d.show();

    return a.exec();
}
