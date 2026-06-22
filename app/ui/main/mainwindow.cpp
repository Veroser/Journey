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

    ui->ratingGroupValue->setStyleSheet("font-size: 32px; font-weight: bold; color: #4CAF50;");
    ui->ratingGroupLabel->setStyleSheet("font-size: 12px; color: #999;");
    ui->ratingGroupTotal->setStyleSheet("font-size: 12px; color: #bbb;");

    ui->ratingStreamValue->setStyleSheet("font-size: 32px; font-weight: bold; color: #2196F3;");
    ui->ratingStreamLabel->setStyleSheet("font-size: 12px; color: #999;");
    ui->ratingStreamTotal->setStyleSheet("font-size: 12px; color: #bbb;");

    ui->ratingBlockTitle->setStyleSheet("font-size: 14px; font-weight: 600; color: #333;");

    connect(m_scheduleManager, &ScheduleManager::homeworkUpdated,
            this, &MainWindow::updateHomeworkLabels);
    updateHomeworkLabels();

    m_scheduleWidget = new ScheduleWidget(m_scheduleManager, this);
    ui->scheduleTab->layout()->addWidget(m_scheduleWidget);

    QSettings settings;
    m_isDarkTheme = settings.value("theme/dark", false).toBool();
    applyTheme();

    updateMetricsDisplay();

    // Рейтинг при старте
    m_scheduleManager->loadRatingFromDb();
    if (m_scheduleManager->groupTotal() > 0) {
        ui->ratingGroupValue->setText(QString("#%1").arg(m_scheduleManager->groupPosition()));
        ui->ratingGroupTotal->setText(QString("из %1").arg(m_scheduleManager->groupTotal()));
        ui->ratingStreamValue->setText(QString("#%1").arg(m_scheduleManager->streamPosition()));
        ui->ratingStreamTotal->setText(QString("из %1").arg(m_scheduleManager->streamTotal()));
        updateLeaderboard();
    }
    m_scheduleManager->loadFeedbackFromDb();
    connect(m_scheduleManager, &ScheduleManager::feedbackUpdated, this, [this]() {
        updateFeedbackList();
    });
    // Инфа о пользователе
    m_scheduleManager->loadUserInfoFromDb();
    if (!m_scheduleManager->userName().isEmpty()) {
        ui->userLabel->setText(m_scheduleManager->userName());
        ui->coinsValue->setText(QString::number(m_scheduleManager->userCoins()));
        ui->gemsValue->setText(QString::number(m_scheduleManager->userGems()));
    }

    connect(m_scheduleManager, &ScheduleManager::metricsUpdated,
            this, &MainWindow::updateMetricsDisplay);

    connect(m_scheduleManager, &ScheduleManager::syncStarted, this, [this]() {
        qDebug() << "Синхронизация началась";
        ui->syncButton->setEnabled(false);
        ui->syncButton->setText("...Синхронизация...");
    });

    connect(m_scheduleManager, &ScheduleManager::syncFinished, this, [this]() {
        qDebug() << "Синхронизация завершена";
        ui->syncButton->setEnabled(true);
        ui->syncButton->setText("Синхронизировать");
        m_scheduleManager->loadDaySchedule(QDate::currentDate());
    });

    connect(m_scheduleManager, &ScheduleManager::syncFailed, this, [this](const QString &error) {
        qWarning() << "Ошибка синхронизации:" << error;
        ui->syncButton->setEnabled(true);
        ui->syncButton->setText("Синхронизировать");
        QMessageBox::warning(this, "Ошибка синхронизации", error);
    });

    connect(m_scheduleManager, &ScheduleManager::syncStarted, this, [this]() {
        statusBar()->showMessage("Синхронизация...");
    });

    connect(m_scheduleManager, &ScheduleManager::syncFinished, this, [this]() {
        statusBar()->showMessage("Синхронизация завершена", 5000);
    });

    connect(m_scheduleManager, &ScheduleManager::syncFailed, this, [this](const QString &error) {
        statusBar()->showMessage("Ошибка синхронизации: " + error, 0);
    });

    connect(m_scheduleManager, &ScheduleManager::parserProgress, this, [this](int percent) {
        statusBar()->showMessage(QString("Синхронизация: %1%").arg(percent));
    });

    connect(m_scheduleManager, &ScheduleManager::syncStatusChanged, this, [this](const QString &status) {
        statusBar()->showMessage(status);
    });

    connect(m_scheduleManager, &ScheduleManager::ratingUpdated, this, [this]() {
        ui->ratingGroupValue->setText(QString("#%1").arg(m_scheduleManager->groupPosition()));
        ui->ratingGroupTotal->setText(QString("из %1").arg(m_scheduleManager->groupTotal()));
        ui->ratingStreamValue->setText(QString("#%1").arg(m_scheduleManager->streamPosition()));
        ui->ratingStreamTotal->setText(QString("из %1").arg(m_scheduleManager->streamTotal()));
        updateLeaderboard();
    });

    connect(m_scheduleManager, &ScheduleManager::userInfoUpdated, this, [this]() {
        ui->userLabel->setText(m_scheduleManager->userName());
        ui->coinsValue->setText(QString::number(m_scheduleManager->userCoins()));
        ui->gemsValue->setText(QString::number(m_scheduleManager->userGems()));
    });

    connect(ui->leaderboardGroupBtn, &QPushButton::clicked, this, [this]() {
        ui->leaderboardGroupBtn->setChecked(true);
        ui->leaderboardStreamBtn->setChecked(false);
        ui->leaderboardStack->setCurrentIndex(0);
    });

    connect(ui->leaderboardStreamBtn, &QPushButton::clicked, this, [this]() {
        ui->leaderboardStreamBtn->setChecked(true);
        ui->leaderboardGroupBtn->setChecked(false);
        ui->leaderboardStack->setCurrentIndex(1);
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

    // Кнопка переключения темы
    QPushButton *themeButton = new QPushButton(m_isDarkTheme ? "☀️" : "🌙", this);
    themeButton->setMaximumWidth(40);
    themeButton->setStyleSheet("QPushButton { border: none; font-size: 18px; background: transparent; }");
    ui->topBar->layout()->addWidget(themeButton);

    connect(themeButton, &QPushButton::clicked, this, [this, themeButton]() {
        m_isDarkTheme = !m_isDarkTheme;
        themeButton->setText(m_isDarkTheme ? "☀️" : "🌙");

        QSettings settings;
        settings.setValue("theme/dark", m_isDarkTheme);

        applyTheme();
    });
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
    // Костыль
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

    ui->homeworkTotalLabel->setStyleSheet(
        "font-size: 11px; color: #666; background-color: #f5f5f5; border-radius: 4px; padding: 6px 10px;");
    ui->homeworkDoneLabel->setStyleSheet(
        "font-size: 11px; color: #4CAF50; background-color: #E8F5E9; border-radius: 4px; padding: 6px 10px;");
    ui->homeworkPendingLabel->setStyleSheet(
        "font-size: 11px; color: #FF9800; background-color: #FFF3E0; border-radius: 4px; padding: 6px 10px;");
    ui->homeworkOverdueLabel->setStyleSheet(
        "font-size: 11px; color: #F44336; background-color: #FFEBEE; border-radius: 4px; padding: 6px 10px;");
    ui->homeworkCurrentLabel->setStyleSheet(
        "font-size: 16px; font-weight: bold; color: #2196F3; background-color: #E3F2FD; border-radius: 8px; padding: 12px 16px;");
    ui->homeworkCurrentLabel->setAlignment(Qt::AlignCenter);
}

void MainWindow::updateMetricsDisplay()
{
    int grade = m_scheduleManager->gradesPercent();
    ui->gradesPercentLabel->setText(QString::number(grade));
    ui->gradesProgressBar->setMinimum(0);
    ui->gradesProgressBar->setMaximum(5);
    ui->gradesProgressBar->setValue(grade);

    int attendancePct = m_scheduleManager->attendancePercent();
    ui->attendancePercentLabel->setText(QString("%1%").arg(attendancePct));
    ui->attendanceProgressBar->setMinimum(0);
    ui->attendanceProgressBar->setMaximum(100);
    ui->attendanceProgressBar->setValue(attendancePct);

    ui->coinsValue->setText(QString::number(m_scheduleManager->coins()));
    ui->gemsValue->setText(QString::number(m_scheduleManager->gems()));
}

void MainWindow::updateLeaderboard()
{
    // Группа
    ui->leaderboardGroupList->clear();
    auto group = m_scheduleManager->groupRating();
    for (const auto &s : group) {
        QString text;
        if (s.position <= 3) {
            QString medal = s.position == 1 ? "🥇" : s.position == 2 ? "🥈" : "🥉";
            text = QString("%1 %2 — %3").arg(medal, s.fullName).arg(s.amount);
        } else {
            text = QString("%1. %2 — %3").arg(s.position).arg(s.fullName).arg(s.amount);
        }

        if (s.isCurrentUser) {
            text = "▶ " + text;
        }

        ui->leaderboardGroupList->addItem(text);
    }

    // Поток
    ui->leaderboardStreamList->clear();
    auto stream = m_scheduleManager->streamRating();
    for (const auto &s : stream) {
        QString text;
        if (s.position <= 3) {
            QString medal = s.position == 1 ? "🥇" : s.position == 2 ? "🥈" : "🥉";
            text = QString("%1 %2 — %3").arg(medal, s.fullName).arg(s.amount);
        } else {
            text = QString("%1. %2 — %3").arg(s.position).arg(s.fullName).arg(s.amount);
        }

        if (s.isCurrentUser) {
            text = "▶ " + text;
        }

        ui->leaderboardStreamList->addItem(text);
    }
}

void MainWindow::updateFeedbackList()
{
    ui->feedbackList->clear();

    QVector<ScheduleManager::FeedbackInfo> feedbackItems = m_scheduleManager->feedback();

    for (const auto &item : feedbackItems) {
        QString text = QString("%1\n%2\nПредмет: %3 | Преподаватель: %4")
                           .arg(item.date, item.message, item.subject, item.teacher);
        ui->feedbackList->addItem(text);
    }

    if (feedbackItems.isEmpty()) {
        ui->feedbackList->addItem("Нет отзывов");
    }

    ui->feedbackList->show();
}

void MainWindow::applyTheme()
{
    QString theme;

    if (m_isDarkTheme) {
        theme = R"(
            QMainWindow, QWidget#centralwidget, QWidget#homeTab, QWidget#scheduleTab,
            QWidget#gradesTab, QWidget#paymentTab, QWidget#feedbackTab {
                background-color: #1a1a2e;
            }

            QWidget#topBar {
                background-color: #16213e;
                border-bottom: 1px solid #0f3460;
            }

            QLabel {
                color: #e0e0e0;
                background: transparent;
            }

            QLabel#appTitle {
                font-size: 18px;
                font-weight: bold;
                color: #e0e0e0;
            }

            QLabel#welcomeLabel {
                font-size: 24px;
                font-weight: bold;
                color: #e0e0e0;
            }

            QLabel#userLabel {
                color: #888;
                font-size: 12px;
            }

            QLabel#coinsValue {
                font-size: 15px;
                font-weight: bold;
                color: #FFD54F;
            }

            QLabel#gemsValue {
                font-size: 15px;
                font-weight: bold;
                color: #81C784;
            }

            QPushButton {
                background-color: #16213e;
                border: 1px solid #0f3460;
                border-radius: 4px;
                padding: 6px 12px;
                color: #e0e0e0;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: #0f3460;
            }
            QPushButton:checked {
                background-color: #4CAF50;
                color: white;
            }

            QPushButton#syncButton {
                background-color: #4CAF50;
                color: white;
                border: none;
            }
            QPushButton#syncButton:hover {
                background-color: #45a049;
            }

            QPushButton#logoutButton {
                background-color: #ff6b6b;
                color: white;
                border: none;
            }
            QPushButton#logoutButton:hover {
                background-color: #ff5252;
            }

            QTabWidget::pane {
                border: 1px solid #0f3460;
                background-color: #1a1a2e;
            }
            QTabBar::tab {
                background-color: #16213e;
                color: #888;
                border: 1px solid #0f3460;
                padding: 10px 20px;
                margin-right: 2px;
            }
            QTabBar::tab:selected {
                background-color: #1a1a2e;
                color: #e0e0e0;
                border-bottom: 2px solid #4CAF50;
            }
            QTabBar::tab:hover {
                background-color: #0f3460;
            }

            QFrame#coinsBadge {
                background-color: #3d2e00;
                border: 1px solid #FFD54F;
                border-radius: 20px;
                padding: 6px 14px;
            }
            QFrame#gemsBadge {
                background-color: #0a3d0a;
                border: 1px solid #81C784;
                border-radius: 20px;
                padding: 6px 14px;
            }

            QFrame#gradesAttendanceBlock, QFrame#homeworkBlock, QFrame#awardsBlock,
            QFrame#leaderboardBlock, QFrame#examsBlock, QFrame#ratingBlock {
                background-color: #16213e;
                border: 1px solid #0f3460;
                border-radius: 8px;
                padding: 16px;
            }

            QLabel#ratingBlockTitle {
                font-size: 14px;
                font-weight: 600;
                color: #e0e0e0;
            }

            QFrame#ratingDivider {
                background-color: #0f3460;
            }

            QProgressBar {
                background-color: #0f3460;
                border: none;
                border-radius: 4px;
                height: 8px;
            }
            QProgressBar::chunk {
                background-color: #4CAF50;
                border-radius: 4px;
            }

            QProgressBar#attendanceProgressBar::chunk {
                background-color: #2196F3;
            }

            QListWidget {
                background: transparent;
                border: none;
                color: #e0e0e0;
            }
            QListWidget::item {
                padding: 6px 8px;
                border-bottom: 1px solid #0f3460;
            }
        )";
    } else {
        theme = R"(
            QMainWindow, QWidget#centralwidget, QWidget#homeTab, QWidget#scheduleTab,
            QWidget#gradesTab, QWidget#paymentTab, QWidget#feedbackTab {
                background-color: #ffffff;
            }

            QWidget#topBar {
                background-color: #f8f9fa;
                border-bottom: 1px solid #e0e0e0;
            }

            QLabel {
                color: #333;
                background: transparent;
            }

            QLabel#appTitle {
                font-size: 18px;
                font-weight: bold;
                color: #333;
            }

            QLabel#welcomeLabel {
                font-size: 24px;
                font-weight: bold;
                color: #1a1a2e;
            }

            QLabel#userLabel {
                color: #666;
                font-size: 12px;
            }

            QLabel#coinsValue {
                font-size: 15px;
                font-weight: bold;
                color: #F9A825;
            }

            QLabel#gemsValue {
                font-size: 15px;
                font-weight: bold;
                color: #2E7D32;
            }

            QPushButton {
                background-color: #f5f5f5;
                border: 1px solid #e0e0e0;
                border-radius: 4px;
                padding: 6px 12px;
                color: #333;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: #eeeeee;
            }
            QPushButton:checked {
                background-color: #4CAF50;
                color: white;
            }

            QPushButton#syncButton {
                background-color: #4CAF50;
                color: white;
                border: none;
            }
            QPushButton#syncButton:hover {
                background-color: #45a049;
            }

            QPushButton#logoutButton {
                background-color: #ff6b6b;
                color: white;
                border: none;
            }
            QPushButton#logoutButton:hover {
                background-color: #ff5252;
            }

            QTabWidget::pane {
                border: 1px solid #e0e0e0;
                background-color: #ffffff;
            }
            QTabBar::tab {
                background-color: #f8f9fa;
                color: #666;
                border: 1px solid #e0e0e0;
                padding: 10px 20px;
                margin-right: 2px;
            }
            QTabBar::tab:selected {
                background-color: #ffffff;
                color: #333;
                border-bottom: 2px solid #4CAF50;
            }
            QTabBar::tab:hover {
                background-color: #f0f0f0;
            }

            QFrame#coinsBadge {
                background-color: #FFF8E1;
                border: 1px solid #FFD54F;
                border-radius: 20px;
                padding: 6px 14px;
            }
            QFrame#gemsBadge {
                background-color: #E8F5E9;
                border: 1px solid #81C784;
                border-radius: 20px;
                padding: 6px 14px;
            }

            QFrame#gradesAttendanceBlock, QFrame#homeworkBlock, QFrame#awardsBlock,
            QFrame#leaderboardBlock, QFrame#examsBlock, QFrame#ratingBlock {
                background-color: #ffffff;
                border: 1px solid #e0e0e0;
                border-radius: 8px;
                padding: 16px;
            }

            QLabel#ratingBlockTitle {
                font-size: 14px;
                font-weight: 600;
                color: #333;
            }

            QFrame#ratingDivider {
                background-color: #e0e0e0;
            }

            QProgressBar {
                background-color: #f0f0f0;
                border: none;
                border-radius: 4px;
                height: 8px;
            }
            QProgressBar::chunk {
                background-color: #4CAF50;
                border-radius: 4px;
            }

            QProgressBar#attendanceProgressBar::chunk {
                background-color: #2196F3;
            }

            QListWidget {
                background: transparent;
                border: none;
                color: #333;
            }
            QListWidget::item {
                padding: 6px 8px;
                border-bottom: 1px solid #f5f5f5;
            }
        )";
    }

    qApp->setStyleSheet(theme);
    if (m_isDarkTheme) {
        ui->ratingBlockTitle->setStyleSheet("color: #e0e0e0; font-size: 14px; font-weight: 600;");
    } else {
        ui->ratingBlockTitle->setStyleSheet("color: #333; font-size: 14px; font-weight: 600;");
    }
    m_scheduleWidget->applyTheme(m_isDarkTheme);
}