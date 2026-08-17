#ifndef ELASPLITTER_H
#define ELASPLITTER_H

#include <QSplitter>

#include "ElaDef.h"
#include "ElaProperty.h"

class ELA_EXPORT ElaSplitter : public QSplitter
{
    Q_OBJECT
public:
    explicit ElaSplitter(QWidget* parent = nullptr);
    explicit ElaSplitter(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~ElaSplitter() override;

protected:
    QSplitterHandle* createHandle() override;

private:
    void _initStyle();
    ElaThemeType::ThemeMode _themeMode;
};

class ELA_EXPORT ElaSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
public:
    explicit ElaSplitterHandle(Qt::Orientation orientation, QSplitter* parent);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ElaThemeType::ThemeMode _themeMode;
};

#endif // ELASPLITTER_H
