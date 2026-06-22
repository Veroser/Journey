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

// Парсер расписания

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

    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/db/dist/schedule",
        QCoreApplication::applicationDirPath() + "/db/dist_win/schedule.exe",
        QCoreApplication::applicationDirPath() + "/db/schedule.py",
        QCoreApplication::applicationDirPath() + "/../db/schedule.py",
        QCoreApplication::applicationDirPath() + "/../../data/db/schedule.py",
        QDir::currentPath() + "/data/db/schedule.py"
    };

    QString scriptPath;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            scriptPath = QFileInfo(path).absoluteFilePath();
            break;
        }
    }

    if (scriptPath.isEmpty()) {
        m_isSyncing = false;
        emit syncFailed("Скрипт schedule.py не найден");
        return;
    }

    m_parserRunner->runParser(scriptPath, jwtToken, dbPath);

    emit syncStatusChanged("Загрузка расписания...");
    qDebug() << "Запуск парсера расписания:" << scriptPath;
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
    if (success) {
        qDebug() << "Парсер расписания завершён:" << message;
        emit syncStatusChanged("Расписание загружено, загрузка домашних заданий...");
        QDate today = QDate::currentDate();
        loadDaySchedule(today);
    } else {
        m_isSyncing = false;
        emit syncFailed(message);
        return;
    }
    startHomeworkSync();
}

void ScheduleManager::onParserError(const QString &error)
{
    m_isSyncing = false;
    emit syncFailed(error);
}

void ScheduleManager::onParserProgress(int percent)
{
    qDebug() << "Прогресс синхронизации:" << percent << "%";
    emit parserProgress(percent);
}

// Парсер домашних заданий

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
        QCoreApplication::applicationDirPath() + "/db/dist/homework",
        QCoreApplication::applicationDirPath() + "/db/dist_win/homework.exe",
        QCoreApplication::applicationDirPath() + "/db/homework.py",
        QCoreApplication::applicationDirPath() + "/../db/homework.py",
        QDir::currentPath() + "/data/db/homework.py"
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
        qWarning() << "homework.py не найден, пропускаем ДЗ";
        m_isSyncing = false;
        emit syncFinished();
        return;
    }
    emit syncStatusChanged("Загрузка домашних заданий");

    qDebug() << "Запуск парсера домашних заданий:" << scriptPath;
    m_homeworkRunner->runParser(scriptPath, m_pendingJwtToken, dbPath);
}

void ScheduleManager::onHomeworkParserFinished(bool success, const QString &message)
{
    if (success) {
        qDebug() << "Парсер ДЗ завершён:" << message;
        loadHomeworkFromDb();
        emit homeworkUpdated();
    } else {
        qWarning() << "Парсер ДЗ провалился:" << message;
    }

    startMetricsSync();
}

// Загрузка ДЗ из SQLite

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
        qDebug() << "homework.db не найден";
        return;
    }

    QString connName = "homework_load";
    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase::removeDatabase(connName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Не удалось открыть homework.db:" << db.lastError().text();
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

            qDebug() << "ДЗ: всего=" << m_homeworkAll
                     << "сдано=" << m_homeworkDone
                     << "на проверке=" << m_homeworkUnderInspection
                     << "просрочено=" << m_homeworkOverdue
                     << "текущие=" << m_homeworkCurrent;
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);
}

void ScheduleManager::startMetricsSync()
{
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/db/dist/performance",
        QCoreApplication::applicationDirPath() + "/db/dist_win/performance.exe",
        QCoreApplication::applicationDirPath() + "/db/performance.py",
        QCoreApplication::applicationDirPath() + "/../db/performance.py",
        QDir::currentPath() + "/data/db/performance.py"
    };

    QString scriptPath;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            scriptPath = QFileInfo(path).absoluteFilePath();
            break;
        }
    }
    emit syncStatusChanged("Загрузка успеваемости");

    QString dbPath = QCoreApplication::applicationDirPath() + "/db/main.db";
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/main.db";
    }

    if (scriptPath.isEmpty()) {
        qWarning() << "performance.py не найден";
        loadMetricsFromMainDb();
        startRatingSync();  // продолжаем цепочку даже без парсера
        return;
    }

    if (!m_metricsRunner) {
        m_metricsRunner = new ParserRunner(this);
        connect(m_metricsRunner, &ParserRunner::parserFinished, this, [this](bool success, const QString &msg) {
            if (success) {
                qDebug() << "Метрики обновлены";
                loadMetricsFromMainDb();
                emit metricsUpdated();
            } else {
                qWarning() << "Ошибка парсера метрик:" << msg;
            }
            // Убрал emit syncFinished() отсюда — цепочка продолжается
            startRatingSync();
        });

        connect(m_metricsRunner, &ParserRunner::parserError, this, [this](const QString &error) {
            qWarning() << "Ошибка парсера метрик:" << error;
            // Не завершаем синхронизацию, пробуем продолжить
            startRatingSync();
        });
    }

    m_metricsRunner->runParser(scriptPath, m_pendingJwtToken, dbPath);
}

void ScheduleManager::loadMetricsFromMainDb()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/db/main.db";
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/main.db";
    }

    if (!QFileInfo::exists(dbPath)) {
        qDebug() << "main.db не найден";
        return;
    }

    QString connName = "main_metrics";
    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase::removeDatabase(connName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Не удалось открыть main.db:" << db.lastError().text();
        return;
    }

    QSqlQuery query(db);

    if (query.exec("SELECT points FROM grade ORDER BY id DESC LIMIT 1")) {
        if (query.next()) {
            m_gradesPercent = query.value(0).toInt();
        }
    }

    if (query.exec("SELECT points FROM attendance ORDER BY id DESC LIMIT 1")) {
        if (query.next()) {
            m_attendancePercent = query.value(0).toInt();
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);

    qDebug() << "Метрики: средний балл=" << m_gradesPercent
             << "посещаемость=" << m_attendancePercent << "%";
}

void ScheduleManager::startRatingSync()
{
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/db/dist/rating",
        QCoreApplication::applicationDirPath() + "/db/dist_win/rating.exe",
        QCoreApplication::applicationDirPath() + "/db/rating.py",
        QCoreApplication::applicationDirPath() + "/../db/rating.py",
        QDir::currentPath() + "/data/db/rating.py"
    };

    QString scriptPath;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            scriptPath = QFileInfo(path).absoluteFilePath();
            break;
        }
    }

    QString dbPath = QCoreApplication::applicationDirPath() + "/db/main.db";
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/main.db";
    }

    if (scriptPath.isEmpty()) {
        qWarning() << "rating.py не найден";
        loadRatingFromDb();
        startUserInfoSync();    // продолжаем цепочку
        startFeedbackSync();
        return;
    }

    if (!m_ratingRunner) {
        m_ratingRunner = new ParserRunner(this);
        connect(m_ratingRunner, &ParserRunner::parserFinished, this, [this](bool success, const QString &msg) {
            if (success) {
                loadRatingFromDb();
                emit ratingUpdated();
            }
            startUserInfoSync();
            startFeedbackSync();
        });

        connect(m_ratingRunner, &ParserRunner::parserError, this, [this](const QString & /*error*/) {
            // Продолжаем цепочку даже при ошибке
            startUserInfoSync();
            startFeedbackSync();
        });
    }

    emit syncStatusChanged("Загрузка рейтинга...");
    m_ratingRunner->runParser(scriptPath, m_pendingJwtToken, dbPath);
}

void ScheduleManager::loadRatingFromDb()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/db/main.db";
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/main.db";
    }

    if (!QFileInfo::exists(dbPath)) {
        return;
    }

    QString connName = "rating_load";
    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase::removeDatabase(connName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Не удалось открыть main.db для рейтинга";
        return;
    }

    QSqlQuery query(db);

    if (query.exec("SELECT all_group_amount, all_stream_amount, group_user_position, stream_user_position FROM user_rate ORDER BY id DESC LIMIT 1")) {
        if (query.next()) {
            m_groupTotal = query.value(0).toInt();
            m_streamTotal = query.value(1).toInt();
            m_groupPosition = query.value(2).toInt();
            m_streamPosition = query.value(3).toInt();
        }
    }

    m_groupRating.clear();
    query.exec("SELECT full_name, amount, position FROM group_rate ORDER BY position");
    while (query.next()) {
        StudentInfo info;
        info.fullName = query.value(0).toString();
        info.amount = query.value(1).toInt();
        info.position = query.value(2).toInt();
        info.isCurrentUser = (info.position == m_groupPosition);
        m_groupRating.append(info);
    }

    m_streamRating.clear();
    query.exec("SELECT full_name, amount, position FROM stream_rate ORDER BY position");
    while (query.next()) {
        StudentInfo info;
        info.fullName = query.value(0).toString();
        info.amount = query.value(1).toInt();
        info.position = query.value(2).toInt();
        info.isCurrentUser = (info.position == m_streamPosition);
        m_streamRating.append(info);
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);

    qDebug() << "Рейтинг: группа" << m_groupPosition << "/" << m_groupTotal
             << "поток" << m_streamPosition << "/" << m_streamTotal;
}

void ScheduleManager::startUserInfoSync()
{
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/db/dist/user",
        QCoreApplication::applicationDirPath() + "/db/dist_win/user.exe",
        QCoreApplication::applicationDirPath() + "/db/user.py",
        QCoreApplication::applicationDirPath() + "/../db/user.py",
        QDir::currentPath() + "/data/db/user.py"
    };

    QString scriptPath;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            scriptPath = QFileInfo(path).absoluteFilePath();
            break;
        }
    }

    QString dbPath = QCoreApplication::applicationDirPath() + "/db/main.db";
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/main.db";
    }

    if (scriptPath.isEmpty()) {
        qWarning() << "user_info.py не найден";
        loadUserInfoFromDb();
        return;
    }

    if (!m_userInfoRunner) {
        m_userInfoRunner = new ParserRunner(this);
        connect(m_userInfoRunner, &ParserRunner::parserFinished, this, [this](bool success, const QString &msg) {
            if (success) {
                loadUserInfoFromDb();
                emit userInfoUpdated();
            }
        });
    }

    m_userInfoRunner->runParser(scriptPath, m_pendingJwtToken, dbPath);
}

void ScheduleManager::startFeedbackSync()
{
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/db/dist/feedback",
        QCoreApplication::applicationDirPath() + "/db/dist_win/feedback.exe",
        QCoreApplication::applicationDirPath() + "/db/feedback.py",
        QCoreApplication::applicationDirPath() + "/../db/feedback.py",
        QDir::currentPath() + "/data/db/feedback.py"
    };

    QString scriptPath;
    for (const QString &path : searchPaths) {
        if (QFileInfo::exists(path)) {
            scriptPath = QFileInfo(path).absoluteFilePath();
            break;
        }
    }

    QString dbPath = QCoreApplication::applicationDirPath() + "/db/feedback.db";
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/feedback.db";
    }

    if (scriptPath.isEmpty()) {
        qWarning() << "feedback.py не найден";
        loadFeedbackFromDb();
        // Завершаем синхронизацию даже без парсера
        emit syncStatusChanged("Синхронизация завершена");
        m_isSyncing = false;
        emit syncFinished();
        return;
    }

    if (!m_feedbackRunner) {
        m_feedbackRunner = new ParserRunner(this);
        connect(m_feedbackRunner, &ParserRunner::parserFinished, this, [this](bool success, const QString &msg) {
            if (success) {
                loadFeedbackFromDb();
                emit feedbackUpdated();
            }
            // Финальное завершение всей цепочки
            emit syncStatusChanged("Синхронизация завершена");
            m_isSyncing = false;
            emit syncFinished();
        });

        connect(m_feedbackRunner, &ParserRunner::parserError, this, [this](const QString &error) {
            qWarning() << "Ошибка парсера отзывов:" << error;
            // Даже при ошибке завершаем синхронизацию
            emit syncStatusChanged("Синхронизация завершена");
            m_isSyncing = false;
            emit syncFinished();
        });
    }

    emit syncStatusChanged("Загрузка отзывов...");
    m_feedbackRunner->runParser(scriptPath, m_pendingJwtToken, dbPath);
}

void ScheduleManager::loadFeedbackFromDb()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/db/feedback.db";
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/feedback.db";
    }

    if (!QFileInfo::exists(dbPath)) {
        return;
    }

    QString connName = "feedback_load";
    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase::removeDatabase(connName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Не удалось открыть feedback.db";
        return;
    }

    QSqlQuery query(db);
    m_feedback.clear();

    if (query.exec("SELECT date, message, full_spec, teacher FROM feedback ORDER BY id DESC")) {
        while (query.next()) {
            FeedbackInfo info;
            info.date = query.value(0).toString();
            info.message = query.value(1).toString();
            info.subject = query.value(2).toString();
            info.teacher = query.value(3).toString();
            m_feedback.append(info);
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);

    qDebug() << "Отзывы: загружено" << m_feedback.size();
}

QVector<ScheduleManager::FeedbackInfo> ScheduleManager::feedback() const { return m_feedback; }

void ScheduleManager::loadUserInfoFromDb()
{
    QString dbPath = QCoreApplication::applicationDirPath() + "/db/main.db";
    if (!QFileInfo::exists(dbPath)) {
        dbPath = QDir::currentPath() + "/data/db/main.db";
    }

    if (!QFileInfo::exists(dbPath)) {
        return;
    }

    QString connName = "user_info_load";
    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase::removeDatabase(connName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Не удалось открыть main.db для user_info";
        return;
    }

    QSqlQuery query(db);

    if (query.exec("SELECT name, coins, gems FROM user_info ORDER BY id DESC LIMIT 1")) {
        if (query.next()) {
            m_userName = query.value(0).toString();
            m_userCoins = query.value(1).toInt();
            m_userGems = query.value(2).toInt();
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connName);

    qDebug() << "Пользователь:" << m_userName << "Монеты:" << m_userCoins << "Геммы:" << m_userGems;
}

QString ScheduleManager::userName() const { return m_userName; }
int ScheduleManager::userCoins() const { return m_userCoins; }
int ScheduleManager::userGems() const { return m_userGems; }

// Геттеры

QVector<ScheduleManager::StudentInfo> ScheduleManager::groupRating() const { return m_groupRating; }
QVector<ScheduleManager::StudentInfo> ScheduleManager::streamRating() const { return m_streamRating; }

int ScheduleManager::groupPosition() const { return m_groupPosition; }
int ScheduleManager::groupTotal() const { return m_groupTotal; }
int ScheduleManager::streamPosition() const { return m_streamPosition; }
int ScheduleManager::streamTotal() const { return m_streamTotal; }

int ScheduleManager::homeworkAll() const { return m_homeworkAll; }
int ScheduleManager::homeworkDone() const { return m_homeworkDone; }
int ScheduleManager::homeworkOverdue() const { return m_homeworkOverdue; }
int ScheduleManager::homeworkUnderInspection() const { return m_homeworkUnderInspection; }
int ScheduleManager::homeworkCurrent() const { return m_homeworkCurrent; }

int ScheduleManager::gradesPercent() const { return m_gradesPercent; }
int ScheduleManager::attendancePercent() const { return m_attendancePercent; }
int ScheduleManager::coins() const { return m_userCoins; }
int ScheduleManager::gems() const { return m_userGems; }