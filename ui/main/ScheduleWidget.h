#ifndef SCHEDULEWIDGET_H
#define SCHEDULEWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QCalendarWidget>
#include <QTextBrowser>
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
    void onPreviousDate();
    void onNextDate();
    void onDateSelected(const QDate &date);
    void onDayScheduleLoaded(const DaySchedule &schedule);
    void onWeekScheduleLoaded(const WeekSchedule &schedule);
    void onSyncStarted();
    void onSyncFinished();
    void onDayButtonClicked();
    void onCalendarDayClicked(const QDate &date);
    void onDayViewClicked();
    void onWeekViewClicked();
    void onMonthViewClicked();

private:
    void initUI();
    void updateDayView(const QDate &date);
    void updateWeekView(const QDate &startDate);
    void updateMonthView(const QDate &month);
    void displayLessons(const QVector<Lesson> &lessons);
    void applyStyles();
    void createWeekDayButtons(const QDate &startDate);
    void updateCalendarForMonth(const QDate &month);
    void updateDayButtonStyles();
    void updateViewModeButtons();

    ScheduleManager *m_scheduleManager;
    ViewMode m_currentMode;
    QDate m_currentDate;

    // UI элементы
    QLabel *m_dateLabel;
    QPushButton *m_dayViewBtn;
    QPushButton *m_weekViewBtn;
    QPushButton *m_monthViewBtn;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    QPushButton *m_todayButton;
    QTextBrowser *m_lessonsListWidget;
    QCalendarWidget *m_calendarWidget;
    QWidget *m_daysContainer;
    QVector<QPushButton*> m_dayButtons;
    QPushButton *m_currentSelectedButton;
    QMap<QDate, bool> m_daysWithLessons;
};

#endif // SCHEDULEWIDGET_H
