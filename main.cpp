#include <QApplication>
#include "core/auth/ApiClient.h"
#include "core/auth/AuthManager.h"
#include "ui/login/LoginWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("JournalApp");
    QApplication::setOrganizationName("TopAcademy");

    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));

    ApiClient apiClient;
    AuthManager authManager(&apiClient);

    LoginWindow loginWindow(&authManager, &apiClient);
    loginWindow.show();

    loginWindow.loadSavedCredentials();

    authManager.tryAutoLogin();

    return app.exec();
}