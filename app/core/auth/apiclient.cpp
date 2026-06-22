#include "ApiClient.h"
#include <QUrl>
#include <QDebug>

const QString BASE_URL = "https://msapi.top-academy.ru/api/v2";

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_baseUrl(BASE_URL)
{
}

void ApiClient::setAuthToken(const QString &token)
{
    m_authToken = token;
}

void ApiClient::clearAuthToken()
{
    m_authToken.clear();
}

QNetworkRequest ApiClient::createRequest(const QString &endpoint) const
{
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Origin", "https://journal.top-academy.ru");
    request.setRawHeader("Referer", "https://journal.top-academy.ru/");
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("User-Agent", "JournalApp/1.0 (Qt)");

    if (!m_authToken.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }

    return request;
}

void ApiClient::handleReply(QNetworkReply *reply,
                            std::function<void(QJsonObject)> onSuccess,
                            std::function<void(QString)> onError)
{
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                if (onSuccess) onSuccess(doc.object());
            } else {
                if (onError) onError("Ошибка парсинга JSON: " + parseError.errorString());
            }
        } else {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray responseData = reply->readAll();
            QString errorMsg;

            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            if (doc.isObject() && doc.object().contains("message")) {
                errorMsg = doc.object()["message"].toString();
            } else if (doc.isObject() && doc.object().contains("error")) {
                errorMsg = doc.object()["error"].toString();
            } else {
                errorMsg = QString("HTTP %1: %2").arg(statusCode).arg(reply->errorString());
            }

            if (onError) onError(errorMsg);
        }
    });
}

void ApiClient::get(const QString &endpoint,
                    std::function<void(QJsonObject)> onSuccess,
                    std::function<void(QString)> onError)
{
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_manager->get(request);
    handleReply(reply, onSuccess, onError);
}

void ApiClient::post(const QString &endpoint,
                     const QJsonObject &body,
                     std::function<void(QJsonObject)> onSuccess,
                     std::function<void(QString)> onError)
{
    QNetworkRequest request = createRequest(endpoint);
    QByteArray data = QJsonDocument(body).toJson();
    QNetworkReply *reply = m_manager->post(request, data);
    handleReply(reply, onSuccess, onError);
}