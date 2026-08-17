#ifndef ELALISTWIDGETPRIVATE_H
#define ELALISTWIDGETPRIVATE_H

#include <QObject>

#include "ElaDef.h"

class ElaListWidget;
class ElaListViewStyle;
class ElaListWidgetPrivate : public QObject
{
    Q_OBJECT
    Q_D_CREATE(ElaListWidget)

public:
    explicit ElaListWidgetPrivate(QObject* parent = nullptr);
    ~ElaListWidgetPrivate() override;

private:
    ElaListViewStyle* _listViewStyle{nullptr};

    friend class ElaListWidget;
};

#endif // ELALISTWIDGETPRIVATE_H
