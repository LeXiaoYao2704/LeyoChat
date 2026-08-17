#include "ui/ForwardDetailDialog.h"
#include "ui/AppStyle.h"

#include <QApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QVBoxLayout>

using namespace AppStyle;

ForwardDetailDialog::ForwardDetailDialog(QWidget* parent)
    : QDialog(parent, Qt::FramelessWindowHint)
{
    setWindowTitle(QStringLiteral("\u804a\u5929\u8bb0\u5f55"));
    setFixedSize(420, 520);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

    // 整体样式
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; border-radius: 10px; }"
        "QPushButton#closeBtn { background: transparent; border: none; font-size: 18px; color: %2; }"
        "QPushButton#closeBtn:hover { color: %3; }")
        .arg(surface(), textSecondary(), textPrimary()));

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 标题栏（标题 + 关闭按钮）
    auto* headerWidget = new QWidget(this);
    headerWidget->setFixedHeight(44);
    headerWidget->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-top-left-radius: 10px; border-top-right-radius: 10px; "
        "border-bottom: 1px solid %2; }")
        .arg(surfaceAlt(), border()));
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(16, 0, 8, 0);

    auto* titleLabel = new QLabel(QStringLiteral("\U0001F4CB \u804a\u5929\u8bb0\u5f55"), headerWidget);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 15px; font-weight: bold; color: %1; background: transparent; border: none; }")
        .arg(textPrimary()));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    auto* closeBtn = new QPushButton(QStringLiteral("\u2715"), headerWidget);
    closeBtn->setObjectName(QStringLiteral("closeBtn"));
    closeBtn->setFixedSize(32, 32);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    headerLayout->addWidget(closeBtn);

    m_mainLayout->addWidget(headerWidget);

    // 滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: %1; border: none; }"
        "QScrollBar:vertical { width: 6px; background: transparent; }"
        "QScrollBar::handle:vertical { background: %2; border-radius: 3px; min-height: 30px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }")
        .arg(surface(), border()));

    m_contentWidget = new QWidget;
    m_contentWidget->setStyleSheet(QStringLiteral("QWidget { background: %1; }").arg(surface()));
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(16, 12, 16, 12);
    m_contentLayout->setSpacing(12);

    m_scrollArea->setWidget(m_contentWidget);
    m_mainLayout->addWidget(m_scrollArea);
}

void ForwardDetailDialog::setPackage(const QJsonObject& package)
{
    const QJsonArray messages = package.value(QStringLiteral("messages")).toArray();
    buildContent(messages);
}

void ForwardDetailDialog::buildContent(const QJsonArray& messages)
{
    // 清除旧内容
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    static const QColor kAvatarPalette[] = {
        QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
        QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
        QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
    };

    for (int i = 0; i < messages.size(); ++i) {
        const QJsonObject msg = messages[i].toObject();
        const QString senderName = msg.value(QStringLiteral("sender")).toString();
        const QString msgType = msg.value(QStringLiteral("type")).toString();
        const QString body = msg.value(QStringLiteral("text")).toString();
        const bool isFile = (msgType == QStringLiteral("file"));
        const bool isImage = (msgType == QStringLiteral("image"));
        const QString attachName = msg.value(QStringLiteral("fileName")).toString();
        const QString imageBase64 = msg.value(QStringLiteral("imageBase64")).toString();
        const qint64 ts = msg.value(QStringLiteral("ts")).toInteger();

        // ── 消息行：[头像] [名称+时间 / 正文] ──
        auto* row = new QWidget(m_contentWidget);
        row->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
        auto* rowHLayout = new QHBoxLayout(row);
        rowHLayout->setContentsMargins(0, 0, 0, 0);
        rowHLayout->setSpacing(10);
        rowHLayout->setAlignment(Qt::AlignTop);

        // 头像（彩色圆形 + 首字符）
        constexpr int avatarSize = 32;
        auto* avatarLabel = new QLabel(row);
        avatarLabel->setFixedSize(avatarSize, avatarSize);
        {
            QPixmap avatarPm(avatarSize, avatarSize);
            avatarPm.fill(Qt::transparent);
            QPainter ap(&avatarPm);
            ap.setRenderHint(QPainter::Antialiasing);
            int colorHash = 0;
            for (const QChar ch : senderName)
                colorHash = (colorHash * 31 + ch.unicode()) & 0x7FFF'FFFF;
            ap.setBrush(kAvatarPalette[colorHash % 6]);
            ap.setPen(Qt::NoPen);
            ap.drawEllipse(0, 0, avatarSize, avatarSize);
            ap.setPen(Qt::white);
            QFont avatarFont;
            avatarFont.setPixelSize(14);
            avatarFont.setBold(true);
            ap.setFont(avatarFont);
            const QString initial = senderName.isEmpty() ? QStringLiteral("?") : senderName.left(1);
            ap.drawText(QRect(0, 0, avatarSize, avatarSize), Qt::AlignCenter, initial);
            ap.end();
            avatarLabel->setPixmap(avatarPm);
        }
        avatarLabel->setStyleSheet(QStringLiteral("QLabel { background: transparent; }"));
        rowHLayout->addWidget(avatarLabel);

        // 右侧内容区
        auto* contentWidget = new QWidget(row);
        contentWidget->setStyleSheet(QStringLiteral("QWidget { background: transparent; }"));
        auto* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(2);

        // 名称 + 时间
        auto* headerLabel = new QLabel(contentWidget);
        const QString timeStr = ts > 0
            ? QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("MM-dd HH:mm"))
            : QString();
        headerLabel->setText(QStringLiteral(
            "<span style='font-weight:bold; color:%1'>%2</span>"
            "  <span style='color:%3; font-size:11px'>%4</span>")
            .arg(textPrimary(), senderName.toHtmlEscaped(), textMuted(), timeStr));
        headerLabel->setStyleSheet(QStringLiteral(
            "QLabel { font-size: 12px; background: transparent; }"));
        headerLabel->setTextFormat(Qt::RichText);
        contentLayout->addWidget(headerLabel);

        // 消息正文
        if (isImage && !imageBase64.isEmpty()) {
            const QByteArray imgData = QByteArray::fromBase64(imageBase64.toLatin1());
            QPixmap pm;
            pm.loadFromData(imgData);
            if (!pm.isNull()) {
                if (pm.width() > 220) {
                    pm = pm.scaledToWidth(220, Qt::SmoothTransformation);
                }
                auto* imgLabel = new QLabel(contentWidget);
                imgLabel->setPixmap(pm);
                imgLabel->setFixedSize(pm.size());
                imgLabel->setStyleSheet(QStringLiteral(
                    "QLabel { background: transparent; border-radius: 4px; }"));
                contentLayout->addWidget(imgLabel);
            }
        } else if (isImage) {
            auto* imgPlaceholder = new QLabel(contentWidget);
            imgPlaceholder->setText(QStringLiteral("\U0001F5BC [%1]").arg(
                attachName.isEmpty() ? QStringLiteral("\u56fe\u7247") : attachName.toHtmlEscaped()));
            imgPlaceholder->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 13px; background: transparent; "
                "padding: 4px 8px; border: 1px dashed %2; border-radius: 4px; }")
                .arg(textSecondary(), border()));
            contentLayout->addWidget(imgPlaceholder);
        } else if (isFile) {
            auto* fileLabel = new QLabel(contentWidget);
            fileLabel->setText(QStringLiteral(
                "<span style='color:%1'>\U0001F4CE %2</span>")
                .arg(textPrimary(), attachName.toHtmlEscaped()));
            fileLabel->setStyleSheet(QStringLiteral(
                "QLabel { font-size: 13px; background: %1; padding: 6px 10px; "
                "border-radius: 6px; border: 1px solid %2; }")
                .arg(surfaceAlt(), border()));
            fileLabel->setTextFormat(Qt::RichText);
            fileLabel->setWordWrap(true);
            contentLayout->addWidget(fileLabel);
        } else {
            auto* bodyLabel = new QLabel(contentWidget);
            bodyLabel->setTextFormat(Qt::PlainText);
            bodyLabel->setText(body);
            bodyLabel->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 13px; background: transparent; "
                "line-height: 1.4; }").arg(textPrimary()));
            bodyLabel->setWordWrap(true);
            bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
            contentLayout->addWidget(bodyLabel);
        }

        rowHLayout->addWidget(contentWidget, 1);
        m_contentLayout->addWidget(row);

        // 分割线（除了最后一条）
        if (i < messages.size() - 1) {
            auto* separator = new QWidget(m_contentWidget);
            separator->setFixedHeight(1);
            separator->setStyleSheet(QStringLiteral("QWidget { background: %1; }").arg(border()));
            m_contentLayout->addWidget(separator);
        }
    }
    m_contentLayout->addStretch();
}
