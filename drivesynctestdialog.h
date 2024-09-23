#ifndef DRIVESYNCTESTDIALOG_H
#define DRIVESYNCTESTDIALOG_H

#include <QDialog>
#include "googledriveservice.h"

namespace Ui {
class DriveSyncTestDialog;
}

class DriveSyncTestDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DriveSyncTestDialog(QWidget *parent = nullptr);
    ~DriveSyncTestDialog();

private slots:
    void on_pushButtonSyncData_clicked();

    void on_pushButtonAuth_clicked();

    void on_pushbuttonCreateFolder_clicked();

    void on_pushbuttonUploadFile_clicked();

    void on_pushbuttonDownload_clicked();

    void on_pushbuttonListFiles_clicked();

private:
    Ui::DriveSyncTestDialog *ui;

    GoogleDriveService* m_gd;
};

#endif // DRIVESYNCTESTDIALOG_H
