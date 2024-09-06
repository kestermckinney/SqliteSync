#include "googledriveservice.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QDateTime>

GoogleDriveService::GoogleDriveService(QObject *parent) : QObject(parent)
{
    m_google = new QOAuth2AuthorizationCodeFlow(this);

    // Set Scope or Permissions required from Google
    // List can be obtained from https://developers.google.com/identity/protocols/oauth2/scopes

    connect(m_google, &QOAuth2AuthorizationCodeFlow::tokenChanged, [=](const QString &refreshToken) {

        // Get refresh token and store it in file
        QJsonObject tokenObject;
        tokenObject["refresh_token"] = refreshToken;

        QJsonDocument refreshTokenDoc(tokenObject);

        QFile refreshTokenFileHandle("refresh_token.json");
        refreshTokenFileHandle.open(QFile::WriteOnly);
        refreshTokenFileHandle.write(refreshTokenDoc.toJson());
        refreshTokenFileHandle.close();

        qDebug() << "refreshed token";
    });

    // Load refresh token from file
    QFile refreshTokenFileHandle("refresh_token.json");
    if (refreshTokenFileHandle.exists())
    {
        refreshTokenFileHandle.open(QFile::ReadOnly);
        QJsonDocument refreshTokenDoc = QJsonDocument::fromJson(refreshTokenFileHandle.readAll());
        m_google->setRefreshToken(refreshTokenDoc.object()["refresh_token"].toString());
        refreshTokenFileHandle.close();
    }

    m_google->setScope("https://www.googleapis.com/auth/drive");

    connect(m_google, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [=](QUrl url) {
        QUrlQuery query(url);

        query.addQueryItem("prompt", "consent");      // Param required to get data everytime
        query.addQueryItem("access_type", "offline"); // Needed for Refresh Token (as AccessToken expires shortly)
        url.setQuery(query);
        QDesktopServices::openUrl(url);    
    });

    // Here the parameters from Google JSON are filled up
    // Attached screenshot of JSON file and Google Console

    m_google->setAuthorizationUrl(QUrl("https://accounts.google.com/o/oauth2/auth"));
    m_google->setAccessTokenUrl(QUrl("https://oauth2.googleapis.com/token"));
    m_google->setClientIdentifier("874435141224-gefl74mqdqd8bmoj9dq91bf77iopt4br.apps.googleusercontent.com");
    m_google->setXXClientIdentifierSharedKXXey("bbbbbbbbGOCSPX-79e5mmxRs-aPqBfrDYtJEbiTMJH8");

    // In my case, I have hardcoded 5476
    // This is set in Redirect URI in Google Developers Console of the app
    // Same can be seen in the downloaded JSON file

    auto replyHandler = new QOAuthHttpServerReplyHandler(5476, this);
    m_google->setReplyHandler(replyHandler);

    connect(m_google, &QOAuth2AuthorizationCodeFlow::granted, [=]() {
        qDebug() << __FUNCTION__ << __LINE__ << "Access Granted!";
    });

    auto reply = m_google->get(QUrl("https://www.googleapis.com/drive/v3/files?fields=*"));
    connect(reply, &QNetworkReply::finished, [reply]() {
        QJsonDocument jdoc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray jarray = jdoc["files"].toArray();

        for ( QJsonValue jv : jarray)
            qDebug() << jv["name"].toString() << " modified:  " << jv["modifiedTime"].toString();
    });
}

bool GoogleDriveService::checkAuthentication()
{
    QEventLoop eventLoop;
    QTimer timer;

    timer.setSingleShot(true);

    // Check if the token is expired or about to expire
    if (m_google->expirationAt() < QDateTime::currentDateTime().addSecs(60)) {
        // Token needs to be refreshed
        m_google->refreshAccessToken();

        // Create a temporary event loop to wait for the token refresh
        QObject::connect(m_google, &QOAuth2AuthorizationCodeFlow::tokenChanged, &eventLoop, &QEventLoop::quit);

        QObject::connect(&timer, &QTimer::timeout, [&eventLoop]() {
            qDebug() << "Request timed out.";
            eventLoop.quit();
        });

        // Wait for the token refresh to complete
        timer.start(30000); // 30 seconds timeout
        eventLoop.exec();
    }

    // Check if the token is valid
    if (m_google->status() != QAbstractOAuth::Status::Granted) {
        // Authentication is needed

        // Create a temporary event loop to wait for the token refresh
        QObject::connect(m_google, &QOAuth2AuthorizationCodeFlow::tokenChanged, &eventLoop, &QEventLoop::quit);

        QObject::connect(&timer, &QTimer::timeout, [&eventLoop]() {
            qDebug() << "Request timed out.";
            eventLoop.quit();
        });

        // Wait for the token refresh to complete
        timer.start(30000); // 30 seconds timeout
        eventLoop.exec();

        // if it wasn't granted exit
        if (m_google->status() != QAbstractOAuth::Status::Granted)
            return false;
    }

    return true;
}


GoogleDriveService::~GoogleDriveService()
{
    delete m_google;
}

// Call this function to prompt authentication
// and receive data from Google

void GoogleDriveService::authenticate()
{
    m_google->grant();
}

QString GoogleDriveService::folderExists(const QString& t_folder, const QString& t_parent_id)
{
    QEventLoop eventLoop;
    QTimer timer;

    timer.setSingleShot(true);

    // Create a network request to check if the folder exists
    QUrl url("https://www.googleapis.com/drive/v3/files");
    QUrlQuery query;

    if (t_parent_id.isEmpty())
        query.addQueryItem("q", QString("mimeType='application/vnd.google-apps.folder' and name='%1' and trashed=false").arg(t_folder));
    else
        query.addQueryItem("q", QString("mimeType='application/vnd.google-apps.folder' and name='%1' and '%2' in parents and trashed=false").arg(t_folder).arg(t_parent_id));

    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_google->token()).toUtf8());

    // Send the request and get the response
    QNetworkReply* reply = m_network_manager.get(request);

    // Create a temporary event loop to wait for the token refresh
    QObject::connect(&timer, &QTimer::timeout, [&eventLoop, reply]() {
        qDebug() << "Request timed out.";
        eventLoop.quit();
        reply->abort();
    });

    // Wait for the response
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);

    timer.start(30000); // 30 seconds timeout
    eventLoop.exec();

    // Check if the folder exists
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();

        if (response.object().contains("files") && response.object()["files"].isArray()) {
            QJsonArray files = response.object()["files"].toArray();
            if (files.size() > 0) {
                qDebug() << "Folder exists!";
                QString folderId = files.first().toObject()["id"].toString();
                return  folderId;
            } else {
                qDebug() << "Folder does not exist.";
                return QString();
            }
        } else {
            qDebug() << "Error checking folder existence.";
            return QString();
        }
    } else {
        qDebug() << "Error sending request:" << reply->errorString();
        return QString();
    }
}

QString GoogleDriveService::createFolder(const QString& t_folder, const QString& t_parent_id)
{
    QEventLoop eventLoop;
    QTimer timer;

    timer.setSingleShot(true);

    QString folder_id = folderExists(t_folder);

    if (!folder_id.isEmpty())
        return folder_id;

    // Create folder on Google Drive
    QUrl driveUrl("https://www.googleapis.com/drive/v3/files");

    // Create a JSON object with the folder metadata
    QJsonObject folderMetadata;
    folderMetadata["name"] = t_folder;
    folderMetadata["mimeType"] = "application/vnd.google-apps.folder";

    if (!t_parent_id.isEmpty())
    {
        folderMetadata["parents"] = QJsonArray() << t_parent_id;
    }

    // Convert the JSON object to a QByteArray
    QJsonDocument jsonDoc(folderMetadata);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkRequest request(driveUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + m_google->token().toUtf8());

    // Make the POST request
    QNetworkReply *reply = m_network_manager.post(request, jsonData);

    // Create a temporary event loop to wait for the token refresh
    QObject::connect(&timer, &QTimer::timeout, [&eventLoop, reply]() {
        qDebug() << "Request timed out.";
        eventLoop.quit();
        reply->abort();
    });

    // Wait for the response
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);

    timer.start(30000); // 30 seconds timeout
    eventLoop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        reply->deleteLater();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonObject jsonObj = jsonDoc.object();
        folder_id = jsonObj["id"].toString();
        qDebug() << "Created folder with ID:" << folder_id;

        return folder_id;
    }

    qDebug() << "Folder Creation Failed.";
    return folder_id;
}

QString GoogleDriveService::fileExists(const QString& t_file, const QString& t_parent_id)
{
    QEventLoop eventLoop;
    QTimer timer;

    timer.setSingleShot(true);

    qDebug() << "searing for file " << t_file << " in parent " << t_parent_id;

    // Create a network request to check if the folder exists
    QUrl url("https://www.googleapis.com/drive/v3/files");
    QUrlQuery query;

    if (t_parent_id.isEmpty())
        query.addQueryItem("q", QString("name='%1' and trashed=false").arg(t_file));
    else
        query.addQueryItem("q", QString("name='%1' and '%2' in parents and trashed=false").arg(t_file).arg(t_parent_id));

    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_google->token()).toUtf8());

    // Send the request and get the response
    QNetworkReply* reply = m_network_manager.get(request);

    QObject::connect(&timer, &QTimer::timeout, [&eventLoop, reply]() {
        qDebug() << "Request timed out.";
        eventLoop.quit();
        reply->abort();
    });

    // Wait for the response
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);

    timer.start(30000); // 30 seconds timeout
    eventLoop.exec();

    // Check if the file exists
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument response = QJsonDocument::fromJson(reply->readAll());

        reply->deleteLater();

        if (response.object().contains("files") && response.object()["files"].isArray()) {
            QJsonArray files = response.object()["files"].toArray();
            if (files.size() > 0) {
                qDebug() << "File exists!";
                QString folderId = files.first().toObject()["id"].toString();
                return  folderId;
            } else {
                qDebug() << "File does not exist.";
                return QString();
            }
        } else {
            qDebug() << "Error checking file existence.";
            return QString();
        }
    } else {
        qDebug() << "Error sending request:" << reply->errorString();
        return QString();
    }
}

QString GoogleDriveService::uploadFile(const QString& t_source_file, const QString& t_dest_file, const QString& t_parent_id)
{
    QEventLoop eventLoop;
    QTimer timer;

    timer.setSingleShot(true);

    QFile file(t_source_file);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Unable to open file:" << t_source_file;
        return QString();
    }

    QString file_id = fileExists(t_dest_file, t_parent_id);

    if (!file_id.isEmpty())
        deleteFile(file_id);

    QUrl url(QString("https://www.googleapis.com/upload/drive/v3/files/?uploadType=multipart"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/related; boundary=foo_bar_baz");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_google->token()).toUtf8());

    QByteArray metadata;
    metadata.append("--foo_bar_baz\r\n");
    metadata.append("Content-Type: application/json; charset=UTF-8\r\n\r\n");

    QJsonObject fileMetadata;
    fileMetadata["name"] = t_dest_file;

    if (!t_parent_id.isEmpty())
    {
        fileMetadata["parents"] = QJsonArray() << t_parent_id;
    }

    QJsonDocument doc(fileMetadata);
    metadata.append(doc.toJson());
    metadata.append("\r\n");

    QByteArray fileData;
    fileData.append("--foo_bar_baz\r\n");
    fileData.append("Content-Type: application/octet-stream\r\n\r\n");
    fileData.append(file.readAll());
    fileData.append("\r\n");
    fileData.append("--foo_bar_baz--");

    QByteArray multipartData = metadata + fileData;

    QNetworkReply *reply = m_network_manager.post(request, multipartData);

    QObject::connect(reply, &QNetworkReply::finished, [&eventLoop]() {
        eventLoop.quit();
    });

    QObject::connect(&timer, &QTimer::timeout, [&eventLoop, reply]() {
        qDebug() << "Request timed out.";
        eventLoop.quit();
        reply->abort();
    });

    timer.start(30000); // 30 seconds timeout
    eventLoop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonObject jsonObj = jsonDoc.object();
        QString fileId = jsonObj["id"].toString();
        qDebug() << "Uploaded file with ID:" << fileId;
        reply->deleteLater();
        return fileId;
    } else {
        qDebug() << "Error occurred:" << reply->errorString();
        qDebug() << reply->readAll();
        reply->deleteLater();
        return QString();
    }
}

bool GoogleDriveService::deleteFile(const QString& t_file_id)
{
    QEventLoop eventLoop;
    QTimer timer;
    timer.setSingleShot(true);

    QUrl url(QString("https://www.googleapis.com/drive/v3/files/%1").arg(t_file_id));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_google->token()).toUtf8());

    QNetworkReply* reply = m_network_manager.deleteResource(request);

    QObject::connect(reply, &QNetworkReply::finished, [&eventLoop]() {
        eventLoop.quit();
    });

    QObject::connect(&timer, &QTimer::timeout, [&eventLoop, reply]() {
        qDebug() << "Request timed out.";
        eventLoop.quit();
        reply->abort();
    });

    timer.start(30000); // 30 seconds timeout
    eventLoop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Deleted file with ID:" << t_file_id;
        reply->deleteLater();
        return true;
    } else {
        qDebug() << "Error occurred while deleting:" << reply->errorString();
        reply->deleteLater();
        return false;
    }
}

QString GoogleDriveService::getFileNameById(const QString& t_file_id)
{
    QEventLoop eventLoop;
    QTimer timer;
    timer.setSingleShot(true);

    QUrl url(QString("https://www.googleapis.com/drive/v3/files/%1?fields=name").arg(t_file_id));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_google->token()).toUtf8());

    QNetworkReply* reply = m_network_manager.get(request);

    QObject::connect(reply, &QNetworkReply::finished, [&eventLoop]() {
        eventLoop.quit();
    });

    QObject::connect(&timer, &QTimer::timeout, [&eventLoop, reply]() {
        qDebug() << "Request timed out.";
        eventLoop.quit();
        reply->abort();
    });

    timer.start(30000); // 30 seconds timeout
    eventLoop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonObject jsonObj = jsonDoc.object();
        QString fileName = jsonObj["name"].toString();
        reply->deleteLater();
        return fileName;
    } else {
        qDebug() << "Error occurred while getting file name:" << reply->errorString();
        reply->deleteLater();
        return QString();
    }
}

bool GoogleDriveService::downloadFile(const QString& t_file_id, const QString& t_dest_folder)
{
    QEventLoop eventLoop;
    QTimer timer;
    timer.setSingleShot(true);

    QString fileName = getFileNameById(t_file_id);

    QUrl url(QString("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(t_file_id));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_google->token()).toUtf8());

    QNetworkReply* reply = m_network_manager.get(request);

    QObject::connect(reply, &QNetworkReply::finished, [&eventLoop]() {
        eventLoop.quit();
    });

    QObject::connect(&timer, &QTimer::timeout, [&eventLoop, reply]() {
        qDebug() << "Request timed out.";
        eventLoop.quit();
        reply->abort();
    });

    timer.start(30000); // 30 seconds timeout
    eventLoop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QFile file(QString("%1/%2").arg(t_dest_folder).arg(fileName) );
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            qDebug() << "Downloaded file to:" << fileName;
            reply->deleteLater();
            return true;
        } else {
            qDebug() << "Unable to open file for writing:" << fileName;
            reply->deleteLater();
            return false;
        }
    } else {
        qDebug() << "Error occurred while downloading:" << reply->errorString();
        reply->deleteLater();
        return false;
    }
}

QList<QString> GoogleDriveService::listFilesNewerThan(const QString& t_parent_id, const QDateTime& t_date)
{
    QEventLoop eventLoop;
    QTimer timer;
    timer.setSingleShot(true);

    QString dateString = t_date.toString(Qt::ISODate);
    QString query = QString("'%1' in parents and modifiedTime > '%2' and trashed=false ").arg(t_parent_id, dateString);
    QUrl url(QString("https://www.googleapis.com/drive/v3/files?q=%1").arg(query));
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_google->token()).toUtf8());

    QNetworkReply* reply = m_network_manager.get(request);

    QObject::connect(reply, &QNetworkReply::finished, [&eventLoop]() {
        eventLoop.quit();
    });

    QObject::connect(&timer, &QTimer::timeout, [&eventLoop, reply]() {
        qDebug() << "Request timed out.";
        eventLoop.quit();
        reply->abort();
    });

    timer.start(30000); // 30 seconds timeout
    eventLoop.exec();

    QList<QString> fileIds;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        QJsonObject jsonObj = jsonDoc.object();
        QJsonArray files = jsonObj["files"].toArray();

        for (const QJsonValue& value : files) {
            QJsonObject fileObj = value.toObject();
            fileIds.append(fileObj["id"].toString());

            qDebug() << "file name " << fileObj["name"].toString();
        }

        reply->deleteLater();
    } else {
        qDebug() << "Error occurred while listing files:" << reply->errorString();
        reply->deleteLater();
    }

    return fileIds;
}



/*
void GoogleDriveService::click_refresh_token()
{
    m_google->setRefreshToken(m_refreshToken);
    m_google->refreshAccessToken();
}
*/
