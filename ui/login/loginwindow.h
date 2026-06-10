#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWindow;
}
QT_END_NAMESPACE

class AuthManager;
class ApiClient;
class MainWindow;
struct AuthData;

class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginWindow(AuthManager *authManager, ApiClient *apiClient, QWidget *parent = nullptr);
    ~LoginWindow() override;

    void loadSavedCredentials();

private slots:
    void onLoginClicked();
    void onLoginSuccess(const AuthData &data);
    void onLoginFailed(const QString &error);

private:
    Ui::LoginWindow *ui;
    AuthManager *m_authManager;
    ApiClient *m_apiClient;
    MainWindow *m_mainWindow;
    QString m_savedUsername;
    QString m_savedPassword;
};

#endif // LOGINWINDOW_H
