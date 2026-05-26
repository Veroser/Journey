#ifndef SCHEDULEWIDGET_H
#define SCHEDULEWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QCalendarWidget>
#include <QListWidget>
#include <QDate>
#include "../../core/schedule/ScheduleModels.h"

class ScheduleManager;

class ScheduleWidget : public QWidget {
    Q_OBJECT

public:
    enum ViewMode {
        DayView,
        WeekView,
        MonthView
    };

    explicit ScheduleWidget(ScheduleManager *scheduleManager, QWidget *parent = nullptr);

private slots:
    void onViewModeChanged(int index);
    void onPreviousDate();
    void onNextDate();
    void onDateSelected(const QDate &date);
    void onDayScheduleLoaded(const DaySchedule &schedule);
    void onWeekScheduleLoaded(const WeekSchedule &schedule);
    void onSyncStarted();
    void onSyncFinished();

private:
    void initUI();
    void updateDayView(const QDate &date);
    void updateWeekView(const QDate &startDate);
    void updateMonthView(const QDate &month);
    void displayLessons(const QVector<Lesson> &lessons);
    void applyStyles();

    ScheduleManager *m_scheduleManager;
    ViewMode m_currentMode;
    QDate m_currentDate;

    // UI элементы
    QLabel *m_dateLabel;
    QComboBox *m_viewModeCombo;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    QPushButton *m_todayButton;
    QListWidget *m_lessonsListWidget;
    QCalendarWidget *m_calendarWidget;
};

#endif // SCHEDULEWIDGET_H
