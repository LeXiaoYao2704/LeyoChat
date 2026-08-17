#include "ElaFrame.h"

#include <QPainter>
#include <QPainterPath>

#include "ElaTheme.h"

ElaFrame::ElaFrame(QWidget* parent, Qt::WindowFlags f)
    : QFrame(parent, f)
{
    _themeMode = eTheme->getThemeMode();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        _themeMode = themeMode;
        update();
    });
}

ElaFrame::~ElaFrame()
{
}

void ElaFrame::paintEvent(QPaintEvent* event)
{
    int fShape = frameShape();
    if (fShape == QFrame::HLine || fShape == QFrame::VLine)
    {
        // 分隔线：使用主题边框色
        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing);
        painter.setPen(ElaThemeColor(_themeMode, BasicBorder));
        if (fShape == QFrame::HLine)
        {
            painter.drawLine(0, height() / 2, width(), height() / 2);
        }
        else
        {
            painter.drawLine(width() / 2, 0, width() / 2, height());
        }
        return;
    }

    if (fShape == QFrame::Box || fShape == QFrame::Panel || fShape == QFrame::StyledPanel)
    {
        // 卡片/面板容器：主题感知背景+圆角+边框
        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing);
        QRectF frameRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

        // 背景
        painter.setPen(Qt::NoPen);
        painter.setBrush(ElaThemeColor(_themeMode, BasicBase));
        painter.drawRoundedRect(frameRect, 6, 6);

        // 边框
        if (lineWidth() > 0)
        {
            painter.setPen(QPen(ElaThemeColor(_themeMode, BasicBorder), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(frameRect, 6, 6);
        }
        return;
    }

    // NoFrame 或其他情况不做绘制，保持透明
    if (fShape != QFrame::NoFrame)
    {
        QFrame::paintEvent(event);
    }
}
