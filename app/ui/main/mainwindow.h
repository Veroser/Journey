#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class AuthManager;
class ScheduleManager;
class ApiClient;
class ScheduleWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(AuthManager *authManager, ApiClient *apiClient, QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onLogout();
    void onSyncClicked();
    void updateHomeworkLabels();
    void updateMetricsDisplay();
    void updateLeaderboard();

private:
    void applyStyles();

    Ui::MainWindow *ui;
    AuthManager *m_authManager;
    ScheduleManager *m_scheduleManager;
    ScheduleWidget *m_scheduleWidget;
    bool m_isDarkTheme = false;
    void applyTheme();
};

#endif // MAINWINDOW_H