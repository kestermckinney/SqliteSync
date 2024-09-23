#include "sqllitesync.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>

SqlLiteSync::SqlLiteSync(QObject *t_parent)
    : QObject{t_parent}
{
    // get the latest configuration
    QSqlQuery select;
    select.prepare("select last_sync_time, session_info, max_items_to_sync, service_type, application_name, temporary_folder from sqlitesync where sync_id = 1");

    if (select.exec())
    {
        if (select.next())
        {
            if (!select.value("last_sync_time").isNull()) m_previous_sync_time.setSecsSinceEpoch(select.value("last_sync_time").toLongLong());
            if (!select.value("session_info").isNull()) m_session_info = select.value("session_info").toString();
            if (!select.value("max_items_to_sync").isNull()) m_max_items_to_sync = select.value("max_items_to_sync").toInt();
            if (!select.value("application_name").isNull()) m_application_name = select.value("application_name").toString();
            if (!select.value("temporary_folder").isNull()) m_temporary_folder = select.value("temporary_folder").toString();

            if (select.value("service_type").isNull())
                m_service_type = ServiceType::GoogleDrive;
            else
            {
                switch (select.value("service_type").toInt()) {
                case 1:
                    m_service_type = ServiceType::GoogleDrive;
                    break;
                default:
                    m_service_type = ServiceType::GoogleDrive;
                    break;
                }

                setDriveService(m_service_type);
            }
        }
        else
        {
            qDebug() << "Failed to get SqlLiteSync session information. Database will not sync with host.";
            qDebug() << select.lastError();
            return;
        }
    }
    else
    {
        qDebug() << "Failed to get SqlLiteSync session information. Database will not sync with host.";
        qDebug() << select.lastError();
        return;
    }

    GetTableNames();
}

void SqlLiteSync::setDriveService(ServiceType t_service)
{
    if (t_service == ServiceType::GoogleDrive)
    {
        m_service_type = t_service;
        m_drive_service = new GoogleDriveService();
    }
}

SqlLiteSync::~SqlLiteSync()
{
    if (m_drive_service)
        delete m_drive_service;
}

void SqlLiteSync::CreateLocalFolders()
{
    if (m_temporary_folder.isEmpty())
    {
        qDebug() << "SqlLiteSync failed to create local folders. Temporary folder was not specified.  Database will not sync with host.";
        return;
    }

    if (m_application_name.isEmpty())
    {
        qDebug() << "SqlLiteSync failed to create local folders. Application name was not specified.  Database will not sync with host.";
        return;
    }

    if (m_table_names.count() == 0)
    {
        qDebug() << "SqlLiteSync failed to create local folders. No table names were found.  Database will not sync with host.";
        return;
    }

    QDir d(m_temporary_folder + "/" + m_application_name);
    for (QString tn : m_table_names)
    {
        d.mkpath(tn);
        qDebug() << "creating folder:  " << m_temporary_folder + "/" + m_application_name + "/" + tn;
    }
}

void SqlLiteSync::CreateRemoteFolders()
{
    if (!m_drive_service)
    {
        qDebug() << "SqlLiteSync failed to create remote folders. Drive service not specified.";
        return;
    }

    if (m_application_name.isEmpty())
    {
        qDebug() << "SqlLiteSync failed to create remote folders. Application name was not specified.  Database will not sync with host.";
    }

    if (m_table_names.count() == 0)
    {
        qDebug() << "SqlLiteSync failed to create remote folders. No table names were found.  Database will not sync with host.";
        return;
    }

    if (m_drive_service->checkAuthentication())
    {
        QString db_folder_id = m_drive_service->createFolder(m_application_name);

        if (!db_folder_id.isEmpty())
            for (QString tn : m_table_names)
            {
                m_drive_service->createFolder(tn, db_folder_id);
            }
    }
}

void SqlLiteSync::GetTableNames()
{
    // get all of the table names from the open sqllite database
    // be sure to ignore "sqllitesync" table

    QSqlQuery select;

    select.prepare("select name from sqlite_master where name <> 'sqlitesync' and type='table'");

    if (select.exec())
    {
        while (select.next())
            m_table_names.append(select.value(0).toString());
    }
    else
    {
        qDebug() << "SqlLiteSync failed to get a list of tables.  Database will not sync with host.";
        return;
    }
}

void SqlLiteSync::PullUpdatedFiles()
{
    // look for files that have changed in the last 10 minutes prior to previous sync time
    // download those files
    // compare files to corresponding row
    // if record has not changed update it with the newer content
}

void SqlLiteSync::SendChangesToServer()
{
    for (QString tn : m_table_names)
    {
        // look for all records changed since last sync time
        QSqlQuery select;

        select.prepare(QString("select * from %1 where sync_updated > %2 or sync_updated is null ").arg(tn).arg(m_previous_sync_time.toSecsSinceEpoch()));
        if (select.exec())
        {
            qDebug() << "Found records in table: " << tn << select.executedQuery();

            while (select.next())
            {
                //qDebug() << "Writing record id: " << select.value(0).toString();

                // download those record files



                // compare files to see which is newer
                QJsonDocument jdoc;
                QFile file;
                file.setFileName( (m_temporary_folder + "/" + m_application_name) +"/" + tn + "/" + select.value(0).toString() + ".json" );
                file.open(QIODevice::ReadOnly | QIODevice::Text);
                jdoc = QJsonDocument::fromJson(file.readAll());
                file.close();

                QDateTime server_timestamp = QDateTime::fromSecsSinceEpoch(jdoc["sync_updated"].toString().toULongLong());
                QDateTime record_timestamp = QDateTime::fromSecsSinceEpoch(select.value("sync_updated").toULongLong());

                if (server_timestamp > record_timestamp)
                    qDebug() << "record id: " << select.value(0).toString() << " is newer ";

                // upload new file if the sever one hasn't changed
                if (false)
                {
                    QJsonObject recordobj;

                    for (int i = 0; i < select.record().count(); i++)
                        recordobj.insert(select.record().fieldName(i), select.record().value(i).toString());

                    QJsonDocument jdoc(recordobj);
                    QFile file;
                    file.setFileName( (m_temporary_folder + "/" + m_application_name) +"/" + tn + "/" + select.value(0).toString() + ".json" );
                    file.open(QIODevice::WriteOnly | QIODevice::Text);
                    file.write(jdoc.toJson());
                    file.close();


                }
            }
        }
        else
        {
            qDebug() << "SqlLiteSync failed to get a list of tables.  Database will not sync with host.";
            return;
        }
    }
}
