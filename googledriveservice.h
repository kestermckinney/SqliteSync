#ifndef GoogleDriveService_H
#define GoogleDriveService_H

#include <QObject>
#include <QOAuth2AuthorizationCodeFlow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QAbstractOAuth>
#include <QUrl>
#include <QUrlQuery>
#include <QOAuthHttpServerReplyHandler>
#include <QDesktopServices>

class GoogleDriveService : public QObject
{
    Q_OBJECT
public:
    explicit GoogleDriveService(QObject *parent = nullptr);
    ~GoogleDriveService();

    QString createFolder(const QString& t_folder, const QString& t_parent_id = QString());
    QString folderExists(const QString& t_folder, const QString& t_parent_id = QString());
    QString fileExists(const QString& t_file, const QString& t_parent_id = QString());
    QString uploadFile(const QString& t_source_file, const QString& t_dest_file, const QString& t_parent_id = QString());
    bool deleteFile(const QString& t_file_id);
    bool downloadFile(const QString& t_file_id, const QString& t_dest_folder);
    QList<QString> listFilesNewerThan(const QString& t_parent_id, const QDateTime& t_date);
    QString getFileNameById(const QString& t_file_id);

    void authenticate();
    bool checkAuthentication();
    //Q_INVOKABLE void click_refresh_token();


private:
    QOAuth2AuthorizationCodeFlow* m_google;

    QNetworkAccessManager m_network_manager;
};

#endif // GoogleDriveService_H
