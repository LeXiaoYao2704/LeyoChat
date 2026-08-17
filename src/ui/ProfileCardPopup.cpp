#include "ui/ProfileCardPopup.h"

#include "ui/AppStyle.h"

#include <ElaFrame.h>
#include <ElaPushButton.h>
#include <ElaText.h>

#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScreen>
#include <QVBoxLayout>

ProfileCardPopup::ProfileCardPopup(QWidget* parent)
    : QFrame(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setFixedWidth(280);
    setAttribute(Qt::WA_StyledBackground, true);

    // 延迟隐藏定时器
    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(300);
    connect(&m_hideTimer, &QTimer::timeout, this, &QWidget::hide);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 16);
    mainLayout->setSpacing(0);

    // 头像
    m_avatarLabel = new ElaText;
    m_avatarLabel->setFixedSize(56, 56);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignCenter);
    mainLayout->addSpacing(10);

    // 姓名
    m_nameLabel = new ElaText;
    m_nameLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_nameLabel);
    mainLayout->addSpacing(4);

    // 在线状态
    m_statusLabel = new ElaText;
    m_statusLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addSpacing(4);

    // 个性签名
    m_signatureLabel = new ElaText;
    m_signatureLabel->setAlignment(Qt::AlignCenter);
    m_signatureLabel->setWordWrap(true);
    mainLayout->addWidget(m_signatureLabel);
    mainLayout->addSpacing(12);

    // 分隔线
    auto* separator = new ElaFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setObjectName(QStringLiteral("profileCardSeparator"));
    separator->setFixedHeight(1);
    mainLayout->addWidget(separator);
    mainLayout->addSpacing(10);

    // 信息区域 — 使用行列布局
    auto makeInfoRow = [&](const QString& iconText, ElaText*& valueLabel) {
        auto* rowWidget = new QWidget;
        auto* row = new QHBoxLayout(rowWidget);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);
        auto* icon = new ElaText(iconText);
        icon->setFixedWidth(20);
        icon->setAlignment(Qt::AlignCenter);
        icon->setObjectName(QStringLiteral("profileCardInfoIcon"));
        row->addWidget(icon);
        valueLabel = new ElaText;
        valueLabel->setObjectName(QStringLiteral("profileCardInfoValue"));
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(valueLabel, 1);
        return rowWidget;
    };

    mainLayout->addWidget(makeInfoRow(QStringLiteral("\U0001F464"), m_idLabel));
    mainLayout->addSpacing(6);
    mainLayout->addWidget(makeInfoRow(QStringLiteral("\U0001F310"), m_hostLabel));
    mainLayout->addSpacing(6);
    mainLayout->addWidget(makeInfoRow(QStringLiteral("\U0001F50C"), m_portLabel));
    mainLayout->addSpacing(6);
    mainLayout->addWidget(makeInfoRow(QStringLiteral("\U0001F3E2"), m_departmentLabel));
    mainLayout->addSpacing(6);
    mainLayout->addWidget(makeInfoRow(QStringLiteral("\U0001F4BC"), m_jobTitleLabel));
    mainLayout->addSpacing(6);
    mainLayout->addWidget(makeInfoRow(QStringLiteral("\U0001F4DE"), m_phoneLabel));
    mainLayout->addSpacing(6);
    mainLayout->addWidget(makeInfoRow(QStringLiteral("\u2642\uFE0F"), m_genderLabel));
    mainLayout->addSpacing(6);
    mainLayout->addWidget(makeInfoRow(QStringLiteral("\u2709\uFE0F"), m_emailLabel));
    mainLayout->addSpacing(14);

    // "发送消息" 按钮
    m_sendMessageBtn = new ElaPushButton(QStringLiteral("\U0001F4AC \u53D1\u9001\u6D88\u606F"));
    m_sendMessageBtn->setCursor(Qt::PointingHandCursor);
    m_sendMessageBtn->setFixedHeight(36);
    mainLayout->addWidget(m_sendMessageBtn);

    connect(m_sendMessageBtn, &QAbstractButton::clicked, this, [this]() {
        if (!m_currentClientId.isEmpty()) {
            emit sendMessageRequested(m_currentClientId);
        }
        hide();
    });

    applyThemeStyle();
}

void ProfileCardPopup::applyThemeStyle()
{
    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    setStyleSheet(QStringLiteral(
        "ProfileCardPopup {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:12px;"
        "}"
        "ProfileCardPopup QWidget { background:transparent; }"
        "QLabel#profileCardInfoIcon {"
        "  background:transparent;"
        "  border:none;"
        "  color:%3;"
        "  font-size:13px;"
        "}"
        "QLabel#profileCardInfoValue {"
        "  background:transparent;"
        "  border:none;"
        "  color:%4;"
        "  font-size:12px;"
        "}"
        "QFrame#profileCardSeparator {"
        "  background:%2;"
        "  border:none;"
        "}").arg(AppStyle::surface(mode),
                 AppStyle::border(mode),
                 AppStyle::textMuted(mode),
                 AppStyle::textSecondary(mode)));

    if (m_nameLabel) {
        m_nameLabel->setStyleSheet(QStringLiteral(
            "font-size:16px; font-weight:700; color:%1; background:transparent; border:none;")
            .arg(AppStyle::textPrimary(mode)));
    }
    if (m_signatureLabel) {
        m_signatureLabel->setStyleSheet(QStringLiteral(
            "font-size:12px; color:%1; font-style:italic; background:transparent; border:none;")
            .arg(AppStyle::textMuted(mode)));
    }
    if (m_sendMessageBtn) {
        m_sendMessageBtn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background:%1;"
            "  color:%2;"
            "  border:1px solid %3;"
            "  border-radius:6px;"
            "  font-size:13px;"
            "  font-weight:600;"
            "}"
            "QPushButton:hover { background:%4; }")
            .arg(AppStyle::surfaceAlt(mode),
                 AppStyle::textPrimary(mode),
                 AppStyle::border(mode),
                 AppStyle::hoverBg(mode)));
    }
}

void ProfileCardPopup::showProfile(const ProfileInfo& info, const QPoint& globalPos)
{
    m_currentClientId = info.clientId;
    applyThemeStyle();
    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();

    // 头像字母
    const QString letter = info.displayName.trimmed().isEmpty()
        ? QStringLiteral("?")
        : QString(info.displayName.trimmed().front()).toUpper();
    m_avatarLabel->setText(letter);

    // 头像颜色 — 与气泡头像一致
    static const QColor kPalette[] = {
        QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
        QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
        QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
    };
    int h = 0;
    for (const QChar ch : info.displayName) {
        h = (h * 31 + ch.unicode()) & 0x7FFF'FFFF;
    }
    const QColor bg = kPalette[h % 6];
    m_avatarLabel->setStyleSheet(
        QStringLiteral("background: %1; color: white; border-radius: 28px;"
                       " font-size: 22px; font-weight: 700;").arg(bg.name()));

    m_nameLabel->setText(info.displayName.isEmpty() ? info.clientId : info.displayName);

    if (info.isOnline) {
        m_statusLabel->setText(QStringLiteral("\u2022 \u5728\u7EBF"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: #2FA484; font-weight: 600;"));
    } else {
        m_statusLabel->setText(QStringLiteral("\u25CB \u79BB\u7EBF"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "font-size: 12px; color: %1;").arg(AppStyle::textMuted(mode)));
    }

    m_idLabel->setText(QStringLiteral("ID\uFF1A%1").arg(info.clientId));

    if (!info.host.isEmpty()) {
        m_hostLabel->setText(QStringLiteral("IP\uFF1A%1").arg(info.host));
        m_hostLabel->show();
    } else {
        m_hostLabel->hide();
    }

    if (info.port > 0) {
        m_portLabel->setText(QStringLiteral("\u7AEF\u53E3\uFF1A%1").arg(info.port));
        m_portLabel->show();
    } else {
        m_portLabel->hide();
    }

    if (!info.signature.isEmpty()) {
        m_signatureLabel->setText(info.signature);
        m_signatureLabel->show();
    } else {
        m_signatureLabel->hide();
    }

    auto showOrHide = [](QLabel* label, const QString& prefix, const QString& value) {
        QWidget* rowWidget = label->parentWidget();
        if (!value.isEmpty()) {
            label->setText(prefix + value);
            if (rowWidget) rowWidget->show();
        } else {
            label->setText(QString());
            if (rowWidget) rowWidget->hide();
        }
    };
    showOrHide(m_departmentLabel, QStringLiteral("\u90E8\u95E8\uFF1A"), info.department);
    showOrHide(m_jobTitleLabel,   QStringLiteral("\u804C\u4F4D\uFF1A"), info.jobTitle);
    showOrHide(m_phoneLabel,      QStringLiteral("\u7535\u8BDD\uFF1A"), info.phoneNumber);
    showOrHide(m_genderLabel,     QStringLiteral("\u6027\u522B\uFF1A"), info.gender);
    showOrHide(m_emailLabel,      QStringLiteral("\u90AE\u7BB1\uFF1A"), info.email);

    adjustSize();

    // 定位：弹出在点击位置右下方，避免超出屏幕
    QPoint pos = globalPos;
    const QRect screenRect = screen() ? screen()->availableGeometry()
                                      : QRect(0, 0, 1920, 1080);
    if (pos.x() + width() > screenRect.right()) {
        pos.setX(pos.x() - width());
    }
    if (pos.y() + height() > screenRect.bottom()) {
        pos.setY(pos.y() - height());
    }
    move(pos);
    show();
    raise();
    // 安装全局事件过滤器，点击弹窗外部时关闭
    qApp->installEventFilter(this);
}

void ProfileCardPopup::scheduleHide()
{
    if (!m_hideTimer.isActive())
        m_hideTimer.start();
}

void ProfileCardPopup::cancelHide()
{
    m_hideTimer.stop();
}

void ProfileCardPopup::enterEvent(QEnterEvent* /*event*/)
{
    // 鼠标进入弹窗，取消延迟隐藏
    cancelHide();
}

void ProfileCardPopup::leaveEvent(QEvent* /*event*/)
{
    // 鼠标离开弹窗，开始延迟隐藏
    scheduleHide();
}

bool ProfileCardPopup::eventFilter(QObject* watched, QEvent* event)
{
    // 全局事件过滤：点击弹窗外部任何位置时关闭
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (!geometry().contains(me->globalPosition().toPoint())) {
            hide();
        }
    }
    return QFrame::eventFilter(watched, event);
}

bool ProfileCardPopup::event(QEvent* e)
{
    // 窗口隐藏时移除全局事件过滤器
    if (e->type() == QEvent::Hide) {
        qApp->removeEventFilter(this);
        m_hideTimer.stop();
    }
    // 用户交互后如果窗口失去焦点（例如点击了其他窗口），自动关闭
    if (e->type() == QEvent::WindowDeactivate) {
        hide();
        return true;
    }
    return QFrame::event(e);
}
