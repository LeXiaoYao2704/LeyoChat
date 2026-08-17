#include "ui/ConversationHistoryCalendarWidget.h"
#include "ui/AppStyle.h"

#include <ElaToolButton.h>
#include <ElaPushButton.h>
#include <ElaText.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

ConversationHistoryCalendarWidget::ConversationHistoryCalendarWidget(QWidget* parent)
    : QFrame(parent)
    , m_viewMonth(QDate::currentDate().year(), QDate::currentDate().month(), 1)
{
    setFrameShape(QFrame::NoFrame);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(4, 0, 4, 0);

    m_prevButton = new ElaToolButton(this);
    m_prevButton->setElaIcon(ElaIconType::AngleLeft);
    m_prevButton->setFixedSize(28, 28);
    m_prevButton->setIsTransparent(true);

    m_nextButton = new ElaToolButton(this);
    m_nextButton->setElaIcon(ElaIconType::AngleRight);
    m_nextButton->setFixedSize(28, 28);
    m_nextButton->setIsTransparent(true);

    m_monthLabel = new ElaText(this);
    m_monthLabel->setAlignment(Qt::AlignCenter);
    m_monthLabel->setStyleSheet(QStringLiteral("font-size:14px; font-weight:600; color:%1;")
                                    .arg(AppStyle::textPrimary()));

    topBar->addWidget(m_prevButton);
    topBar->addStretch();
    topBar->addWidget(m_monthLabel);
    topBar->addStretch();
    topBar->addWidget(m_nextButton);

    // 星期头
    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(0);
    static const QString dayNames[] = {
        QStringLiteral("日"), QStringLiteral("一"), QStringLiteral("二"),
        QStringLiteral("三"), QStringLiteral("四"), QStringLiteral("五"),
        QStringLiteral("六")
    };
    for (int i = 0; i < 7; ++i) {
        auto* lbl = new ElaText(dayNames[i], this);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedSize(36, 24);
        lbl->setStyleSheet(QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
        headerRow->addWidget(lbl);
    }

    // 日期网格 7x6
    m_gridLayout = new QGridLayout;
    m_gridLayout->setSpacing(2);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 42; ++i) {
        auto* btn = new ElaToolButton(this);
        btn->setFixedSize(36, 32);
        btn->setCursor(Qt::PointingHandCursor);
        m_dayCells[i] = btn;
        m_gridLayout->addWidget(btn, i / 7, i % 7);
        connect(btn, &QToolButton::clicked, this, [this, i]() {
            const QString text = m_dayCells[i]->text();
            if (text.isEmpty()) return;
            const int day = text.toInt();
            if (day < 1) return;
            QDate clicked(m_viewMonth.year(), m_viewMonth.month(), day);
            if (clicked.isValid()) {
                m_selectedDate = clicked;
                rebuildGrid();
                emit dateSelected(clicked);
            }
        });
    }

    // 回到今天
    m_todayButton = new ElaPushButton(QStringLiteral("回到今天"), this);
    m_todayButton->setFixedHeight(28);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);
    mainLayout->addLayout(topBar);
    mainLayout->addLayout(headerRow);
    mainLayout->addLayout(m_gridLayout);
    mainLayout->addWidget(m_todayButton, 0, Qt::AlignCenter);

    connect(m_prevButton, &ElaToolButton::clicked, this, &ConversationHistoryCalendarWidget::navigatePrev);
    connect(m_nextButton, &ElaToolButton::clicked, this, &ConversationHistoryCalendarWidget::navigateNext);
    connect(m_todayButton, &ElaPushButton::clicked, this, &ConversationHistoryCalendarWidget::goToToday);

    updateMonthLabel();
    rebuildGrid();
}

void ConversationHistoryCalendarWidget::setActiveDates(const QSet<QDate>& dates)
{
    m_activeDates = dates;
    rebuildGrid();
}

void ConversationHistoryCalendarWidget::navigatePrev()
{
    m_viewMonth = m_viewMonth.addMonths(-1);
    updateMonthLabel();
    rebuildGrid();
}

void ConversationHistoryCalendarWidget::navigateNext()
{
    m_viewMonth = m_viewMonth.addMonths(1);
    updateMonthLabel();
    rebuildGrid();
}

void ConversationHistoryCalendarWidget::goToToday()
{
    const QDate today = QDate::currentDate();
    m_viewMonth = QDate(today.year(), today.month(), 1);
    m_selectedDate = today;
    updateMonthLabel();
    rebuildGrid();
    emit dateSelected(today);
}

void ConversationHistoryCalendarWidget::updateMonthLabel()
{
    m_monthLabel->setText(QStringLiteral("%1年%2月")
                             .arg(m_viewMonth.year())
                             .arg(m_viewMonth.month()));
}

void ConversationHistoryCalendarWidget::rebuildGrid()
{
    const int daysInMonth = m_viewMonth.daysInMonth();
    // Qt: dayOfWeek() 返回 1=周一 ... 7=周日，转为 0=周日
    const int firstWeekday = (m_viewMonth.dayOfWeek() % 7);
    const QDate today = QDate::currentDate();

    const QString accentColor = AppStyle::accent();
    const QString textColor = AppStyle::textPrimary();
    const QString mutedColor = AppStyle::textMuted();
    const QString hoverColor = AppStyle::hoverBg();
    const QString surfaceColor = AppStyle::surface();
    const QString borderColor = AppStyle::border();

    for (int i = 0; i < 42; ++i) {
        ElaToolButton* btn = m_dayCells[i];
        const int dayNum = i - firstWeekday + 1;
        if (dayNum < 1 || dayNum > daysInMonth) {
            btn->setText(QString());
            btn->setEnabled(false);
            btn->setStyleSheet(QStringLiteral("QToolButton { background:transparent; border:none; }"));
            continue;
        }

        btn->setText(QString::number(dayNum));
        btn->setEnabled(true);

        const QDate cellDate(m_viewMonth.year(), m_viewMonth.month(), dayNum);
        const bool isSelected = (cellDate == m_selectedDate);
        const bool isToday = (cellDate == today);
        const bool hasMessages = m_activeDates.contains(cellDate);

        QString style;
        if (isSelected) {
            style = QStringLiteral(
                "QToolButton { background:%1; color:#FFFFFF; border-radius:6px; border:none; font-weight:600; }"
                "QToolButton:hover { background:%2; }")
                        .arg(accentColor, AppStyle::accentHover());
        } else if (isToday) {
            style = QStringLiteral(
                "QToolButton { background:transparent; color:%1; border:2px solid %1; border-radius:6px; font-weight:600; }"
                "QToolButton:hover { background:%2; }")
                        .arg(accentColor, hoverColor);
        } else if (hasMessages) {
            style = QStringLiteral(
                "QToolButton { background:transparent; color:%1; border:none; border-radius:6px; font-weight:600; text-decoration:underline; }"
                "QToolButton:hover { background:%2; }")
                        .arg(accentColor, hoverColor);
        } else {
            style = QStringLiteral(
                "QToolButton { background:transparent; color:%1; border:none; border-radius:6px; }"
                "QToolButton:hover { background:%2; }")
                        .arg(textColor, hoverColor);
        }
        btn->setStyleSheet(style);
    }
}
