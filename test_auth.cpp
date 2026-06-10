#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>
#include <QCommandLineParser>
#include "ApiClient.h"
#include "AuthManager.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("JournalAuthTest");

    QCommandLineParser parser;
    parser.setApplicationDescription("Тест авторизации Journal API");
    parser.addHelpOption();

    QCommandLineOption userOption("u", "Логин", "username");
    QCommandLineOption passOption("p", "Пароль", "password");
    QCommandLineOption autoOption("auto", "Попробовать автовход с сохранённым токеном");

    parser.addOption(userOption);
    parser.addOption(passOption);
    parser.addOption(autoOption);
    parser.process(app);

    // Создаём API клиент и менеджер авторизации
    ApiClient apiClient;
    AuthManager authManager(&apiClient);

    QTextStream out(stdout);
    QTextStream err(stderr);

    int exitCode = 0;

    // Обработчики сигналов
    QObject::connect(&authManager, &AuthManager::loginSuccess,
                     [&](const AuthData &data) {
                         out << "\n========================================" << Qt::endl;
                         out << "✅ АВТОРИЗАЦИЯ УСПЕШНА" << Qt::endl;
                         out << "========================================" << Qt::endl;
                         out << "User ID:     " << data.user_id << Qt::endl;
                         out << "Роль:        " << data.user_role << Qt::endl;
                         out << "Город:       " << data.city_data.name << Qt::endl;
                         out << "Префикс:     " << data.city_data.prefix << Qt::endl;
                         out << "Часовой пояс: " << data.city_data.timezone_name << Qt::endl;
                         out << "Токен истекает: " << QDateTime::fromSecsSinceEpoch(data.expires_in_access).toString("dd.MM.yyyy hh:mm:ss") << Qt::endl;
                         out << "Access token:  " << data.access_token.left(50) << "..." << Qt::endl;
                         out << "Refresh token: " << data.refresh_token.left(50) << "..." << Qt::endl;
                         QCoreApplication::quit();
                     });

    QObject::connect(&authManager, &AuthManager::loginFailed,
                     [&](const QString &error) {
                         err << "❌ Ошибка: " << error << Qt::endl;
                         exitCode = 1;
                         QCoreApplication::exit(1);
                     });

    QObject::connect(&authManager, &AuthManager::loggedOut,
                     [&]() {
                         out << "Выполнен выход" << Qt::endl;
                         QCoreApplication::quit();
                     });

    // Логика запуска
    if (parser.isSet(autoOption)) {
        out << "Пробуем автовход..." << Qt::endl;
        authManager.tryAutoLogin();
    }
    else if (parser.isSet(userOption) && parser.isSet(passOption)) {
        QString username = parser.value(userOption);
        QString password = parser.value(passOption);
        out << "Выполняем вход для " << username << "..." << Qt::endl;
        authManager.login(username, password);
    }
    else {
        // Интерактивный режим
        out << "=== Тест авторизации Journal API ===" << Qt::endl;
        out << "Логин: ";
        out.flush();

        QTextStream in(stdin);
        QString username = in.readLine();

        out << "Пароль: ";
        out.flush();

        QString password;
        // Скрытый ввод пароля (без echo)
        password = in.readLine();

        authManager.login(username, password);
    }

    return app.exec();
}