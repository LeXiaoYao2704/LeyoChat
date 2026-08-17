#include "StorageCleanupDialog.h"
#include "AppStyle.h"

#include <ElaCheckBox.h>
#include <ElaPushButton.h>
#include <ElaText.h>
#include <ElaTheme.h>

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

StorageCleanupDialog::StorageCleanupDialog(StorageCategory category,
                                           const QList<CleanupItem>& items,
                                           QWidget* parent)
    : ElaDialog(parent)
    , category_(category)
    , items_(items)
{
    setWindowTitle(QStringLiteral("管理 - %1").arg(categoryTitle()));
    setMinimumSize(640, 420);
    resize(720, 500);

    // 对话框背景跟随主题，支持深色模式和透明度
    auto applyDialogBg = [this]() {
        setStyleSheet(QStringLiteral("StorageCleanupDialog { background:%1; }")
                          .arg(AppStyle::windowBg()));
    };
    applyDialogBg();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [applyDialogBg](ElaThemeType::ThemeMode) {
        applyDialogBg();
    });

    setupUi();
}

QString StorageCleanupDialog::categoryTitle() const
{
    switch (category_) {
    case StorageCategory::Logs:     return QStringLiteral("日志文件");
    case StorageCategory::Messages: return QStringLiteral("聊天消息");
    case StorageCategory::Files:    return QStringLiteral("接收的文件");
    case StorageCategory::Images:   return QStringLiteral("图片/截图");
    }
    return QString();
}

QString StorageCleanupDialog::formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(static_cast<double>(bytes) / (1024.0 * 1024), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(static_cast<double>(bytes) / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

void StorageCleanupDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 顶部：全选 + 数量说明
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);
    selectAllCheck_ = new ElaCheckBox(QStringLiteral("\u5168\u9009"), this);
    selectAllCheck_->setTristate(true);
    topRow->addWidget(selectAllCheck_);
    summaryLabel_ = new ElaText(this);
    summaryLabel_->setTextPixelSize(12);
    topRow->addWidget(summaryLabel_, 1);
    mainLayout->addLayout(topRow);

    // 文件列表表格
    tableWidget_ = new QTableWidget(this);
    tableWidget_->setColumnCount(4);
    tableWidget_->setHorizontalHeaderLabels({
        QStringLiteral(""),
        QStringLiteral("名称"),
        QStringLiteral("大小"),
        QStringLiteral("修改时间")
    });
    tableWidget_->horizontalHeader()->setStretchLastSection(true);
    tableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    tableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
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
    tableWidget_->setRowCount(items_.size());
    for (int i = 0; i < items_.size(); ++i) {
        auto* checkBox = new ElaCheckBox(this);
        checkBox->setChecked(items_[i].selected);
        connect(checkBox, &ElaCheckBox::toggled, this, [this, i](bool checked) {
            items_[i].selected = checked;
            updateSelectionSummary();
            syncSelectAllState();
        });
        tableWidget_->setCellWidget(i, 0, checkBox);

        auto* nameItem = new QTableWidgetItem(items_[i].name);
        tableWidget_->setItem(i, 1, nameItem);

        auto* sizeItem = new QTableWidgetItem(formatSize(items_[i].sizeBytes));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tableWidget_->setItem(i, 2, sizeItem);

        auto* dateItem = new QTableWidgetItem(
            items_[i].lastModified.toString(QStringLiteral("yyyy-MM-dd HH:mm")));
        tableWidget_->setItem(i, 3, dateItem);
    }
    mainLayout->addWidget(tableWidget_, 1);

    // 全选逻辑
    // 使用 stateChanged(int) 而非 toggled —— tristate 模式下 Unchecked→PartiallyChecked
    // 时 toggled 不触发（isChecked() 未变化），会导致全选操作失效。
    // 遇到 PartiallyChecked 状态（仅出现在用户从 Unchecked 点击一次时）立即提升到 Checked。
    connect(selectAllCheck_, &QCheckBox::stateChanged, this, [this](int state) {
        if (state == Qt::PartiallyChecked) {
            // 将 "部分选中" 状态强制提升为 "全选"（同微信行为）
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
                items_[i].selected = checked;
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

    deleteBtn_ = new ElaPushButton(this);
    deleteBtn_->setText(QStringLiteral("删除所选"));
    deleteBtn_->setFixedHeight(36);
    deleteBtn_->setBorderRadius(10);
    deleteBtn_->setEnabled(false);
    connect(deleteBtn_, &ElaPushButton::clicked, this, [this]() {
        emit deleteRequested(category_, selectedIndices());
        accept();
    });
    bottomRow->addWidget(deleteBtn_);
    mainLayout->addLayout(bottomRow);

    updateSelectionSummary();
}

void StorageCleanupDialog::updateSelectionSummary()
{
    int count = 0;
    qint64 totalSize = 0;
    for (const auto& item : items_) {
        if (item.selected) {
            ++count;
            totalSize += item.sizeBytes;
        }
    }
    summaryLabel_->setText(QStringLiteral("已选 %1 项，共 %2")
                               .arg(count)
                               .arg(formatSize(totalSize)));
    deleteBtn_->setEnabled(count > 0);
}

QList<int> StorageCleanupDialog::selectedIndices() const
{
    QList<int> result;
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].selected)
            result.append(i);
    }
    return result;
}

void StorageCleanupDialog::applyTableStyle()
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

void StorageCleanupDialog::syncSelectAllState()
{
    int selectedCount = 0;
    const int total = items_.size();
    for (const auto& item : items_) {
        if (item.selected) ++selectedCount;
    }
    // 使用 blockSignals 防止触发全选的 toggled → 再次遍历子项
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
