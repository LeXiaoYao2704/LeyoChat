#ifndef ELATEXTEDIT_H
#define ELATEXTEDIT_H

#include <QTextEdit>

#include "ElaProperty.h"

class ElaTextEditPrivate;
class ELA_EXPORT ElaTextEdit : public QTextEdit
{
    Q_OBJECT
    Q_Q_CREATE(ElaTextEdit)
public:
    explicit ElaTextEdit(QWidget* parent = nullptr);
    explicit ElaTextEdit(const QString& text, QWidget* parent = nullptr);
    ~ElaTextEdit() override;

protected:
    virtual void focusInEvent(QFocusEvent* event) override;
    virtual void focusOutEvent(QFocusEvent* event) override;
    virtual void contextMenuEvent(QContextMenuEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
};

#endif // ELATEXTEDIT_H
