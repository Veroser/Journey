#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QDateTime>
#include "AuthModels.h"

class ApiClient;

class AuthManager : public QObject
{
    Q_OBJECT

public:
    explicit AuthManager(ApiClient *apiClient, QObject *parent = nullptr);
    ~AuthManager() override = default;

    void login(const QString &username, const QString &password);
    void tryAutoLogin();
    void logout();
    void refreshToken();

    bool isAuthenticated() const;
    QString accessToken() const;
    const AuthData& authData() const;

    void saveCredentials(const QString &username, const QString &password);
    void clearCredentials();
    QString getSavedUsername() const;
    QString getSavedPassword() const;
    QString getJwtToken() const;

signals:
    void loginSuccess(const AuthData &data);
    void loginFailed(const QString &error);
    void tokenRefreshed();
    void loggedOut();

private slots:
    void onRefreshTimeout();

private:
    ApiClient *m_apiClient;
    QSettings m_settings;
    AuthData m_authData;
    QTimer m_refreshTimer;

    void processLoginResponse(const QJsonObject &json);
    void processRefreshResponse(const QJsonObject &json);
    void saveToSettings();
    void loadFromSettings();
    void clearSettings();
    void scheduleAutoRefresh();
    void fetchUserProfile();

    QJsonObject decodeJwtPayload(const QString &token) const;
};

#endif // AUTHMANAGER_H