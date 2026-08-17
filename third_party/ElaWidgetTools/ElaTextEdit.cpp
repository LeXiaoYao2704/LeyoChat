#include "ElaTextEdit.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QPainter>
#include <QPropertyAnimation>

#include "ElaEventBus.h"
#include "ElaMenu.h"
#include "ElaScrollBar.h"
#include "ElaTextEditPrivate.h"
#include "ElaTextEditStyle.h"
#include "ElaTheme.h"

ElaTextEdit::ElaTextEdit(QWidget* parent)
    : QTextEdit(parent), d_ptr(new ElaTextEditPrivate())
{
    Q_D(ElaTextEdit);
    d->q_ptr = this;
    setObjectName("ElaTextEdit");
    setStyleSheet("#ElaTextEdit{background-color:transparent;}");
    setHorizontalScrollBar(new ElaScrollBar(this));
    setVerticalScrollBar(new ElaScrollBar(this));
    setMouseTracking(true);
    // 事件总线
    d->_focusEvent = new ElaEvent("WMWindowClicked", "onWMWindowClickedEvent", d);
    d->_focusEvent->registerAndInit();

    d->_style = new ElaTextEditStyle(style());
    setStyle(d->_style);
    d->onThemeChanged(eTheme->getThemeMode());
    connect(eTheme, &ElaTheme::themeModeChanged, d, &ElaTextEditPrivate::onThemeChanged);
}

ElaTextEdit::ElaTextEdit(const QString& text, QWidget* parent)
    : ElaTextEdit(parent)
{
    setText(text);
}

ElaTextEdit::~ElaTextEdit()
{
    Q_D(ElaTextEdit);
    delete d->_style;
}

void ElaTextEdit::focusInEvent(QFocusEvent* event)
{
    Q_D(ElaTextEdit);
    if (event->reason() == Qt::MouseFocusReason)
    {
        QPropertyAnimation* markAnimation = new QPropertyAnimation(d->_style, "pExpandMarkWidth");
        connect(markAnimation, &QPropertyAnimation::valueChanged, this, [=](const QVariant& value) {
            update();
        });
        markAnimation->setDuration(300);
        markAnimation->setEasingCurve(QEasingCurve::InOutSine);
        markAnimation->setStartValue(d->_style->getExpandMarkWidth());
        markAnimation->setEndValue(width() / 2 - 3);
        markAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }
    QTextEdit::focusInEvent(event);
}

void ElaTextEdit::focusOutEvent(QFocusEvent* event)
{
    Q_D(ElaTextEdit);
    if (event->reason() != Qt::PopupFocusReason)
    {
        QPropertyAnimation* markAnimation = new QPropertyAnimation(d->_style, "pExpandMarkWidth");
        connect(markAnimation, &QPropertyAnimation::valueChanged, this, [=](const QVariant& value) {
            update();
        });
        markAnimation->setDuration(300);
        markAnimation->setEasingCurve(QEasingCurve::InOutSine);
        markAnimation->setStartValue(d->_style->getExpandMarkWidth());
        markAnimation->setEndValue(0);
        markAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }
    QTextEdit::focusOutEvent(event);
}

void ElaTextEdit::contextMenuEvent(QContextMenuEvent* event)
{
    ElaMenu* menu = new ElaMenu(this);
    menu->setMenuItemHeight(27);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction* action = nullptr;
    if (!isReadOnly())
    {
        action = menu->addElaIconAction(ElaIconType::ArrowRotateLeft, "撤销", QKeySequence::Undo);
        action->setEnabled(document()->isUndoAvailable());
        connect(action, &QAction::triggered, this, &ElaTextEdit::undo);

        action = menu->addElaIconAction(ElaIconType::ArrowRotateRight, "恢复", QKeySequence::Redo);
        action->setEnabled(document()->isRedoAvailable());
        connect(action, &QAction::triggered, this, &ElaTextEdit::redo);
        menu->addSeparator();
    }
#ifndef QT_NO_CLIPBOARD
    if (!isReadOnly())
    {
        action = menu->addElaIconAction(ElaIconType::KnifeKitchen, "剪切", QKeySequence::Cut);
        action->setEnabled(!isReadOnly() && !textCursor().selectedText().isEmpty());
        connect(action, &QAction::triggered, this, &ElaTextEdit::cut);
    }

    action = menu->addElaIconAction(ElaIconType::Copy, "复制", QKeySequence::Copy);
    action->setEnabled(!textCursor().selectedText().isEmpty());
    connect(action, &QAction::triggered, this, &ElaTextEdit::copy);

    if (!isReadOnly())
    {
        action = menu->addElaIconAction(ElaIconType::Paste, "粘贴", QKeySequence::Paste);
        action->setEnabled(!isReadOnly() && !QGuiApplication::clipboard()->text().isEmpty());
        connect(action, &QAction::triggered, this, &ElaTextEdit::paste);
    }
#endif
    if (!isReadOnly())
    {
        action = menu->addElaIconAction(ElaIconType::DeleteLeft, "删除");
        action->setEnabled(!isReadOnly() && !toPlainText().isEmpty() && !textCursor().selectedText().isEmpty());
        connect(action, &QAction::triggered, this, [=](bool checked) {
            if (!textCursor().selectedText().isEmpty())
            {
                textCursor().deleteChar();
            }
        });
    }
    if (!menu->isEmpty())
    {
        menu->addSeparator();
    }
    action = menu->addAction("全选");
    action->setShortcut(QKeySequence::SelectAll);
    action->setEnabled(!toPlainText().isEmpty() && !(textCursor().selectedText() == toPlainText()));
    connect(action, &QAction::triggered, this, &ElaTextEdit::selectAll);
    menu->popup(event->globalPos());
    this->setFocus();
}

void ElaTextEdit::paintEvent(QPaintEvent* event)
{
    Q_D(ElaTextEdit);
    if (palette().color(QPalette::Text) != ElaThemeColor(d->_themeMode, BasicText))
    {
        d->onThemeChanged(d->_themeMode);
    }
    QTextEdit::paintEvent(event);
}
