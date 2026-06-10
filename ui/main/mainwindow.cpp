#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ScheduleWidget.h"
#include "../../core/schedule/ScheduleManager.h"
#include "../../core/auth/authmanager.h"
#include "../../core/auth/AuthModels.h"
#include "../../core/auth/apiclient.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QMessageBox>
#include <QDate>
#include <QTimer>

MainWindow::MainWindow(AuthManager *authManager, ApiClient *apiClient, QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_authManager(authManager)
{
    ui->setupUi(this);

    m_scheduleManager = new ScheduleManager(apiClient, this);

    connect(m_scheduleManager, &ScheduleManager::homeworkUpdated,
            this, &MainWindow::updateHomeworkLabels);
    updateHomeworkLabels();

    m_scheduleWidget = new ScheduleWidget(m_scheduleManager, this);
    ui->scheduleTab->layout()->addWidget(m_scheduleWidget);

    connect(m_scheduleManager, &ScheduleManager::syncStarted, this, [this]() {
        qDebug() << "Синхронизация началась";
        ui->syncButton->setEnabled(false);
        ui->syncButton->setText("...Синхронизация...");
    });

    connect(m_scheduleManager, &ScheduleManager::syncFinished, this, [this]() {
        qDebug() << "Синхронизация завершена";
        ui->syncButton->setEnabled(true);
        ui->syncButton->setText("Синхронизировать");
        QMessageBox::information(this, "Синхронизация", "Расписание успешно обновлено!");

        m_scheduleManager->loadDaySchedule(QDate::currentDate());
    });

    connect(m_scheduleManager, &ScheduleManager::syncFailed, this, [this](const QString &error) {
        qWarning() << "Ошибка синхронизации:" << error;
        ui->syncButton->setEnabled(true);
        ui->syncButton->setText("Синхронизировать");
        QMessageBox::warning(this, "Ошибка синхронизации", error);
    });

    // Кнопки
    connect(ui->logoutButton, &QPushButton::clicked, this, &MainWindow::onLogout);
    connect(ui->syncButton, &QPushButton::clicked, this, &MainWindow::onSyncClicked);

    // Инфа о пользователе
    const AuthData &authData = m_authManager->authData();
    QString nameUser = authData.display_name;
    if (nameUser.isEmpty()) {
        nameUser = authData.user_role + " #" + QString::number(authData.user_id);
    }
    ui->userLabel->setText(nameUser);

    ui->mainTabWidget->setCurrentIndex(0);

    applyStyles();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onLogout()
{
    m_authManager->logout();
    close();
}

void MainWindow::applyStyles()
{
    // Забей, заглушка что бы эта залупа работала
}

void MainWindow::onSyncClicked()
{
    QString jwtToken = m_authManager->getJwtToken();

    if (jwtToken.isEmpty()) {
        qWarning() << "Нет JWT токена для синхронизации";
        QMessageBox::warning(this, "Ошибка", "Отсутствует токен авторизации");
        return;
    }

    m_scheduleManager->syncWithParser(jwtToken);
}

void MainWindow::updateHomeworkLabels()
{
    ui->homeworkTotalLabel->setText(
        QString("Всего: %1").arg(m_scheduleManager->homeworkAll()));
    ui->homeworkDoneLabel->setText(
        QString("Сдано: %1").arg(m_scheduleManager->homeworkDone()));
    ui->homeworkPendingLabel->setText(
        QString("На проверке: %1").arg(m_scheduleManager->homeworkUnderInspection()));
    ui->homeworkOverdueLabel->setText(
        QString("Просрочено: %1").arg(m_scheduleManager->homeworkOverdue()));
    ui->homeworkCurrentLabel->setText(
        QString("Текущие: %1 задания").arg(m_scheduleManager->homeworkCurrent()));
}