#pragma once

#include <QFrame>
#include <QDate>
#include <QSet>

class ElaText;
class ElaToolButton;
class ElaPushButton;
class QGridLayout;

class ConversationHistoryCalendarWidget : public QFrame {
    Q_OBJECT

public:
    explicit ConversationHistoryCalendarWidget(QWidget* parent = nullptr);

    void setActiveDates(const QSet<QDate>& dates);
    QDate selectedDate() const { return m_selectedDate; }

signals:
    void dateSelected(const QDate& date);

private slots:
    void navigatePrev();
    void navigateNext();
    void goToToday();

private:
    void rebuildGrid();
    void updateMonthLabel();

    ElaText* m_monthLabel = nullptr;
    ElaToolButton* m_prevButton = nullptr;
    ElaToolButton* m_nextButton = nullptr;
    ElaPushButton* m_todayButton = nullptr;
    QGridLayout* m_gridLayout = nullptr;
    ElaToolButton* m_dayCells[42] = {};

    QDate m_viewMonth;       // 当前显示月份的第一天
    QDate m_selectedDate;
    QSet<QDate> m_activeDates;
};
