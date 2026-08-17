#include "ElaStackedWidget.h"

#include <QPainter>

#include "ElaTheme.h"

ElaStackedWidget::ElaStackedWidget(QWidget* parent)
    : QStackedWidget(parent)
{
    _themeMode = eTheme->getThemeMode();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        _themeMode = themeMode;
        update();
    });
}

ElaStackedWidget::~ElaStackedWidget()
{
}

void ElaStackedWidget::paintEvent(QPaintEvent* event)
{
    // 默认透明背景，主题切换时触发子控件刷新
    QStackedWidget::paintEvent(event);
}
