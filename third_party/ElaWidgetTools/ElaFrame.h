#ifndef ELAFRAME_H
#define ELAFRAME_H

#include <QFrame>

#include "ElaDef.h"
#include "ElaProperty.h"

class ELA_EXPORT ElaFrame : public QFrame
{
    Q_OBJECT
public:
    explicit ElaFrame(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~ElaFrame() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ElaThemeType::ThemeMode _themeMode;
};

#endif // ELAFRAME_H
