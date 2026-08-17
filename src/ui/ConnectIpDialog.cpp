#include "ui/ConnectIpDialog.h"

#include "ui/AppStyle.h"
#include "ui/LeyoDialog.h"

#include <ElaLineEdit.h>
#include <ElaListWidget.h>
#include <ElaPushButton.h>
#include <ElaText.h>

#include <QHBoxLayout>
#include <QHostAddress>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

ConnectIpDialog::ConnectIpDialog(QWidget* parent)
    : ElaDialog(parent)
{
    LeyoDialog::applySecondaryDialogScaffold(this, QStringLiteral("connectIpDialog"));
    setWindowTitle(QStringLiteral("\u6DFB\u52A0\u8054\u7CFB\u4EBA"));
    setFixedWidth(460);

    const QString editStyle = QStringLiteral(
        "QLineEdit {"
        "  background:%1;"
        "  border:1.5px solid %2;"
        "  border-radius:8px;"
        "  padding:0 14px;"
        "  font-size:14px;"
        "  color:%3;"
        "}"
        "QLineEdit:focus {"
        "  border:1.5px solid %4;"
        "  background:%5;"
        "}").arg(AppStyle::surfaceAlt(),
                  AppStyle::border(),
                  AppStyle::textPrimary(),
                  AppStyle::accent(),
                  AppStyle::surface());

    m_hostEdit = new ElaLineEdit(this);
    m_hostEdit->setPlaceholderText(QStringLiteral("\u8F93\u5165 IP \u5730\u5740\u6216\u8054\u7CFB\u4EBA\u59D3\u540D"));
    m_hostEdit->setFixedHeight(44);
    m_hostEdit->setStyleSheet(editStyle);

    m_searchBtn = new ElaPushButton(QStringLiteral("\u641C\u7D22"), this);
    m_searchBtn->setFixedSize(72, 44);
    m_searchBtn->setCursor(Qt::PointingHandCursor);

    m_portEdit = new ElaLineEdit(this);
    m_portEdit->setPlaceholderText(QStringLiteral("45454"));
    m_portEdit->setText(QStringLiteral("45454"));
    m_portEdit->setFixedHeight(44);
    m_portEdit->setStyleSheet(editStyle);

    m_resultsList = new ElaListWidget(this);
    m_resultsList->setMinimumHeight(60);
    m_resultsList->setMaximumHeight(220);
    m_resultsList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  border:1px solid %1; border-radius:8px; background:%2; outline:none;"
        "}"
        "QListWidget::item { padding:6px 10px; border-bottom:1px solid %3; }"
        "QListWidget::item:hover { background:%4; }"
        ).arg(AppStyle::border(), AppStyle::surface(),
              AppStyle::surfaceAlt(), QStringLiteral("#F0F4FF")));
    m_resultsList->hide();

    m_okButton = new ElaPushButton(QStringLiteral("\u6DFB\u52A0"), this);
    m_okButton->setFixedHeight(44);
    m_okButton->setCursor(Qt::PointingHandCursor);

    auto* cancelBtn = new QPushButton(QStringLiteral("\u53D6\u6D88"), this);
    cancelBtn->setFlat(true);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color:%1; font-size:13px; border:none; background:transparent; }"
        "QPushButton:hover { color:%2; }").arg(AppStyle::textMuted(), AppStyle::accent()));

    // 右上角关闭按钮
    auto* closeBtn = new ElaPushButton(QStringLiteral("\u2715"), this);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:transparent; border:none; font-size:16px; color:%1; border-radius:14px; }"
        "QPushButton:hover { background:%2; color:%3; }")
        .arg(AppStyle::textMuted(), AppStyle::surfaceAlt(), AppStyle::textPrimary()));
    connect(closeBtn, &QAbstractButton::clicked, this, &QDialog::reject);

    auto* topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->addStretch();
    topRow->addWidget(closeBtn);

    auto* logoLabel = new ElaText(QStringLiteral("<span style='font-size:36px;'>&#128279;</span>"), this);
    logoLabel->setAlignment(Qt::AlignCenter);

    auto* titleLabel = new ElaText(QStringLiteral("\u6DFB\u52A0\u8054\u7CFB\u4EBA"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QStringLiteral("font-size:20px; font-weight:bold; color:%1;")
                                  .arg(AppStyle::textPrimary()));

    auto* subtitleLabel = new ElaText(
        QStringLiteral("\u8F93\u5165 IP \u76F4\u8FDE\uFF0C\u6216\u641C\u7D22\u59D3\u540D\u540E\u52FE\u9009\u6DFB\u52A0"), this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet(QStringLiteral("font-size:13px; color:%1;")
                                     .arg(AppStyle::textMuted()));

    const auto makeLabel = [this](const QString& text) {
        auto* lbl = new ElaText(text, this);
        lbl->setStyleSheet(QStringLiteral("font-size:13px; color:%1; font-weight:500;")
                               .arg(AppStyle::textPrimary()));
        return lbl;
    };

    auto* searchRow = new QHBoxLayout;
    searchRow->setSpacing(8);
    searchRow->addWidget(m_hostEdit, 1);
    searchRow->addWidget(m_searchBtn);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(cancelBtn);
    btnRow->addStretch();
    m_okButton->setFixedWidth(120);
    btnRow->addWidget(m_okButton);

    auto* inputSection = LeyoDialog::createSectionFrame(this, QStringLiteral("connectIpDialogInputSection"));
    inputSection->setStyleSheet(QStringLiteral(
        "QFrame#connectIpDialogInputSection {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:16px;"
        "}")
        .arg(AppStyle::surface(), AppStyle::border()));
    auto* inputSectionLayout = new QVBoxLayout(inputSection);
    inputSectionLayout->setContentsMargins(0, 0, 0, 0);
    inputSectionLayout->setSpacing(0);

    auto* inputArea = new QWidget(inputSection);
    inputArea->setObjectName(QStringLiteral("connectIpInputArea"));
    auto* inputLayout = new QVBoxLayout(inputArea);
    inputLayout->setContentsMargins(40, 12, 40, 32);
    inputLayout->setSpacing(0);
    inputLayout->addLayout(topRow);
    inputLayout->addWidget(logoLabel);
    inputLayout->addSpacing(8);
    inputLayout->addWidget(titleLabel);
    inputLayout->addSpacing(4);
    inputLayout->addWidget(subtitleLabel);
    inputLayout->addSpacing(24);
    inputLayout->addWidget(makeLabel(QStringLiteral("IP / \u59D3\u540D")));
    inputLayout->addSpacing(6);
    inputLayout->addLayout(searchRow);
    inputLayout->addSpacing(8);
    inputLayout->addWidget(m_resultsList);
    inputLayout->addSpacing(12);
    inputLayout->addWidget(makeLabel(QStringLiteral("\u7AEF\u53E3\uFF08IP \u76F4\u8FDE\u65F6\u4F7F\u7528\uFF09")));
    inputLayout->addSpacing(6);
    inputLayout->addWidget(m_portEdit);
    inputSectionLayout->addWidget(inputArea);

    auto* actionSection = LeyoDialog::createSectionFrame(this, QStringLiteral("connectIpDialogActionSection"));
    auto* actionSectionLayout = new QVBoxLayout(actionSection);
    actionSectionLayout->setContentsMargins(0, 0, 0, 0);
    actionSectionLayout->setSpacing(0);
    auto* actionArea = new QWidget(actionSection);
    actionArea->setObjectName(QStringLiteral("connectIpButtonArea"));
    auto* actionLayout = new QVBoxLayout(actionArea);
    actionLayout->setContentsMargins(40, 12, 40, 16);
    actionLayout->setSpacing(0);
    actionLayout->addLayout(btnRow);
    actionSectionLayout->addWidget(actionArea);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(16);
    outer->addWidget(inputSection);
    outer->addWidget(actionSection);

    connect(m_searchBtn, &QAbstractButton::clicked, this, &ConnectIpDialog::performSearch);
    connect(m_hostEdit, &QLineEdit::returnPressed, this, &ConnectIpDialog::performSearch);
    connect(m_okButton, &QAbstractButton::clicked, this, &QDialog::accept);
    connect(cancelBtn,  &QAbstractButton::clicked, this, &QDialog::reject);
}

void ConnectIpDialog::setKnownPeers(const QVector<SearchResult>& peers)
{
    m_knownPeers = peers;
}

void ConnectIpDialog::performSearch()
{
    const QString query = m_hostEdit->text().trimmed();
    if (query.isEmpty()) return;

    // 先检查是否是 IP 地址 → 不做搜索，直接由 accept() 处理
    QHostAddress addr;
    if (addr.setAddress(query)) {
        m_searchResults.clear();
        m_resultsList->clear();
        m_resultsList->hide();
        adjustSize();
        return;
    }

    // 搜索已知联系人
    m_searchResults.clear();
    m_resultsList->clear();
    const QString lowerQuery = query.toLower();
    for (const auto& peer : m_knownPeers) {
        if (peer.displayName.toLower().contains(lowerQuery)
            || peer.clientId.toLower().contains(lowerQuery)
            || peer.host.contains(lowerQuery)) {
            m_searchResults.append(peer);
        }
    }

    if (m_searchResults.isEmpty()) {
        auto* item = new QListWidgetItem(QStringLiteral("\u672A\u627E\u5230\u5339\u914D\u7684\u8054\u7CFB\u4EBA"));
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor(AppStyle::textMuted()));
        m_resultsList->addItem(item);
    } else {
        for (int i = 0; i < m_searchResults.size(); ++i) {
            const auto& r = m_searchResults[i];
            auto* item = new QListWidgetItem(m_resultsList);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            const QString statusDot = r.isOnline
                ? QStringLiteral("\u2022 \u5728\u7EBF") : QStringLiteral("\u25CB \u79BB\u7EBF");
            item->setText(QStringLiteral("%1  (%2:%3)  %4")
                              .arg(r.displayName.isEmpty() ? r.clientId : r.displayName,
                                   r.host,
                                   QString::number(r.port),
                                   statusDot));
            item->setData(Qt::UserRole, i);
        }
    }
    m_resultsList->show();
    adjustSize();
}

void ConnectIpDialog::updateAddButton()
{
    // No-op for now; button is always enabled
}

QString ConnectIpDialog::host() const
{
    return m_hostEdit->text().trimmed();
}

quint16 ConnectIpDialog::port() const
{
    bool ok = false;
    const quint16 p = m_portEdit->text().trimmed().toUShort(&ok);
    return ok ? p : 45454;
}

bool ConnectIpDialog::hasSelectedResults() const
{
    for (int i = 0; i < m_resultsList->count(); ++i) {
        auto* item = m_resultsList->item(i);
        if (item && item->checkState() == Qt::Checked) {
            return true;
        }
    }
    return false;
}

QVector<ConnectIpDialog::SearchResult> ConnectIpDialog::selectedResults() const
{
    QVector<SearchResult> selected;
    for (int i = 0; i < m_resultsList->count(); ++i) {
        auto* item = m_resultsList->item(i);
        if (!item || item->checkState() != Qt::Checked) continue;
        const int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_searchResults.size()) {
            selected.append(m_searchResults[idx]);
        }
    }
    return selected;
}
