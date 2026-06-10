#include "LoginWindow.h"
#include "ui_loginwindow.h"
#include "../main/MainWindow.h"
#include "../../core/auth/apiclient.h"
#include "../../core/auth/authmanager.h"
#include "../../core/auth/AuthModels.h"
#include <QMessageBox>

LoginWindow::LoginWindow(AuthManager *authManager, ApiClient *apiClient, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LoginWindow)
    , m_authManager(authManager)
    , m_apiClient(apiClient)
    , m_mainWindow(nullptr)
{
    ui->setupUi(this);

    connect(m_authManager, &AuthManager::loginSuccess,
            this, &LoginWindow::onLoginSuccess);
    connect(m_authManager, &AuthManager::loginFailed,
            this, &LoginWindow::onLoginFailed);
    connect(ui->loginButton, &QPushButton::clicked,
            this, &LoginWindow::onLoginClicked);
    connect(ui->passwordEdit, &QLineEdit::returnPressed,
            this, &LoginWindow::onLoginClicked);
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::onLoginClicked()
{
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        ui->statusLabel->setText("Заполните все поля");
        ui->statusLabel->setStyleSheet("color: #ff6b6b; font-size: 12px;");
        return;
    }

    // Сохран если чекбокс активен
    if (ui->rememberCheck->isChecked()) {
        m_authManager->saveCredentials(username, password);
    } else {
        m_authManager->clearCredentials();
    }

    ui->statusLabel->setText("Выполняется вход...");
    ui->statusLabel->setStyleSheet("color: #2196f3; font-size: 12px;");
    ui->loginButton->setEnabled(false);

    m_authManager->login(username, password);
}

void LoginWindow::onLoginSuccess(const AuthData &data)
{
    ui->statusLabel->setText("Вход выполнен!");
    ui->statusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");

    QMessageBox::information(this, "Успех",
                             QString("Добро пожаловать!\nРоль: %1\nГород: %2")
                                 .arg(data.user_role, data.city_data.name));

    m_mainWindow = new MainWindow(m_authManager, m_apiClient, nullptr);
    m_mainWindow->show();

    hide();
}

void LoginWindow::onLoginFailed(const QString &error)
{
    ui->statusLabel->setText(error);
    ui->statusLabel->setStyleSheet("color: #f44336; font-size: 12px;");
    ui->loginButton->setEnabled(true);
}

void LoginWindow::loadSavedCredentials()
{
    QSettings settings("TopAcademy", "JournalApp");
    if (settings.value("credentials/remember", false).toBool()) {
        ui->usernameEdit->setText(settings.value("credentials/username").toString());
        ui->passwordEdit->setText(settings.value("credentials/password").toString());
        ui->rememberCheck->setChecked(true);
    }
}