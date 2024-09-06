#include <QtNetworkAuth>
#include <QSqlDatabase>
#include "drivesynctestdialog.h"
#include "ui_drivesynctestdialog.h"
#include "sqllitesync.h"
#include "googledriveservice.h"

DriveSyncTestDialog::DriveSyncTestDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DriveSyncTestDialog)
{
    ui->setupUi(this);

    m_gd = new GoogleDriveService(this);
}

DriveSyncTestDialog::~DriveSyncTestDialog()
{
    delete m_gd;

    delete ui;
}

void DriveSyncTestDialog::on_pushButtonSyncData_clicked()
{
    printf("test application started...\n");

    QSqlDatabase db;

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("/home/paulmckinney/Documents/DriveSyncTester/ExampleSync.db");
    db.open();

    SqlLiteSync sls;

    sls.CreateLocalFolders();
    sls.CreateRemoteFolders();
    sls.SendChangesToServer();
    db.close();

    printf("application finished.\n");
}


void DriveSyncTestDialog::on_pushButtonAuth_clicked()
{
    m_gd->authenticate();
}


void DriveSyncTestDialog::on_pushbuttonCreateFolder_clicked()
{
    if (m_gd->checkAuthentication())
    {
        QString db_folder = m_gd->createFolder("database");
        QString tb_folder = m_gd->createFolder("tablename", db_folder);
    }
}


void DriveSyncTestDialog::on_pushbuttonUploadFile_clicked()
{
    if (m_gd->checkAuthentication())
    {
        QString db_folder = m_gd->folderExists("database");
        QString tb_folder = m_gd->folderExists("tablename", db_folder);

        QString file_id = m_gd->uploadFile("/home/paulmckinney/Documents/uploadtest.txt", "uploadtest.txt", tb_folder);
    }
}


void DriveSyncTestDialog::on_pushbuttonDownload_clicked()
{
    if (m_gd->checkAuthentication())
    {
        QString db_folder = m_gd->folderExists("database");
        QString tb_folder = m_gd->folderExists("tablename", db_folder);
        QString file_id = m_gd->fileExists("uploadtest.txt", tb_folder);

        m_gd->downloadFile(file_id, "/home/paulmckinney/Documents/download");
    }
}


void DriveSyncTestDialog::on_pushbuttonListFiles_clicked()
{
    if (m_gd->checkAuthentication())
    {
        QString db_folder = m_gd->folderExists("database");
        QString tb_folder = m_gd->folderExists("tablename", db_folder);

        QDateTime date = QDateTime::currentDateTime().addDays(-7);

        qDebug() << m_gd->listFilesNewerThan(tb_folder, date);
    }
}

