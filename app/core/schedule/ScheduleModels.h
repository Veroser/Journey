#ifndef SCHEDULEMODELS_H
#define SCHEDULEMODELS_H

#include <QString>
#include <QVector>
#include <QDate>
#include <QTime>
#include <QJsonObject>

struct Lesson {
    QString subject;
    QString classroom;
    QString teacher;
    QTime startTime;
    QTime endTime;
    QString lessonType;

    bool isValid() const {
        return !subject.isEmpty() && startTime.isValid() && endTime.isValid();
    }

    static Lesson fromJson(const QJsonObject &json) {
        Lesson lesson;
        lesson.subject = json["subject"].toString();
        lesson.classroom = json["classroom"].toString();
        lesson.teacher = json["teacher"].toString();
        lesson.startTime = QTime::fromString(json["start_time"].toString(), "HH:mm");
        lesson.endTime = QTime::fromString(json["end_time"].toString(), "HH:mm");
        lesson.lessonType = json["type"].toString("lecture");
        return lesson;
    }
};

struct DaySchedule {
    QDate date;
    QVector<Lesson> lessons;

    bool isEmpty() const {
        return lessons.isEmpty();
    }
};

struct WeekSchedule {
    QDate startDate;
    QVector<DaySchedule> days; // 7 дней в неделе
};

#endif // SCHEDULEMODELS_H
