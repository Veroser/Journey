#ifndef SCHEDULEMANAGER_H
#define SCHEDULEMANAGER_H

#include <QObject>
#include <QDate>
#include <QSqlDatabase>
#include "ScheduleModels.h"
#include "parserrunner.h"

class ApiClient;

class ScheduleManager : public QObject {
    Q_OBJECT

public:
    explicit ScheduleManager(ApiClient *apiClient, QObject *parent = nullptr);
    ~ScheduleManager();

    // Загрузка расписания
    void loadDaySchedule(const QDate &date);
    void loadWeekSchedule(const QDate &startDate);
    void loadMonthSchedule(const QDate &month);

    // Доступ к данным
    DaySchedule getDaySchedule(const QDate &date) const;
    WeekSchedule getWeekSchedule(const QDate &startDate) const;
    QVector<DaySchedule> getMonthSchedule(const QDate &month) const;

    // Синхронизация
    void syncSchedule();
    void refreshFromServer();
    bool isSyncing() const { return m_isSyncing; }
    bool hasInternetConnection() const;

    int homeworkAll() const;
    int homeworkDone() const;
    int homeworkOverdue() const;
    int homeworkUnderInspection() const;
    int homeworkCurrent() const;

public slots:
    void syncWithParser(const QString &jwtToken);
    // void syncWithParser();
    void cancelSync();

signals:
    void dayScheduleLoaded(const DaySchedule &schedule);
    void weekScheduleLoaded(const WeekSchedule &schedule);
    void monthScheduleLoaded(const QVector<DaySchedule> &schedule);
    void syncStarted();
    void syncFinished();
    void syncFailed(const QString &error);
    void homeworkUpdated();

private slots:
    void onScheduleDataReceived(const QJsonObject &json);
    void onScheduleDataError(const QString &error);
    void onParserFinished(bool success, const QString &message);
    void onParserError(const QString &error);
    void onParserProgress(int percent);
    void onHomeworkParserFinished(bool success, const QString &message);

private:
    ApiClient *m_apiClient;
    QSqlDatabase m_db;
    bool m_isSyncing;

    void initDatabase();
    void saveDayToDatabase(const DaySchedule &schedule);
    void loadDayFromDatabase(QDate date, DaySchedule &schedule);
    void runParser();

    ParserRunner *m_parserRunner;
    void setupParserConnections();

    ParserRunner *m_homeworkRunner;
    void setupHomeworkParserConnections();
    void startHomeworkSync();
    void loadHomeworkFromDb();

    int m_homeworkAll = 0;
    int m_homeworkDone = 0;
    int m_homeworkOverdue = 0;
    int m_homeworkUnderInspection = 0;
    int m_homeworkCurrent = 0;

    QString m_pendingJwtToken;
};

#endif // SCHEDULEMANAGER_H
