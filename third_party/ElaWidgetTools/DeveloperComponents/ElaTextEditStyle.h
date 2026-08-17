#ifndef ELATEXTEDITSTYLE_H
#define ELATEXTEDITSTYLE_H

#include <QProxyStyle>

#include "ElaDef.h"

class ElaTextEditStyle : public QProxyStyle
{
    Q_OBJECT
    Q_PROPERTY_CREATE(qreal, ExpandMarkWidth)
public:
    explicit ElaTextEditStyle(QStyle* style = nullptr);
    ~ElaTextEditStyle();
    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget = nullptr) const override;

private:
    ElaThemeType::ThemeMode _themeMode;
};

#endif // ELATEXTEDITSTYLE_H
