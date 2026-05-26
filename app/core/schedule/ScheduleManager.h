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

public slots:
    void syncWithParser(const QString &jwtToken);
    void syncWithParser();
    void cancelSync();

signals:
    void dayScheduleLoaded(const DaySchedule &schedule);
    void weekScheduleLoaded(const WeekSchedule &schedule);
    void monthScheduleLoaded(const QVector<DaySchedule> &schedule);
    void syncStarted();
    void syncFinished();
    void syncFailed(const QString &error);

private slots:
    void onScheduleDataReceived(const QJsonObject &json);
    void onScheduleDataError(const QString &error);
    void onParserFinished(bool success, const QString &message);
    void onParserError(const QString &error);
    void onParserProgress(int percent);

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
};

#endif // SCHEDULEMANAGER_H
