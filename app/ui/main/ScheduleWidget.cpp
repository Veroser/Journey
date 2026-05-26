#include "ScheduleWidget.h"
#include "../../core/schedule/ScheduleManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QStyleFactory>
#include <QDate>
#include <QDebug>

ScheduleWidget::ScheduleWidget(ScheduleManager *scheduleManager, QWidget *parent)
    : QWidget(parent), m_scheduleManager(scheduleManager),
      m_currentMode(DayView), m_currentDate(QDate::currentDate())
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

    m_scheduleManager->loadDaySchedule(m_currentDate);
}

void ScheduleWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    m_dateLabel = new QLabel();
    m_dateLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #333;");
    headerLayout->addWidget(m_dateLabel);

    m_viewModeCombo = new QComboBox();
    m_viewModeCombo->addItem("День", DayView);
    m_viewModeCombo->addItem("Неделя", WeekView);
    m_viewModeCombo->addItem("Месяц", MonthView);
    m_viewModeCombo->setMaximumWidth(120);
    connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScheduleWidget::onViewModeChanged);
    headerLayout->addWidget(m_viewModeCombo);

    headerLayout->addStretch();

    //m_syncStatusLabel = new QLabel("✓ Синхронизировано");
    //m_syncStatusLabel->setStyleSheet("color: #666; font-size: 12px;");
    //headerLayout->addWidget(m_syncStatusLabel);

    mainLayout->addLayout(headerLayout);

    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setSpacing(8);

    m_prevButton = new QPushButton("◀ Назад");
    m_nextButton = new QPushButton("Вперёд ▶");
    m_todayButton = new QPushButton("Сегодня");
    //m_syncButton = new QPushButton("⟳ Синхронизировать");

    connect(m_prevButton, &QPushButton::clicked, this, &ScheduleWidget::onPreviousDate);
    connect(m_nextButton, &QPushButton::clicked, this, &ScheduleWidget::onNextDate);
    connect(m_todayButton, &QPushButton::clicked, [this]() {
        m_currentDate = QDate::currentDate();
        onDateSelected(m_currentDate);
    });
    //connect(m_syncButton, &QPushButton::clicked, [this]() {
    //    m_scheduleManager->syncSchedule();
    //});

    navLayout->addWidget(m_prevButton);
    navLayout->addWidget(m_nextButton);
    navLayout->addWidget(m_todayButton);
    navLayout->addStretch();
    //navLayout->addWidget(m_syncButton);

    mainLayout->addLayout(navLayout);

    m_lessonsListWidget = new QListWidget();
    m_lessonsListWidget->setStyleSheet(
        "QListWidget { border: 1px solid #e0e0e0; border-radius: 4px; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #f0f0f0; }"
        "QListWidget::item:selected { background-color: #f5f5f5; }"
    );
    mainLayout->addWidget(m_lessonsListWidget);

    setLayout(mainLayout);
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

void ScheduleWidget::onViewModeChanged(int index)
{
    m_currentMode = static_cast<ViewMode>(m_viewModeCombo->itemData(index).toInt());

    switch (m_currentMode) {
        case DayView:
            updateDayView(m_currentDate);
            break;
        case WeekView:
            updateWeekView(m_currentDate.addDays(-(m_currentDate.dayOfWeek() - 1)));
            break;
        case MonthView:
            updateMonthView(m_currentDate);
            break;
    }
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

    m_lessonsListWidget->clear();
    m_scheduleManager->loadWeekSchedule(startDate);
}

void ScheduleWidget::updateMonthView(const QDate &month)
{
    QLocale russian(QLocale::Russian);
    m_dateLabel->setText(russian.toString(month, "yyyy MMMM"));
    m_lessonsListWidget->clear();
    m_scheduleManager->loadMonthSchedule(month);
}

void ScheduleWidget::displayLessons(const QVector<Lesson> &lessons)
{
    m_lessonsListWidget->clear();

    if (lessons.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem("Нет занятий");
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        m_lessonsListWidget->addItem(item);
        return;
    }

    for (const Lesson &lesson : lessons) {
        QString timeStr = QString("%1 - %2")
            .arg(lesson.startTime.toString("HH:mm"))
            .arg(lesson.endTime.toString("HH:mm"));

        QString text = QString("%1\n%2 | Аудитория: %3 | Преп.: %4")
            .arg(lesson.subject)
            .arg(timeStr)
            .arg(lesson.classroom)
            .arg(lesson.teacher);

        QListWidgetItem *item = new QListWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        m_lessonsListWidget->addItem(item);
    }
}

void ScheduleWidget::onDayScheduleLoaded(const DaySchedule &schedule)
{
    if (schedule.date == m_currentDate && m_currentMode == DayView) {
        displayLessons(schedule.lessons);
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
