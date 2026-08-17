#ifndef ELALISTWIDGET_H
#define ELALISTWIDGET_H

#include <QListWidget>

#include "ElaProperty.h"

class ElaListWidgetPrivate;
class ELA_EXPORT ElaListWidget : public QListWidget
{
    Q_OBJECT
    Q_Q_CREATE(ElaListWidget)
    Q_PROPERTY_CREATE_Q_H(bool, IsTransparent)
public:
    explicit ElaListWidget(QWidget* parent = nullptr);
    ~ElaListWidget();
};

#endif // ELALISTWIDGET_H
