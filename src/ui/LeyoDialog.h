#pragma once

#include <ElaContentDialog.h>
#include <ElaMessageBar.h>
#include <ElaLineEdit.h>
#include <ElaComboBox.h>
#include <ElaPushButton.h>

#include <QFrame>
#include <ElaFrame.h>
#include <ElaText.h>
#include <QVBoxLayout>
#include <QWidget>

namespace LeyoDialog {

inline void applySecondaryDialogScaffold(QWidget* dialog, const QString& key)
{
    if (!dialog) {
        return;
    }
    dialog->setObjectName(QStringLiteral("secondaryPageScaffold_%1").arg(key));
    dialog->setProperty("pageScaffoldRole", QStringLiteral("secondary"));
    dialog->setProperty("pageScaffoldKey", key);
}

inline ElaFrame* createSectionFrame(QWidget* parent, const QString& objectName)
{
    auto* frame = new ElaFrame(parent);
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

// 替代 QMessageBox::question — 同步阻塞，返回 true 表示用户确认
inline bool question(QWidget* parent, const QString& title, const QString& text)
{
    ElaContentDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setLeftButtonText(QStringLiteral("\u53D6\u6D88"));
    dlg.setMiddleButtonText(QString());
    dlg.setRightButtonText(QStringLiteral("\u786E\u5B9A"));

    // 隐藏中间空按钮
    const auto buttons = dlg.findChildren<ElaPushButton*>();
    if (buttons.size() >= 3) {
        buttons[1]->hide();
    }

    auto* content = new QWidget(&dlg);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    auto* label = new ElaText(text, content);
    label->setTextStyle(ElaTextType::Body);
    label->setWordWrap(true);
    layout->addWidget(label);
    dlg.setCentralWidget(content);

    bool accepted = false;
    QObject::connect(&dlg, &ElaContentDialog::rightButtonClicked, &dlg, [&]() {
        accepted = true;
    });
    dlg.exec();
    return accepted;
}

// 替代 QMessageBox::information — 非模态顶部通知条
inline void information(QWidget* parent, const QString& title, const QString& text)
{
    ElaMessageBar::information(ElaMessageBarType::TopRight, title, text, 3000, parent);
}

// 替代 QMessageBox::warning — 非模态顶部警告通知
inline void warning(QWidget* parent, const QString& title, const QString& text)
{
    ElaMessageBar::warning(ElaMessageBarType::TopRight, title, text, 4000, parent);
}

// 替代 QMessageBox::critical
inline void error(QWidget* parent, const QString& title, const QString& text)
{
    ElaMessageBar::error(ElaMessageBarType::TopRight, title, text, 5000, parent);
}

// 替代 QInputDialog::getText — 同步阻塞
inline QString getText(QWidget* parent, const QString& title, const QString& label,
                       const QString& defaultValue = QString(), bool* ok = nullptr)
{
    ElaContentDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setLeftButtonText(QStringLiteral("\u53D6\u6D88"));
    dlg.setMiddleButtonText(QString());
    dlg.setRightButtonText(QStringLiteral("\u786E\u5B9A"));

    // 隐藏中间空按钮
    const auto buttons = dlg.findChildren<ElaPushButton*>();
    if (buttons.size() >= 3) {
        buttons[1]->hide();
    }

    auto* content = new QWidget(&dlg);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(12);
    auto* lbl = new ElaText(label, content);
    lbl->setTextStyle(ElaTextType::Body);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
    auto* edit = new ElaLineEdit(content);
    edit->setText(defaultValue);
    layout->addWidget(edit);
    dlg.setCentralWidget(content);

    bool accepted = false;
    QObject::connect(&dlg, &ElaContentDialog::rightButtonClicked, &dlg, [&]() {
        accepted = true;
    });
    dlg.exec();

    if (ok) *ok = accepted;
    return accepted ? edit->text() : QString();
}

// 替代 QInputDialog::getItem — 同步阻塞
inline QString getItem(QWidget* parent, const QString& title, const QString& label,
                       const QStringList& items, int current = 0, bool editable = false,
                       bool* ok = nullptr)
{
    Q_UNUSED(editable)
    ElaContentDialog dlg(parent);
    dlg.setWindowTitle(title);
    dlg.setLeftButtonText(QStringLiteral("\u53D6\u6D88"));
    dlg.setMiddleButtonText(QString());
    dlg.setRightButtonText(QStringLiteral("\u786E\u5B9A"));

    auto* content = new QWidget(&dlg);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setSpacing(12);
    auto* lbl = new ElaText(label, content);
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
    auto* combo = new ElaComboBox(content);
    combo->addItems(items);
    if (current >= 0 && current < items.size())
        combo->setCurrentIndex(current);
    layout->addWidget(combo);
    dlg.setCentralWidget(content);

    bool accepted = false;
    QObject::connect(&dlg, &ElaContentDialog::rightButtonClicked, &dlg, [&]() {
        accepted = true;
    });
    dlg.exec();

    if (ok) *ok = accepted;
    return accepted ? combo->currentText() : QString();
}

} // namespace LeyoDialog
