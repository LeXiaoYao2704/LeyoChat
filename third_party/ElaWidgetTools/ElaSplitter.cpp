#include "ElaSplitter.h"

#include <QPainter>
#include <QPainterPath>

#include "ElaTheme.h"

ElaSplitter::ElaSplitter(QWidget* parent)
    : QSplitter(parent)
{
    _initStyle();
}

ElaSplitter::ElaSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent)
{
    _initStyle();
}

ElaSplitter::~ElaSplitter()
{
}

void ElaSplitter::_initStyle()
{
    _themeMode = eTheme->getThemeMode();
    setHandleWidth(1);
    setChildrenCollapsible(false);
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        _themeMode = themeMode;
        update();
    });
}

QSplitterHandle* ElaSplitter::createHandle()
{
    return new ElaSplitterHandle(orientation(), this);
}

// --- ElaSplitterHandle ---

ElaSplitterHandle::ElaSplitterHandle(Qt::Orientation orientation, QSplitter* parent)
    : QSplitterHandle(orientation, parent)
{
    _themeMode = eTheme->getThemeMode();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        _themeMode = themeMode;
        update();
    });
}

void ElaSplitterHandle::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ElaThemeColor(_themeMode, BasicBorder));
    painter.drawRect(rect());
}
