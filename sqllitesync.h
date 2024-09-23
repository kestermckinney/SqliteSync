#ifndef SQLLITESYNC_H
#define SQLLITESYNC_H

#include "googledriveservice.h"

#include <QObject>
#include <QDateTime>

enum ServiceType : int {GoogleDrive};

class SqlLiteSync : public QObject
{
    Q_OBJECT

private:
    QDateTime m_previous_sync_time;
    QString m_session_info; // refresh key for authentication
    ServiceType m_service_type; // Google Drive or some other service
    QStringList m_table_names; // keep these in memory, they will correspond with folder names
    int m_max_items_to_sync = 1000; // max items to sync each sync session
    QString m_application_name; // this will be the base folder that holds the table folders
    QString m_temporary_folder; // the temporary location for the sync files

    GoogleDriveService* m_drive_service = nullptr;

public:

    explicit SqlLiteSync(QObject *t_parent = nullptr);
    ~SqlLiteSync();

    void setDriveService(ServiceType t_service);

    void GetTableNames();
    void CreateLocalFolders();
    void CreateRemoteFolders();
    void PullUpdatedFiles();
    void SendChangesToServer();

signals:
};

#endif // SQLLITESYNC_H
