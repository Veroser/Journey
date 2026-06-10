#include "ScheduleWidget.h"
#include "../../core/schedule/ScheduleManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTextBrowser>
#include <QStyleFactory>
#include <QDate>
#include <QDebug>
#include <QLocale>

ScheduleWidget::ScheduleWidget(ScheduleManager *scheduleManager, QWidget *parent)
    : QWidget(parent), m_scheduleManager(scheduleManager),
      m_currentMode(DayView), m_currentDate(QDate::currentDate()),
      m_currentSelectedButton(nullptr)
{
    initUI();
    applyStyles();

    connect(m_scheduleManager, &ScheduleManager::dayScheduleLoaded,
            this, &ScheduleWidget::onDayScheduleLoaded);
    connect(m_scheduleManager, &ScheduleManager::weekScheduleLoaded,
            this, &ScheduleWidget::onWeekScheduleLoaded);
    connect(m_scheduleManager, &ScheduleManager::syncStarted,
            this, &ScheduleWidget::onSyncStarted);
    connect(m_scheduleManager, &ScheduleManager::syncFinished,
            this, &ScheduleWidget::onSyncFinished);

    connect(m_calendarWidget, &QCalendarWidget::clicked,
            this, &ScheduleWidget::onCalendarDayClicked);

    m_scheduleManager->loadDaySchedule(m_currentDate);
}

void ScheduleWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    m_dayViewBtn = new QPushButton("День");
    m_dayViewBtn->setMaximumWidth(70);
    m_dayViewBtn->setMinimumHeight(36);
    connect(m_dayViewBtn, &QPushButton::clicked, this, &ScheduleWidget::onDayViewClicked);

    m_weekViewBtn = new QPushButton("Неделя");
    m_weekViewBtn->setMaximumWidth(80);
    m_weekViewBtn->setMinimumHeight(36);
    connect(m_weekViewBtn, &QPushButton::clicked, this, &ScheduleWidget::onWeekViewClicked);

    m_monthViewBtn = new QPushButton("Месяц");
    m_monthViewBtn->setMaximumWidth(70);
    m_monthViewBtn->setMinimumHeight(36);
    connect(m_monthViewBtn, &QPushButton::clicked, this, &ScheduleWidget::onMonthViewClicked);

    headerLayout->addWidget(m_dayViewBtn);
    headerLayout->addWidget(m_weekViewBtn);
    headerLayout->addWidget(m_monthViewBtn);
    headerLayout->addStretch();

    m_dateLabel = new QLabel();
    m_dateLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #333;");
    headerLayout->addWidget(m_dateLabel);

    mainLayout->addLayout(headerLayout);

    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setSpacing(8);

    m_prevButton = new QPushButton("◀ Назад");
    m_nextButton = new QPushButton("Вперёд ▶");
    m_todayButton = new QPushButton("Сегодня");

    connect(m_prevButton, &QPushButton::clicked, this, &ScheduleWidget::onPreviousDate);
    connect(m_nextButton, &QPushButton::clicked, this, &ScheduleWidget::onNextDate);
    connect(m_todayButton, &QPushButton::clicked, [this]() {
        m_currentDate = QDate::currentDate();
        onDateSelected(m_currentDate);
    });

    navLayout->addWidget(m_prevButton);
    navLayout->addWidget(m_nextButton);
    navLayout->addWidget(m_todayButton);
    navLayout->addStretch();

    mainLayout->addLayout(navLayout);

    m_daysContainer = new QWidget();
    m_daysContainer->setMinimumHeight(80);
    mainLayout->addWidget(m_daysContainer);

    m_calendarWidget = new QCalendarWidget();
    m_calendarWidget->setStyleSheet(
        "QCalendarWidget { background-color: #ffffff; }"
        "QCalendarWidget QAbstractItemView { selection-background-color: #4CAF50; }"
        "QCalendarWidget QToolButton { color: #333; }"
    );
    m_calendarWidget->setMinimumHeight(250);
    m_calendarWidget->hide();
    mainLayout->addWidget(m_calendarWidget);

    m_lessonsListWidget = new QTextBrowser();
    m_lessonsListWidget->setStyleSheet(
        "QTextBrowser { border: 1px solid #e0e0e0; border-radius: 4px; background-color: #fafafa; }"
    );
    m_lessonsListWidget->setMinimumHeight(200);
    mainLayout->addWidget(m_lessonsListWidget);

    setLayout(mainLayout);
    updateViewModeButtons();
    updateDayView(m_currentDate);
}


void ScheduleWidget::applyStyles()
{
    setStyleSheet(
        "QWidget { background-color: #ffffff; color: #333; }"
        "QPushButton {"
        "  background-color: #f5f5f5;"
        "  border: 1px solid #e0e0e0;"
        "  border-radius: 4px;"
        "  padding: 6px 12px;"
        "  color: #333;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background-color: #eeeeee; }"
        "QPushButton:pressed { background-color: #e0e0e0; }"
        "QComboBox {"
        "  background-color: #f5f5f5;"
        "  border: 1px solid #e0e0e0;"
        "  border-radius: 4px;"
        "  padding: 4px 8px;"
        "}"
    );
}


void ScheduleWidget::onPreviousDate()
{
    switch (m_currentMode) {
        case DayView:
            m_currentDate = m_currentDate.addDays(-1);
            updateDayView(m_currentDate);
            break;
        case WeekView:
            m_currentDate = m_currentDate.addDays(-7);
            updateWeekView(m_currentDate);
            break;
        case MonthView:
            m_currentDate = m_currentDate.addMonths(-1);
            updateMonthView(m_currentDate);
            break;
    }
}

void ScheduleWidget::onNextDate()
{
    switch (m_currentMode) {
        case DayView:
            m_currentDate = m_currentDate.addDays(1);
            updateDayView(m_currentDate);
            break;
        case WeekView:
            m_currentDate = m_currentDate.addDays(7);
            updateWeekView(m_currentDate);
            break;
        case MonthView:
            m_currentDate = m_currentDate.addMonths(1);
            updateMonthView(m_currentDate);
            break;
    }
}

void ScheduleWidget::onDateSelected(const QDate &date)
{
    m_currentDate = date;
    switch (m_currentMode) {
        case DayView:
            updateDayView(date);
            break;
        case WeekView:
            updateWeekView(date.addDays(-(date.dayOfWeek() - 1)));
            break;
        case MonthView:
            updateMonthView(date);
            break;
    }
}

void ScheduleWidget::updateDayView(const QDate &date)
{
    m_currentDate = date;
    QLocale russian(QLocale::Russian);
    m_dateLabel->setText(russian.toString(date, "dddd, d MMMM"));

    m_lessonsListWidget->clear();
    m_scheduleManager->loadDaySchedule(date);
}

void ScheduleWidget::updateWeekView(const QDate &startDate)
{
    QLocale russian(QLocale::Russian);
    m_dateLabel->setText(QString("%1 - %2")
        .arg(russian.toString(startDate, "d MMM"))
        .arg(russian.toString(startDate.addDays(6), "d MMM yyyy")));

    m_daysContainer->show();
    m_calendarWidget->hide();
    createWeekDayButtons(startDate);
    updateDayButtonStyles();

    if (!m_dayButtons.isEmpty()) {
        m_scheduleManager->loadDaySchedule(startDate);
        m_currentDate = startDate;
    }
}

void ScheduleWidget::updateMonthView(const QDate &month)
{
    QLocale russian(QLocale::Russian);
    m_dateLabel->setText(russian.toString(month, "yyyy MMMM"));

    m_daysContainer->hide();
    m_calendarWidget->show();
    m_calendarWidget->setSelectedDate(month);

    m_scheduleManager->loadDaySchedule(month);
    m_currentDate = month;
}

void ScheduleWidget::displayLessons(const QVector<Lesson> &lessons)
{
    if (lessons.isEmpty()) {
        m_lessonsListWidget->setText("Нет занятий");
        return;
    }

    QString html = "<div style='margin: 0; padding: 0;'>";

    for (const Lesson &lesson : lessons) {
        QString borderColor = "#4CAF50";
        if (lesson.lessonType == "practice") {
            borderColor = "#2196F3";
        }

        html += QString(
            "<div style='padding: 8px 10px; background-color: #f9f9f9; "
            "border-left: 4px solid %1; margin-bottom: 4px; border-radius: 4px;'>"
            "<b style='font-size: 13px; color: #333;'>%2</b><br/>"
            "<span style='color: #666; font-size: 11px;'>%3 - %4</span><br/>"
            "<span style='color: #999; font-size: 10px;'>Ауд: %5 | Преп: %6</span>"
            "</div>"
        ).arg(borderColor)
         .arg(lesson.subject)
         .arg(lesson.startTime.toString("HH:mm"))
         .arg(lesson.endTime.toString("HH:mm"))
         .arg(lesson.classroom)
         .arg(lesson.teacher);
    }

    html += "</div>";
    m_lessonsListWidget->setHtml(html);
}

void ScheduleWidget::onDayScheduleLoaded(const DaySchedule &schedule)
{
    m_daysWithLessons[schedule.date] = !schedule.lessons.isEmpty();

    if (schedule.date == m_currentDate) {
        if (schedule.lessons.isEmpty()) {
            m_lessonsListWidget->setHtml(
                "<div style='text-align: center; color: #999; padding: 20px;'>"
                "<span style='font-size: 16px;'>Занятий нет</span><br>"
                "<span style='font-size: 12px;'>На этот день занятия не запланированы</span>"
                "</div>"
                );
        } else {
            displayLessons(schedule.lessons);
        }
    }
}

void ScheduleWidget::onWeekScheduleLoaded(const WeekSchedule &schedule)
{
    if (m_currentMode == WeekView) {
        QVector<Lesson> allLessons;
        for (const DaySchedule &day : schedule.days) {
            allLessons.append(day.lessons);
        }
        displayLessons(allLessons);
    }
}

void ScheduleWidget::onSyncStarted()
{
    qDebug() << "Синхронизация началась...";
}

void ScheduleWidget::onSyncFinished()
{
    qDebug() << "Синхронизация завершена, обнов вид...";

    switch (m_currentMode) {
    case DayView:
        updateDayView(m_currentDate);
        break;
    case WeekView:
        updateWeekView(m_currentDate);
        break;
    case MonthView:
        updateMonthView(m_currentDate);
        break;
    }
}

void ScheduleWidget::createWeekDayButtons(const QDate &startDate)
{
    qDeleteAll(m_dayButtons);
    m_dayButtons.clear();
    m_currentSelectedButton = nullptr;

    if (m_daysContainer->layout()) {
        delete m_daysContainer->layout();
    }

    QHBoxLayout *daysRow = new QHBoxLayout(m_daysContainer);
    daysRow->setSpacing(4);
    daysRow->setContentsMargins(0, 0, 0, 0);

    QLocale russian(QLocale::Russian);

    for (int i = 0; i < 7; ++i) {
        QDate date = startDate.addDays(i);

        QPushButton *btn = new QPushButton();
        btn->setMinimumHeight(30);
        btn->setMaximumWidth(50);
        btn->setCursor(Qt::PointingHandCursor);

        QString dayName = russian.toString(date, "ddd").left(2);
        QString text = QString("%1\n%2").arg(dayName).arg(date.day());
        btn->setText(text);
        btn->setProperty("date", date);

        if (date == m_currentDate) {
            m_currentSelectedButton = btn;
        }

        connect(btn, &QPushButton::clicked, this, &ScheduleWidget::onDayButtonClicked);

        m_dayButtons.append(btn);
        daysRow->addWidget(btn);
    }

    daysRow->addStretch();

    m_daysContainer->setLayout(daysRow);
}

void ScheduleWidget::updateDayButtonStyles()
{
    for (QPushButton *btn : m_dayButtons) {
        QDate btnDate = btn->property("date").toDate();
        if (btnDate == m_currentDate) {
            btn->setStyleSheet(
                "QPushButton { background-color: #4CAF50; color: #ffffff; "
                "border: 1px solid #45a049; border-radius: 4px; padding: 6px 12px; "
                "font-weight: 500; }"
                "QPushButton:hover { background-color: #45a049; }"
                "QPushButton:pressed { background-color: #3d8b40; }"
            );
        } else {
            btn->setStyleSheet(
                "QPushButton { background-color: #f5f5f5; color: #333; "
                "border: 1px solid #e0e0e0; border-radius: 4px; padding: 6px 12px; }"
                "QPushButton:hover { background-color: #eeeeee; }"
                "QPushButton:pressed { background-color: #e0e0e0; }"
            );
        }
    }
}

void ScheduleWidget::onDayButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    QDate selectedDate = button->property("date").toDate();
    m_currentDate = selectedDate;

    updateDayButtonStyles();
    m_scheduleManager->loadDaySchedule(selectedDate);
}

void ScheduleWidget::onCalendarDayClicked(const QDate &date)
{
    m_currentDate = date;
    m_scheduleManager->loadDaySchedule(date);
}

void ScheduleWidget::onDayViewClicked()
{
    m_currentMode = DayView;
    m_daysContainer->hide();
    m_calendarWidget->hide();
    updateViewModeButtons();
    updateDayView(m_currentDate);
}

void ScheduleWidget::onWeekViewClicked()
{
    m_currentMode = WeekView;
    updateViewModeButtons();
    updateWeekView(m_currentDate.addDays(-(m_currentDate.dayOfWeek() - 1)));
}

void ScheduleWidget::onMonthViewClicked()
{
    m_currentMode = MonthView;
    updateViewModeButtons();
    updateMonthView(m_currentDate);
}

void ScheduleWidget::updateViewModeButtons()
{
    QString selectedStyle =
        "QPushButton { background-color: #4CAF50; color: #ffffff; "
        "border: 1px solid #45a049; border-radius: 4px; padding: 6px 12px; "
        "font-weight: 600; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }";

    QString normalStyle =
        "QPushButton { background-color: #f5f5f5; color: #333; "
        "border: 1px solid #e0e0e0; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #eeeeee; }"
        "QPushButton:pressed { background-color: #e0e0e0; }";

    m_dayViewBtn->setStyleSheet(m_currentMode == DayView ? selectedStyle : normalStyle);
    m_weekViewBtn->setStyleSheet(m_currentMode == WeekView ? selectedStyle : normalStyle);
    m_monthViewBtn->setStyleSheet(m_currentMode == MonthView ? selectedStyle : normalStyle);
}