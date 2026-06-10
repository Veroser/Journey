#include "ScheduleManager.h"
#include "../auth/ApiClient.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

ScheduleManager::ScheduleManager(ApiClient *apiClient, QObject *parent)
    : QObject(parent), m_apiClient(apiClient), m_isSyncing(false)
{
    initDatabase();

    // Парсер расписания
    m_parserRunner = new ParserRunner(this);
    setupParserConnections();

    // Парсер домашних заданий
    m_homeworkRunner = new ParserRunner(this);
    setupHomeworkParserConnections();

    // Загружаем ДЗ при старте
    loadHomeworkFromDb();
}

ScheduleManager::~ScheduleManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

void ScheduleManager::initDatabase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    QString dbPath = QCoreApplication::applicationDirPath() + "/db/schedule.db";
    m_db.setDatabaseName(dbPath);

    qDebug() << "Путь к БД:" << dbPath;

    if (!m_db.open()) {
        qWarning() << "Ошибка открытия БД:" << m_db.lastError().text();
        return;
    }

    qDebug() << "БД инициализирована";
}

void ScheduleManager::loadDaySchedule(const QDate &date)
{
    if (m_isSyncing) return;

    DaySchedule schedule;
    schedule.date = date;

    loadDayFromDatabase(date, schedule);

    if (!schedule.isEmpty()) {
        emit dayScheduleLoaded(schedule);
    }

    if (hasInternetConnection()) {
        m_isSyncing = true;
        emit syncStarted();

        QString dateStr = date.toString("yyyy-MM-dd");
        m_apiClient->get("/schedule/day/" + dateStr,
                         [this](QJsonObject response) { onScheduleDataReceived(response); },
                         [this](QString error) { onScheduleDataError(error); });
    }
}

void ScheduleManager::loadWeekSchedule(const QDate &startDate)
{
    WeekSchedule weekSchedule;
    weekSchedule.startDate = startDate;

    for (int i = 0; i < 7; ++i) {
        DaySchedule daySchedule;
        daySchedule.date = startDate.addDays(i);
        loadDayFromDatabase(daySchedule.date, daySchedule);
        weekSchedule.days.append(daySchedule);
    }

    emit weekScheduleLoaded(weekSchedule);

    if (hasInternetConnection()) {
        m_isSyncing = true;
        emit syncStarted();
        refreshFromServer();
    }
}

void ScheduleManager::loadMonthSchedule(const QDate &month)
{
    QVector<DaySchedule> monthSchedule;
    QDate startDate(month.year(), month.month(), 1);
    QDate endDate = month.addMonths(1).addDays(-1);

    for (QDate d = startDate; d <= endDate; d = d.addDays(1)) {
        DaySchedule daySchedule;
        daySchedule.date = d;
        loadDayFromDatabase(d, daySchedule);
        monthSchedule.append(daySchedule);
    }

    emit monthScheduleLoaded(monthSchedule);
}

void ScheduleManager::syncSchedule()
{
    if (hasInternetConnection()) {
        m_isSyncing = true;
        emit syncStarted();
        refreshFromServer();
    }
}

void ScheduleManager::refreshFromServer()
{
    QDate today = QDate::currentDate();
    QString dateStr = today.toString("yyyy-MM-dd");

    m_apiClient->get("/schedule/week/" + dateStr,
                     [this](QJsonObject response) { onScheduleDataReceived(response); },
                     [this](QString error) { onScheduleDataError(error); });
}

bool ScheduleManager::hasInternetConnection() const
{
    return false;
}

void ScheduleManager::onScheduleDataReceived(const QJsonObject &json)
{
    QSqlQuery query;
    QJsonArray lessonsArray = json["lessons"].toArray();

    for (const QJsonValue &value : lessonsArray) {
        QJsonObject lessonObj = value.toObject();
        Lesson lesson = Lesson::fromJson(lessonObj);

        if (lesson.isValid()) {
            QString dateStr = lessonObj["date"].toString();

            query.prepare("INSERT OR REPLACE INTO lessons "
                          "(date, subject, classroom, teacher, start_time, end_time, type) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?)");
            query.addBindValue(dateStr);
            query.addBindValue(lesson.subject);
            query.addBindValue(lesson.classroom);
            query.addBindValue(lesson.teacher);
            query.addBindValue(lesson.startTime.toString("HH:mm"));
            query.addBindValue(lesson.endTime.toString("HH:mm"));
            query.addBindValue(lesson.lessonType);

            if (!query.exec()) {
                qWarning() << "Ошибка сохранения урока:" << query.lastError().text();
            }
        }
    }

    m_isSyncing = false;
    emit syncFinished();
    qDebug() << "Расписание синхронизировано";
}

void ScheduleManager::onScheduleDataError(const QString &error)
{
    m_isSyncing = false;
    emit syncFailed(error);
    qWarning() << "Ошибка синхронизации:" << error;
}

void ScheduleManager::saveDayToDatabase(const DaySchedule &schedule)
{
    QSqlQuery query;

    for (const Lesson &lesson : schedule.lessons) {
        query.prepare("INSERT OR IGNORE INTO lessons (date, lesson, started_at, finished_at, teacher_name, subject_name, room_name) "
                      "VALUES (?, 1, ?, ?, ?, ?, ?)");
        query.addBindValue(schedule.date.toString("yyyy-MM-dd"));
        query.addBindValue(lesson.startTime.toString("HH:mm"));
        query.addBindValue(lesson.endTime.toString("HH:mm"));
        query.addBindValue(lesson.teacher);
        query.addBindValue(lesson.subject);
        query.addBindValue(lesson.classroom);

        if (!query.exec()) {
            qWarning() << "Ошибка сохранения:" << query.lastError().text();
        }
    }
}

void ScheduleManager::loadDayFromDatabase(QDate date, DaySchedule &schedule)
{
    QSqlQuery query;
    query.prepare("SELECT subject_name, room_name, teacher_name, started_at, finished_at FROM lessons WHERE date = ? ORDER BY started_at");
    query.addBindValue(date.toString("yyyy-MM-dd"));

    if (!query.exec()) {
        qWarning() << "Ошибка запроса:" << query.lastError().text();
        return;
    }

    while (query.next()) {
        Lesson lesson;
        lesson.subject = query.value(0).toString();
        lesson.classroom = query.value(1).toString();
        lesson.teacher = query.value(2).toString();
        lesson.startTime = QTime::fromString(query.value(3).toString(), "HH:mm");
        lesson.endTime = QTime::fromString(query.value(4).toString(), "HH:mm");

        schedule.lessons.append(lesson);
    }
}

DaySchedule ScheduleManager::getDaySchedule(const QDate &date) const
{
    DaySchedule schedule;
    schedule.date = date;
    return schedule;
}

WeekSchedule ScheduleManager::getWeekSchedule(const QDate &startDate) const
{
    WeekSchedule schedule;
    schedule.startDate = startDate;
    return schedule;
}

QVector<DaySchedule> ScheduleManager::getMonthSchedule(const QDate &month) const
{
    return QVector<DaySchedule>();
}

// ─── Парсер расписания ─────────────────────────────────────────────

void ScheduleManager::setupParserConnections()
{
    connect(m_parserRunner, &ParserRunner::parserStarted, this, [this]() {
        m_isSyncing = true;
        emit syncStarted();
    });

    connect(m_parserRunner, &ParserRunner::parserFinished,
            this, &ScheduleManager::onParserFinished);
    connect(m_parserRunner, &ParserRunner::parserError,
            this, &ScheduleManager::onParserError);
    connect(m_parserRunner, &ParserRunner::parserProgress,
            this, &ScheduleManager::onParserProgress);
}

void ScheduleManager::syncWithParser(const QString &jwtToken)
{
    if (m_isSyncing) {
        qWarning() << "Синхронизация уже выполняется";
        return;
    }

    if (!m_parserRunner) {
        qWarning() << "ParserRunner не инициализирован!";
        return;
    }

    m_pendingJwtToken = jwtToken;
    m_isSyncing = true;
    emit syncStarted();

    QString dbPath = m_db.databaseName();

    // Ищем parser_main.py
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/db/parser_main.py",
        QCoreApplication::applicationDirPath() + "/../db/parser_main.py",
        QCoreApplication::applicationDirPath() + "/../../data/db/parser_main.py",
        QDir::currentPath() + "/data/db/parser_main.py"
    };

    QString scriptPath;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            scriptPath = QFileInfo(path).absoluteFilePath();
            break;
        }
    }

    m_parserRunner->runParser(scriptPath, jwtToken, dbPath);

    if (scriptPath.isEmpty()) {
        m_isSyncing = false;
        emit syncFailed("Скрипт parser_main.py не найден");
        return;
    }

    qDebug() << "🔄 Запуск парсера расписания:" << scriptPath;

}

void ScheduleManager::cancelSync()
{
    if (m_parserRunner && m_parserRunner->isRunning()) {
        m_parserRunner->cancelParser();
    }
    if (m_homeworkRunner && m_homeworkRunner->isRunning()) {
        m_homeworkRunner->cancelParser();
    }
    m_isSyncing = false;
}

void ScheduleManager::onParserFinished(bool success, const QString &message)
{
    // Запускаем парсер ДЗ, флаг m_isSyncing остаётся true
    startHomeworkSync();

    if (success) {
        qDebug() << "✅ Парсер расписания завершён:" << message;

        QDate today = QDate::currentDate();
        loadDaySchedule(today);


    } else {
        m_isSyncing = false;
        emit syncFailed(message);
    }
}

void ScheduleManager::onParserError(const QString &error)
{
    m_isSyncing = false;
    emit syncFailed(error);
}

void ScheduleManager::onParserProgress(int percent)
{
    qDebug() << "Прогресс синхронизации:" << percent << "%";
}

// ─── Парсер домашних заданий ───────────────────────────────────────

void ScheduleManager::setupHomeworkParserConnections()
{
    connect(m_homeworkRunner, &ParserRunner::parserFinished,
            this, &ScheduleManager::onHomeworkParserFinished);
    connect(m_homeworkRunner, &ParserRunner::parserError, this, [this](const QString &error) {
        qWarning() << "Ошибка парсера ДЗ:" << error;
    });
}

void ScheduleManager::startHomeworkSync()
{
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/db/parser_homework.py",
        QCoreApplication::applicationDirPath() + "/../db/parser_homework.py",
        QCoreApplication::applicationDirPath() + "/../../data/db/parser_homework.py",
        QDir::currentPath() + "/data/db/parser_homework.py"
    };

    QString scriptPath;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            scriptPath = QFileInfo(path).absoluteFilePath();
            break;
        }
    }

    QString dbPath = QCoreApplication::applicationDirPath() + "/db/homework.db";

    if (scriptPath.isEmpty()) {
        qWarning() << "⚠️ parser_homework.py не найден, пропускаем ДЗ";
        m_isSyncing = false;
        emit syncFinished();
        return;
    }

    qDebug() << "🔄 Запуск парсера домашних заданий:" << scriptPath;
    m_homeworkRunner->runParser(scriptPath, m_pendingJwtToken, dbPath);
}

void ScheduleManager::onHomeworkParserFinished(bool success, const QString &message)
{
    if (success) {
        qDebug() << "✅ Парсер ДЗ завершён:" << message;
        loadHomeworkFromDb();
        emit homeworkUpdated();
    } else {
        qWarning() << "⚠️ Парсер ДЗ провалился:" << message;
    }

    m_isSyncing = false;
    emit syncFinished();
}

// ─── Загрузка ДЗ из SQLite ─────────────────────────────────────────

void ScheduleManager::loadHomeworkFromDb()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/db/homework.db";

    if (!QFileInfo::exists(dbPath)) {
        dbPath = QCoreApplication::applicationDirPath() + "/../db/homework.db";
    }
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/homework.db";
    }

    if (!QFileInfo::exists(dbPath)) {
        qDebug() << "📭 homework.db не найден";
        return;
    }

    QString connName = "homework_load";
    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase::removeDatabase(connName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "❌ Не удалось открыть homework.db:" << db.lastError().text();
        return;
    }

    QSqlQuery query(db);
    if (query.exec("SELECT checked, current, overdue, under_inspection, all_tasks FROM tasks ORDER BY id DESC LIMIT 1")) {
        if (query.next()) {
            m_homeworkDone = query.value(0).toInt();
            m_homeworkCurrent = query.value(1).toInt();
            m_homeworkOverdue = query.value(2).toInt();
            m_homeworkUnderInspection = query.value(3).toInt();
            m_homeworkAll = query.value(4).toInt();

            qDebug() << "📊 ДЗ: всего=" << m_homeworkAll
                     << "сдано=" << m_homeworkDone
                     << "на проверке=" << m_homeworkUnderInspection
                     << "просрочено=" << m_homeworkOverdue
                     << "текущие=" << m_homeworkCurrent;
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

// ─── Геттеры ДЗ ────────────────────────────────────────────────────

int ScheduleManager::homeworkAll() const { return m_homeworkAll; }
int ScheduleManager::homeworkDone() const { return m_homeworkDone; }
int ScheduleManager::homeworkOverdue() const { return m_homeworkOverdue; }
int ScheduleManager::homeworkUnderInspection() const { return m_homeworkUnderInspection; }
int ScheduleManager::homeworkCurrent() const { return m_homeworkCurrent; }