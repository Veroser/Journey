#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <functional>

class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);
    ~ApiClient() override = default;

    void setAuthToken(const QString &token);
    void clearAuthToken();

    void get(const QString &endpoint,
             std::function<void(QJsonObject)> onSuccess,
             std::function<void(QString)> onError = nullptr);

    void post(const QString &endpoint,
              const QJsonObject &body,
              std::function<void(QJsonObject)> onSuccess,
              std::function<void(QString)> onError = nullptr);

private:
    QNetworkAccessManager *m_manager;
    QString m_baseUrl;
    QString m_authToken;

    QNetworkRequest createRequest(const QString &endpoint) const;
    void handleReply(QNetworkReply *reply,
                     std::function<void(QJsonObject)> onSuccess,
                     std::function<void(QString)> onError);
};

#endif // APICLIENT_H