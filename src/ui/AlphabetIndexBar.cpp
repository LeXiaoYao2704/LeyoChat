#include "ui/AlphabetIndexBar.h"

#include "ui/AppStyle.h"
#include "ui/ContactListModel.h"

#include <QListView>
#include <QMouseEvent>
#include <QPainter>

AlphabetIndexBar::AlphabetIndexBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(18);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);
}

void AlphabetIndexBar::setModel(ContactListModel* model)
{
    m_model = model;
    refresh();
}

void AlphabetIndexBar::setListView(QListView* listView)
{
    m_listView = listView;
}

void AlphabetIndexBar::refresh()
{
    if (!m_model) {
        m_letters.clear();
        update();
        return;
    }
    m_letters = m_model->sectionLetters();
    update();
}

void AlphabetIndexBar::paintEvent(QPaintEvent* /*event*/)
{
    if (m_letters.isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont font = this->font();
    font.setPixelSize(10);
    font.setBold(true);
    painter.setFont(font);

    const int count = m_letters.size();
    const int itemHeight = qMax(14, height() / qMax(1, count));
    const int startY = (height() - itemHeight * count) / 2;

    for (int i = 0; i < count; ++i) {
        const QRect rect(0, startY + i * itemHeight, width(), itemHeight);
        if (i == m_hoveredIndex) {
            painter.setPen(QColor(AppStyle::accent()));
        } else {
            painter.setPen(QColor(AppStyle::textMuted()));
        }
        // 缩短长分组名（如 "★ 收藏" → "★", "离线" → "离"）
        QString label = m_letters.at(i);
        if (label.size() > 2) {
            label = label.left(1);
        }
        painter.drawText(rect, Qt::AlignCenter, label);
    }
}

void AlphabetIndexBar::mousePressEvent(QMouseEvent* event)
{
    m_dragging = true;
    scrollToLetterAt(event->pos());
}

void AlphabetIndexBar::mouseMoveEvent(QMouseEvent* event)
{
    const int count = m_letters.size();
    if (count == 0) return;

    const int itemHeight = qMax(14, height() / qMax(1, count));
    const int startY = (height() - itemHeight * count) / 2;
    const int idx = (event->pos().y() - startY) / itemHeight;
    const int newHovered = (idx >= 0 && idx < count) ? idx : -1;

    if (newHovered != m_hoveredIndex) {
        m_hoveredIndex = newHovered;
        update();
    }

    if (m_dragging) {
        scrollToLetterAt(event->pos());
    }
}

void AlphabetIndexBar::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    m_dragging = false;
}

void AlphabetIndexBar::scrollToLetterAt(const QPoint& pos)
{
    if (!m_model || !m_listView || m_letters.isEmpty()) return;

    const int count = m_letters.size();
    const int itemHeight = qMax(14, height() / qMax(1, count));
    const int startY = (height() - itemHeight * count) / 2;
    const int idx = (pos.y() - startY) / itemHeight;

    if (idx < 0 || idx >= count) return;

    const int row = m_model->rowForSection(m_letters.at(idx));
    if (row >= 0) {
        m_listView->scrollTo(m_model->index(row, 0), QAbstractItemView::PositionAtTop);
    }
}
