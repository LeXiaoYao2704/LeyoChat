#include "ContactSelectionDialog.h"
#include "AppStyle.h"

#include <ElaCheckBox.h>
#include <ElaPushButton.h>
#include <ElaText.h>
#include <ElaTheme.h>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

ContactSelectionDialog::ContactSelectionDialog(const QString& title,
                                               const QList<ContactEntry>& contacts,
                                               QWidget* parent)
    : ElaDialog(parent)
    , title_(title)
    , contacts_(contacts)
{
    setWindowTitle(title);
    setMinimumSize(500, 380);
    resize(560, 450);

    auto applyDialogBg = [this]() {
        setStyleSheet(QStringLiteral("ContactSelectionDialog { background:%1; }")
                          .arg(AppStyle::windowBg()));
    };
    applyDialogBg();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [applyDialogBg](ElaThemeType::ThemeMode) {
        applyDialogBg();
    });

    setupUi();
}

QString ContactSelectionDialog::formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

void ContactSelectionDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 顶部提示
    auto* hintLabel = new ElaText(this);
    hintLabel->setText(QStringLiteral("选择要管理的联系人（可多选）："));
    hintLabel->setTextPixelSize(13);
    mainLayout->addWidget(hintLabel);

    // 全选 + 摘要
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);
    selectAllCheck_ = new ElaCheckBox(QStringLiteral("全选"), this);
    selectAllCheck_->setTristate(true);
    topRow->addWidget(selectAllCheck_);
    summaryLabel_ = new ElaText(this);
    summaryLabel_->setTextPixelSize(12);
    topRow->addWidget(summaryLabel_, 1);
    mainLayout->addLayout(topRow);

    // 联系人列表表格
    tableWidget_ = new QTableWidget(this);
    tableWidget_->setColumnCount(4);
    tableWidget_->setHorizontalHeaderLabels({
        QStringLiteral(""),
        QStringLiteral("联系人"),
        QStringLiteral("数量"),
        QStringLiteral("大小")
    });
    tableWidget_->horizontalHeader()->setStretchLastSection(true);
    tableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tableWidget_->setColumnWidth(0, 36);
    tableWidget_->verticalHeader()->setVisible(false);
    tableWidget_->setSelectionMode(QAbstractItemView::NoSelection);
    tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget_->setAlternatingRowColors(true);

    applyTableStyle();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this](ElaThemeType::ThemeMode) {
        applyTableStyle();
    });

    // 填充数据
    tableWidget_->setRowCount(contacts_.size());
    for (int i = 0; i < contacts_.size(); ++i) {
        auto* checkBox = new ElaCheckBox(this);
        checkBox->setChecked(contacts_[i].selected);
        connect(checkBox, &ElaCheckBox::toggled, this, [this, i](bool checked) {
            contacts_[i].selected = checked;
            updateSelectionSummary();
            syncSelectAllState();
        });
        tableWidget_->setCellWidget(i, 0, checkBox);

        auto* nameItem = new QTableWidgetItem(contacts_[i].name);
        tableWidget_->setItem(i, 1, nameItem);

        auto* countItem = new QTableWidgetItem(
            QStringLiteral("%1 项").arg(contacts_[i].itemCount));
        countItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tableWidget_->setItem(i, 2, countItem);

        auto* sizeItem = new QTableWidgetItem(formatSize(contacts_[i].sizeBytes));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tableWidget_->setItem(i, 3, sizeItem);
    }
    mainLayout->addWidget(tableWidget_, 1);

    // 全选逻辑
    connect(selectAllCheck_, &QCheckBox::stateChanged, this, [this](int state) {
        if (state == Qt::PartiallyChecked) {
            selectAllCheck_->blockSignals(true);
            selectAllCheck_->setCheckState(Qt::Checked);
            selectAllCheck_->blockSignals(false);
            state = Qt::Checked;
        }
        const bool checked = (state == Qt::Checked);
        for (int i = 0; i < tableWidget_->rowCount(); ++i) {
            auto* cb = qobject_cast<ElaCheckBox*>(tableWidget_->cellWidget(i, 0));
            if (cb) {
                cb->blockSignals(true);
                cb->setChecked(checked);
                cb->blockSignals(false);
                contacts_[i].selected = checked;
            }
        }
        updateSelectionSummary();
    });

    // 底部按钮行
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(12);
    bottomRow->addStretch();

    cancelBtn_ = new ElaPushButton(this);
    cancelBtn_->setText(QStringLiteral("取消"));
    cancelBtn_->setFixedHeight(36);
    cancelBtn_->setBorderRadius(10);
    connect(cancelBtn_, &ElaPushButton::clicked, this, &QDialog::reject);
    bottomRow->addWidget(cancelBtn_);

    confirmBtn_ = new ElaPushButton(this);
    confirmBtn_->setText(QStringLiteral("下一步"));
    confirmBtn_->setFixedHeight(36);
    confirmBtn_->setBorderRadius(10);
    confirmBtn_->setEnabled(false);
    connect(confirmBtn_, &ElaPushButton::clicked, this, [this]() {
        emit selectionConfirmed(selectedIndices());
        accept();
    });
    bottomRow->addWidget(confirmBtn_);
    mainLayout->addLayout(bottomRow);

    updateSelectionSummary();
}

void ContactSelectionDialog::updateSelectionSummary()
{
    int count = 0;
    qint64 totalSize = 0;
    for (const auto& c : contacts_) {
        if (c.selected) {
            ++count;
            totalSize += c.sizeBytes;
        }
    }
    summaryLabel_->setText(QStringLiteral("已选 %1 位联系人，共 %2")
                               .arg(count)
                               .arg(formatSize(totalSize)));
    confirmBtn_->setEnabled(count > 0);
}

QList<int> ContactSelectionDialog::selectedIndices() const
{
    QList<int> result;
    for (int i = 0; i < contacts_.size(); ++i) {
        if (contacts_[i].selected)
            result.append(i);
    }
    return result;
}

void ContactSelectionDialog::applyTableStyle()
{
    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    const bool dark = AppStyle::toElaThemeMode(mode) == ElaThemeType::Dark;

    const QString headerBg  = dark ? QStringLiteral("#1e2530") : QStringLiteral("#f4f6f8");
    const QString rowBg     = dark ? QStringLiteral("#181f27") : QStringLiteral("#ffffff");
    const QString rowAlt    = dark ? QStringLiteral("#1c2531") : QStringLiteral("#f7f9fb");
    const QString textColor = AppStyle::textPrimary(mode);
    const QString border    = AppStyle::border(mode);
    const QString selBg     = dark ? QStringLiteral("#2a3545") : QStringLiteral("#e6f0fa");

    tableWidget_->setStyleSheet(QStringLiteral(
        "QTableWidget {"
        "  background:%1; color:%2;"
        "  border:1px solid %3; border-radius:8px;"
        "  gridline-color:%3;"
        "}"
        "QTableWidget::item { padding:4px 6px; color:%2; }"
        "QTableWidget::item:alternate { background:%4; }"
        "QTableWidget::item:selected { background:%5; }"
        "QHeaderView::section {"
        "  background:%6; color:%2;"
        "  border:none; border-bottom:1px solid %3;"
        "  padding:4px 8px; font-weight:600;"
        "}"
        "QScrollBar:vertical { background:transparent; width:8px; border-radius:4px; }"
        "QScrollBar::handle:vertical { background:%3; border-radius:4px; min-height:24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
    ).arg(rowBg, textColor, border, rowAlt, selBg, headerBg));
}

void ContactSelectionDialog::syncSelectAllState()
{
    int selectedCount = 0;
    const int total = contacts_.size();
    for (const auto& c : contacts_) {
        if (c.selected) ++selectedCount;
    }
    selectAllCheck_->blockSignals(true);
    if (selectedCount == 0) {
        selectAllCheck_->setCheckState(Qt::Unchecked);
    } else if (selectedCount == total) {
        selectAllCheck_->setCheckState(Qt::Checked);
    } else {
        selectAllCheck_->setCheckState(Qt::PartiallyChecked);
    }
    selectAllCheck_->blockSignals(false);
}
