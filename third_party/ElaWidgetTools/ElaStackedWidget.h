#ifndef ELASTACKEDWIDGET_H
#define ELASTACKEDWIDGET_H

#include <QStackedWidget>

#include "ElaDef.h"
#include "ElaProperty.h"

class ELA_EXPORT ElaStackedWidget : public QStackedWidget
{
    Q_OBJECT
public:
    explicit ElaStackedWidget(QWidget* parent = nullptr);
    ~ElaStackedWidget() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ElaThemeType::ThemeMode _themeMode;
};

#endif // ELASTACKEDWIDGET_H
