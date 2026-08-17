#include "SettingsPage.h"

#include "ClientAppearance.h"
#include "KnowledgeServiceSettingsWidget.h"
#include "StorageCleanupDialog.h"
#include "app/AppSettings.h"
#include "app/ApplicationInfo.h"
#include "integrations/RemoteChatServiceSettings.h"
#include "ui/AppStyle.h"
#include "ui/GlobalHotkeyManager.h"

#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <ElaFrame.h>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStringListModel>
#include <QSettings>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

#include "ElaComboBox.h"
#include "ElaCheckBox.h"
#include "ElaLineEdit.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaScrollPageArea.h"
#include "ElaSpinBox.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaListView.h"
#include "ElaScrollArea.h"
#include "ElaToggleSwitch.h"
#include "ElaWindow.h"

namespace {

ElaText* createFormLabel(const QString& text, QWidget* parent)
{
    auto* label = new ElaText(text, parent);
    label->setObjectName(QStringLiteral("FieldLabel"));
    return label;
}

void addFormRow(QFormLayout* layout, const QString& labelText, QWidget* field, QWidget* parent)
{
    layout->addRow(createFormLabel(labelText, parent), field);
}

QString remoteChatServiceAddressForDisplay(const QString& baseUrl)
{
    const QString normalized = normalizeRemoteChatServiceBaseUrl(baseUrl);
    const QUrl url(normalized);
    const QUrl defaultUrl(defaultRemoteChatServiceBaseUrl());
    if (url.isValid()
        && url.scheme() == QStringLiteral("http")
        && url.path().isEmpty()
        && url.port(defaultUrl.port()) == defaultUrl.port()) {
        return url.host();
    }
    return normalized;
}

QString colorWithAlpha(QColor color, int alpha)
{
    color.setAlpha(qBound(0, alpha, 255));
    return color.name(QColor::HexArgb);
}

QString settingsPageStyleSheet()
{
    const AppStyle::ThemeMode appMode = AppStyle::currentThemeMode();
    const ElaThemeType::ThemeMode themeMode = AppStyle::toElaThemeMode(appMode);
    const bool dark = themeMode == ElaThemeType::Dark;
    const QColor editableCard = dark ? QColor(24, 31, 39, 188) : QColor(255, 255, 255, 150);
    const QColor readonlyCard = dark ? QColor(20, 27, 35, 168) : QColor(255, 255, 255, 116);
    const QColor rowCard = dark ? QColor(28, 36, 46, 154) : QColor(255, 255, 255, 96);
    const QColor rowHover = dark ? QColor(35, 45, 57, 184) : QColor(255, 255, 255, 142);
    const QColor inputBg = dark ? QColor(18, 26, 34, 176) : QColor(255, 255, 255, 118);
    const QColor inputHoverBg = dark ? QColor(24, 34, 44, 196) : QColor(255, 255, 255, 166);
    const QColor popupBg = dark ? QColor(22, 29, 38, 236) : QColor(250, 253, 255, 236);
    const QColor buttonBg = dark ? QColor(28, 36, 46, 152) : QColor(255, 255, 255, 132);
    const QColor buttonHoverBg = dark ? QColor(35, 45, 57, 188) : QColor(255, 255, 255, 184);
    const QColor border = ElaThemeColor(themeMode, BasicBorder);
    const QColor borderHover = ElaThemeColor(themeMode, BasicBorderHover);
    const QColor text = ElaThemeColor(themeMode, BasicText);
    const QColor muted = ElaThemeColor(themeMode, BasicDetailsText);
    const QColor primary = ElaThemeColor(themeMode, PrimaryNormal);
    QColor heroStart = editableCard;
    QColor heroEnd = dark ? primary.darker(180) : primary.lighter(238);
    heroEnd.setAlpha(dark ? 154 : 130);
    QColor heroBorder = primary;
    heroBorder.setAlpha(dark ? 96 : 58);

    QString ss;
    ss += QStringLiteral(
        "QWidget#settingsPageRoot, QWidget#settingsContentInner { background:transparent; }"
        "ElaScrollArea, ElaScrollArea QWidget, ElaScrollArea > QWidget > QWidget { background:transparent; }"
        "QScrollArea, QScrollArea QWidget, QScrollArea > QWidget > QWidget { background:transparent; }");
    ss += QStringLiteral(
        "QFrame#HeroCard { background:qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 %1, stop:1 %2); border:1px solid %3; border-radius:24px; }")
             .arg(heroStart.name(QColor::HexArgb), heroEnd.name(QColor::HexArgb), heroBorder.name(QColor::HexArgb));
    ss += QStringLiteral("QFrame#EditableCard { background:%1; border:1px solid %2; border-radius:22px; }")
             .arg(editableCard.name(QColor::HexArgb), border.name(QColor::HexArgb));
    ss += QStringLiteral("QFrame#ReadonlyCard { background:%1; border:1px solid %2; border-radius:22px; }")
             .arg(readonlyCard.name(QColor::HexArgb), border.name(QColor::HexArgb));
    ss += QStringLiteral(
        "ElaScrollPageArea { background:transparent; }"
        "ElaScrollPageArea > QWidget { background:transparent; }");
    ss += QStringLiteral("QLabel#HeroTitleLabel { color:%1; font-size:30px; font-weight:800; }").arg(text.name(QColor::HexArgb));
    ss += QStringLiteral("QLabel#HeroSubtitleLabel { color:%1; font-size:13px; }").arg(muted.name(QColor::HexArgb));
    ss += QStringLiteral("QLabel#SectionTitleLabel { color:%1; font-size:20px; font-weight:700; }").arg(text.name(QColor::HexArgb));
    ss += QStringLiteral("QLabel#SectionSubtitleLabel, QLabel#SaveStatusLabel { color:%1; font-size:12px; }").arg(muted.name(QColor::HexArgb));
    ss += QStringLiteral("QLabel#FieldLabel { color:%1; font-size:13px; font-weight:600; min-width:88px; }").arg(text.name(QColor::HexArgb));
    ss += QStringLiteral("QPushButton#GhostButton { background:%1; color:%2; border:1px solid %3; border-radius:12px; padding:10px 16px; font-weight:700; }")
             .arg(buttonBg.name(QColor::HexArgb), text.name(QColor::HexArgb), border.name(QColor::HexArgb));
    ss += QStringLiteral("QPushButton#GhostButton:hover { border:1px solid %1; background:%2; }")
             .arg(borderHover.name(QColor::HexArgb), buttonHoverBg.name(QColor::HexArgb));
    ss += QStringLiteral("QLabel#SettingRowTitle { color:%1; font-size:14px; font-weight:700; }").arg(text.name(QColor::HexArgb));
    ss += QStringLiteral("QLabel#SettingRowDescription { color:%1; font-size:12px; }").arg(muted.name(QColor::HexArgb));

    ss += QStringLiteral(
        "QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QAbstractSpinBox, QKeySequenceEdit {"
        "  background:%1; color:%2; border:1px solid %3; border-radius:8px;"
        "  padding:6px 10px; font-size:13px;"
        "}"
        "QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover, QComboBox:hover, QAbstractSpinBox:hover, QKeySequenceEdit:hover {"
        "  background:%4; border:1px solid %5;"
        "}"
        "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QAbstractSpinBox:focus, QKeySequenceEdit:focus {"
        "  background:%4; border:1px solid %6;"
        "}"
        "QLineEdit:read-only { background:%7; color:%8; }"
        "QComboBox::drop-down { width:28px; border:none; background:transparent; }"
        "QComboBox QAbstractItemView { background:%9; color:%2; border:1px solid %3; outline:0; selection-background-color:%10; }")
             .arg(inputBg.name(QColor::HexArgb), text.name(QColor::HexArgb),
                  border.name(QColor::HexArgb), inputHoverBg.name(QColor::HexArgb),
                  borderHover.name(QColor::HexArgb), primary.name(QColor::HexArgb),
                  readonlyCard.name(QColor::HexArgb), muted.name(QColor::HexArgb),
                  popupBg.name(QColor::HexArgb), colorWithAlpha(primary, dark ? 72 : 46));
    ss += QStringLiteral(
        "QCheckBox, ElaCheckBox { color:%1; background:transparent; font-size:13px; }"
        "QCheckBox::indicator { background:%2; border:1px solid %3; border-radius:4px; width:16px; height:16px; }"
        "QCheckBox::indicator:hover { background:%4; border-color:%5; }"
        "QCheckBox::indicator:checked { background:%6; border-color:%6; }")
              .arg(text.name(QColor::HexArgb), inputBg.name(QColor::HexArgb),
                   border.name(QColor::HexArgb), inputHoverBg.name(QColor::HexArgb),
                   borderHover.name(QColor::HexArgb), primary.name(QColor::HexArgb));
    ss += QStringLiteral(
        "QFrame#EditableCard ElaScrollPageArea {"
        "  background:%1; border:1px solid %2; border-radius:16px;"
        "}"
        "QFrame#EditableCard ElaScrollPageArea:hover { background:%3; border-color:%4; }")
              .arg(rowCard.name(QColor::HexArgb), colorWithAlpha(border, dark ? 128 : 96),
                   rowHover.name(QColor::HexArgb), colorWithAlpha(borderHover, dark ? 190 : 140));

    return ss;
}

QWidget* createSystemSettingRow(const QString& title, const QString& description, QWidget* control, QWidget* parent)
{
    auto* area = new ElaScrollPageArea(parent);
    area->setBorderRadius(16);
    auto* layout = new QHBoxLayout(area);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(12);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    auto* titleLabel = new ElaText(title, area);
    titleLabel->setObjectName(QStringLiteral("SettingRowTitle"));
    textLayout->addWidget(titleLabel);
    auto* descLabel = new ElaText(description, area);
    descLabel->setObjectName(QStringLiteral("SettingRowDescription"));
    descLabel->setWordWrap(true);
    textLayout->addWidget(descLabel);

    layout->addLayout(textLayout, 1);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return area;
}

} // namespace

SettingsPage::SettingsPage(const Profile& profile,
                           const ClientPreferences& preferences,
                           const QString& dataRoot,
                           bool setupMode,
                           QWidget* parent)
    : ElaScrollPage(parent),
      dataRoot_(dataRoot),
      profile_(profile),
      preferences_(preferences),
      setupMode_(setupMode)
{
    setWindowTitle(setupMode_ ? QStringLiteral("\u9996\u6B21\u8BBE\u7F6E") : QStringLiteral("\u8BBE\u7F6E"));
    setTitleVisible(false);
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this](ElaThemeType::ThemeMode themeMode) {
        preferences_.themeMode = themeMode;
        if (themeModeCombo_) { QSignalBlocker b(themeModeCombo_); themeModeCombo_->setCurrentIndex(themeModeToIndex(themeMode)); }
        applyThemeStyles();
    });

    initialOutlookSettings_ = OutlookSettingsStore::load();

    auto* contentWidget = new QWidget(this);
    contentWidget->setObjectName(QStringLiteral("settingsPageRoot"));
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(18);

    // 5 组导航锚点对应的卡片会追加到 sectionAnchors_

    // === Hero Card ===
    auto* heroCard = new ElaFrame(contentWidget);
    heroCard->setObjectName(QStringLiteral("HeroCard"));
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(26, 24, 26, 24);
    heroLayout->setSpacing(18);

    auto* heroTopRow = new QHBoxLayout();
    heroTopRow->setContentsMargins(0, 0, 0, 0);
    heroTopRow->setSpacing(18);

    avatarPreviewLabel_ = new QLabel(heroCard);
    avatarPreviewLabel_->setFixedSize(96, 96);
    avatarPreviewLabel_->setAlignment(Qt::AlignCenter);
    heroTopRow->addWidget(avatarPreviewLabel_, 0, Qt::AlignTop);

    auto* heroTextLayout = new QVBoxLayout();
    heroTextLayout->setContentsMargins(0, 0, 0, 0);
    heroTextLayout->setSpacing(6);
    auto* heroTitle = new ElaText(QStringLiteral("\u6211\u7684\u8D44\u6599"), heroCard);
    heroTitle->setObjectName(QStringLiteral("HeroTitleLabel"));
    heroTextLayout->addWidget(heroTitle);
    accountSummaryLabel_ = new ElaText(heroCard);
    accountSummaryLabel_->setObjectName(QStringLiteral("HeroSubtitleLabel"));
    accountSummaryLabel_->setWordWrap(true);
    heroTextLayout->addWidget(accountSummaryLabel_);

    auto* avatarActions = new QHBoxLayout();
    avatarActions->setContentsMargins(0, 8, 0, 0);
    avatarActions->setSpacing(10);
    auto* chooseAvatarButton = new QPushButton(QStringLiteral("\u9009\u62E9\u5934\u50CF"), heroCard);
    chooseAvatarButton->setObjectName(QStringLiteral("GhostButton"));
    avatarActions->addWidget(chooseAvatarButton);
    auto* clearAvatarButton = new QPushButton(QStringLiteral("\u6062\u590D\u9ED8\u8BA4"), heroCard);
    clearAvatarButton->setObjectName(QStringLiteral("GhostButton"));
    avatarActions->addWidget(clearAvatarButton);
    avatarActions->addStretch(1);
    heroTextLayout->addLayout(avatarActions);
    heroTopRow->addLayout(heroTextLayout, 1);
    heroLayout->addLayout(heroTopRow);
    sectionAnchors_.append(heroCard); // [0] 我的资料
    layout->addWidget(heroCard);

    // === Profile Card ===
    auto* profileCard = new ElaFrame(contentWidget);
    profileCard->setObjectName(QStringLiteral("EditableCard"));
    auto* profileLayout = new QVBoxLayout(profileCard);
    profileLayout->setContentsMargins(24, 22, 24, 22);
    profileLayout->setSpacing(16);

    auto* profileTitle = new ElaText(QStringLiteral("\u7528\u6237\u4FE1\u606F\u5361\u7247"), profileCard);
    profileTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    profileLayout->addWidget(profileTitle);
    auto* profileDesc = new ElaText(QStringLiteral("\u7EF4\u62A4\u672C\u5730\u8EAB\u4EFD\u8D44\u6599\u3002\u4FDD\u5B58\u540E\u4F1A\u540C\u6B65\u5237\u65B0\u7528\u6237\u5361\u3001\u804A\u5929\u8EAB\u4EFD\u680F\u548C\u5E7F\u64AD\u6635\u79F0\u3002"), profileCard);
    profileDesc->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    profileDesc->setWordWrap(true);
    profileLayout->addWidget(profileDesc);

    auto* formWidget = new QWidget(profileCard);
    auto* formLayout = new QFormLayout(formWidget);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(12);
    formLayout->setHorizontalSpacing(14);

    displayNameEdit_ = new ElaLineEdit(formWidget);
    displayNameEdit_->setMinimumHeight(40);
    displayNameEdit_->setMaxLength(10);
    displayNameEdit_->setPlaceholderText(QStringLiteral("\u8F93\u5165\u4E00\u4E2A\u7528\u4E8E\u804A\u5929\u5C55\u793A\u7684\u6635\u79F0"));

    titleEdit_ = new ElaLineEdit(formWidget);
    titleEdit_->setMinimumHeight(40);
    titleEdit_->setPlaceholderText(QStringLiteral("\u4F8B\u5982\uFF1A\u8F6F\u4EF6\u5DE5\u7A0B\u5E08"));

    departmentEdit_ = new ElaLineEdit(formWidget);
    departmentEdit_->setMinimumHeight(40);
    departmentEdit_->setPlaceholderText(QStringLiteral("\u4F8B\u5982\uFF1A\u63A7\u5236\u8F6F\u4EF6\u4E0E\u516C\u5171\u8BBE\u8BA1\u90E8"));

    emailEdit_ = new ElaLineEdit(formWidget);
    emailEdit_->setMinimumHeight(40);
    emailEdit_->setPlaceholderText(QStringLiteral("\u7528\u4E8E\u8D44\u6599\u5C55\u793A\uFF0C\u53EF\u7559\u7A7A"));

    phoneEdit_ = new ElaLineEdit(formWidget);
    phoneEdit_->setMinimumHeight(40);
    phoneEdit_->setPlaceholderText(QStringLiteral("\u7528\u4E8E\u8D44\u6599\u5C55\u793A\uFF0C\u53EF\u7559\u7A7A"));

    signatureEdit_ = new ElaPlainTextEdit(formWidget);
    signatureEdit_->setPlaceholderText(QStringLiteral("\u5199\u4E00\u53E5\u4E2A\u6027\u7B7E\u540D"));
    signatureEdit_->setMinimumHeight(104);

    addFormRow(formLayout, QStringLiteral("\u663E\u793A\u540D\u79F0"), displayNameEdit_, formWidget);
    addFormRow(formLayout, QStringLiteral("\u5C97\u4F4D"), titleEdit_, formWidget);
    addFormRow(formLayout, QStringLiteral("\u90E8\u95E8"), departmentEdit_, formWidget);
    addFormRow(formLayout, QStringLiteral("\u90AE\u7BB1"), emailEdit_, formWidget);
    addFormRow(formLayout, QStringLiteral("\u7535\u8BDD"), phoneEdit_, formWidget);
    addFormRow(formLayout, QStringLiteral("\u4E2A\u6027\u7B7E\u540D"), signatureEdit_, formWidget);
    profileLayout->addWidget(formWidget);
    layout->addWidget(profileCard);

    // === System Card ===
    auto* systemCard = new ElaFrame(contentWidget);
    systemCard->setObjectName(QStringLiteral("EditableCard"));
    auto* systemLayout = new QVBoxLayout(systemCard);
    systemLayout->setContentsMargins(24, 22, 24, 22);
    systemLayout->setSpacing(12);

    auto* systemTitle = new ElaText(QStringLiteral("\u7CFB\u7EDF\u8BBE\u7F6E"), systemCard);
    systemTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    systemLayout->addWidget(systemTitle);
    auto* systemDesc = new ElaText(QStringLiteral("\u63A7\u5236\u4E3B\u9898\u3001\u5BFC\u822A\u5448\u73B0\u548C\u9875\u9762\u5207\u6362\u4F53\u9A8C\u3002"), systemCard);
    systemDesc->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    systemDesc->setWordWrap(true);
    systemLayout->addWidget(systemDesc);

    themeModeCombo_ = new ElaComboBox(systemCard);
    themeModeCombo_->addItem(QStringLiteral("\u65E5\u95F4\u6A21\u5F0F"));
    themeModeCombo_->addItem(QStringLiteral("\u591C\u95F4\u6A21\u5F0F"));
    themeModeCombo_->setMinimumWidth(170);

    navigationModeCombo_ = new ElaComboBox(systemCard);
    navigationModeCombo_->addItem(QStringLiteral("\u5C55\u5F00"));
    navigationModeCombo_->addItem(QStringLiteral("\u7D27\u51D1"));
    navigationModeCombo_->addItem(QStringLiteral("\u81EA\u52A8"));
    navigationModeCombo_->setMinimumWidth(170);

    stackSwitchModeCombo_ = new ElaComboBox(systemCard);
    stackSwitchModeCombo_->addItem(QStringLiteral("\u5F39\u51FA"));   // Popup
    stackSwitchModeCombo_->addItem(QStringLiteral("\u7F29\u653E"));   // Scale
    stackSwitchModeCombo_->addItem(QStringLiteral("\u7FFB\u8F6C"));   // Flip
    stackSwitchModeCombo_->addItem(QStringLiteral("\u6A21\u7CCA"));   // Blur
    stackSwitchModeCombo_->addItem(QStringLiteral("\u65E0"));       // None
    stackSwitchModeCombo_->setMinimumWidth(170);

    windowPaintModeCombo_ = new ElaComboBox(systemCard);
    windowPaintModeCombo_->addItem(QStringLiteral("\u6807\u51C6"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u84DD\u96FE"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u6668\u96FE\u7EFF"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u6696\u767D\u7EB8\u611F"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u6781\u7B80\u51B7\u7070"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u52A8\u6001"));
    windowPaintModeCombo_->setMinimumWidth(170);

    windowDisplayModeCombo_ = new ElaComboBox(systemCard);
    windowDisplayModeCombo_->addItem(QStringLiteral("\u6807\u51C6"));       // Normal
    windowDisplayModeCombo_->addItem(QStringLiteral("仿 Mica 效果"));  // ElaMica
#if defined(Q_OS_WIN)
    windowDisplayModeCombo_->addItem(QStringLiteral("Mica"));
    windowDisplayModeCombo_->addItem(QStringLiteral("Mica-Alt"));
    windowDisplayModeCombo_->addItem(QStringLiteral("\u4E9A\u514B\u529B"));   // Acrylic
    windowDisplayModeCombo_->addItem(QStringLiteral("\u6A21\u7CCA\u8FB9\u6846")); // Dwm-Blur
#endif
    windowDisplayModeCombo_->setMinimumWidth(170);

    userCardSwitch_ = new ElaToggleSwitch(systemCard);

    systemLayout->addWidget(createSystemSettingRow(QStringLiteral("\u4E3B\u9898\u5207\u6362"), QStringLiteral("\u660E\u6697\u6A21\u5F0F\u8BBE\u7F6E\u3002"), themeModeCombo_, systemCard));
    systemLayout->addWidget(createSystemSettingRow(QStringLiteral("\u5BFC\u822A\u680F\u6A21\u5F0F"), QStringLiteral("\u51B3\u5B9A\u5DE5\u4F5C\u53F0\u662F\u81EA\u52A8\u3001\u7D27\u51D1\u8FD8\u662F\u5C55\u5F00\u3002"), navigationModeCombo_, systemCard));
    systemLayout->addWidget(createSystemSettingRow(QStringLiteral("\u663E\u793A\u5DE6\u4FA7\u7528\u6237\u5361"), QStringLiteral("\u5173\u95ED\u540E\u5DE6\u4FA7\u5BFC\u822A\u4E0D\u663E\u793A\u5934\u50CF\u8D44\u6599\u5361\u3002"), userCardSwitch_, systemCard));
    systemLayout->addWidget(createSystemSettingRow(QStringLiteral("\u9875\u9762\u5207\u6362\u52A8\u753B"), QStringLiteral("\u63A7\u5236\u9875\u9762\u5207\u6362\u7684\u8FC7\u6E21\u65B9\u5F0F\u3002"), stackSwitchModeCombo_, systemCard));
    systemLayout->addWidget(createSystemSettingRow(QStringLiteral("\u7A97\u53E3\u80CC\u666F"), QStringLiteral("\u7A97\u53E3\u80CC\u666F\u6E32\u67D3\u65B9\u5F0F\u3002"), windowPaintModeCombo_, systemCard));
    systemLayout->addWidget(createSystemSettingRow(QStringLiteral("\u663E\u793A\u6548\u679C"), QStringLiteral("\u7A97\u53E3\u900F\u660E/Mica/Acrylic\u6548\u679C\u3002"), windowDisplayModeCombo_, systemCard));
    sectionAnchors_.append(systemCard); // [1] 系统设置
    layout->addWidget(systemCard);

    // === Notification & Screenshot Card ===
    auto* advancedCard = new ElaFrame(contentWidget);
    advancedCard->setObjectName(QStringLiteral("EditableCard"));
    auto* advancedLayout = new QVBoxLayout(advancedCard);
    advancedLayout->setContentsMargins(24, 22, 24, 22);
    advancedLayout->setSpacing(12);

    auto* advancedTitle = new ElaText(QStringLiteral("\u901A\u77E5\u4E0E\u622A\u56FE"), advancedCard);
    advancedTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    advancedLayout->addWidget(advancedTitle);
    auto* advancedDesc = new ElaText(QStringLiteral("\u6258\u76D8\u5F39\u7A97\u901A\u77E5\u548C\u622A\u56FE\u5FEB\u6377\u952E\u8BBE\u7F6E\u3002"), advancedCard);
    advancedDesc->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    advancedDesc->setWordWrap(true);
    advancedLayout->addWidget(advancedDesc);

    trayPopupSwitch_ = new ElaToggleSwitch(advancedCard);
    {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        trayPopupSwitch_->setIsToggled(cfg.value(QStringLiteral("notification/trayPopupEnabled"), false).toBool());
    }
    advancedLayout->addWidget(createSystemSettingRow(
        QStringLiteral("\u542F\u7528\u7CFB\u7EDF\u6258\u76D8\u5F39\u7A97\u901A\u77E5"),
        QStringLiteral("\u5F00\u542F\u540E\uFF0C\u6536\u5230\u65B0\u6D88\u606F\u65F6\u7CFB\u7EDF\u6258\u76D8\u4F1A\u5F39\u51FA\u63D0\u793A\uFF1B\u5173\u95ED\u540E\u4EC5\u4FDD\u7559\u4EFB\u52A1\u680F\u95EA\u70C1\u548C\u63D0\u793A\u97F3\u3002"),
        trayPopupSwitch_, advancedCard));

    hotkeyEdit_ = new QKeySequenceEdit(advancedCard);
    {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const QString savedHotkey = cfg.value(QStringLiteral("screenshotHotkey"),
                                              QStringLiteral("Ctrl+Alt+A")).toString();
        hotkeyEdit_->setKeySequence(QKeySequence(savedHotkey));
    }
    advancedLayout->addWidget(createSystemSettingRow(
        QStringLiteral("\u622A\u56FE\u5FEB\u6377\u952E"),
        QStringLiteral("\u5728\u4EFB\u610F\u754C\u9762\u6309\u6B64\u5FEB\u6377\u952E\u5373\u53EF\u622A\u56FE\uFF0C\u622A\u56FE\u7ED3\u679C\u4F1A\u8FDB\u5165\u804A\u5929\u8F93\u5165\u6846\u5E76\u590D\u5236\u5230\u526A\u8D34\u677F\u3002"),
        hotkeyEdit_, advancedCard));

    auto* hotkeyTestRow = new QHBoxLayout();
    hotkeyTestRow->setContentsMargins(18, 0, 18, 0);
    auto* hotkeyTestBtn = new QPushButton(QStringLiteral("\u68C0\u6D4B\u51B2\u7A81"), advancedCard);
    hotkeyTestBtn->setObjectName(QStringLiteral("GhostButton"));
    hotkeyTestResult_ = new ElaText(advancedCard);
    hotkeyTestResult_->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    hotkeyTestRow->addWidget(hotkeyTestBtn);
    hotkeyTestRow->addWidget(hotkeyTestResult_, 1);
    hotkeyTestRow->addStretch();
    advancedLayout->addLayout(hotkeyTestRow);

    connect(hotkeyTestBtn, &QPushButton::clicked, this, [this]() {
        const QKeySequence seq = hotkeyEdit_->keySequence();
        if (seq.isEmpty()) {
            hotkeyTestResult_->setText(QStringLiteral("\u8BF7\u5148\u8BBE\u7F6E\u5FEB\u6377\u952E"));
            return;
        }
        if (GlobalHotkeyManager::testHotkeyAvailable(seq)) {
            hotkeyTestResult_->setText(QStringLiteral("\u5FEB\u6377\u952E %1 \u53EF\u7528").arg(seq.toString(QKeySequence::NativeText)));
            hotkeyTestResult_->setStyleSheet(QStringLiteral("color:green; font-size:12px;"));
        } else {
            hotkeyTestResult_->setText(QStringLiteral("\u5FEB\u6377\u952E %1 \u5DF2\u88AB\u5176\u4ED6\u7A0B\u5E8F\u5360\u7528").arg(seq.toString(QKeySequence::NativeText)));
            hotkeyTestResult_->setStyleSheet(QStringLiteral("color:red; font-size:12px;"));
        }
    });

    sectionAnchors_.append(advancedCard); // [2] 高级设置
    layout->addWidget(advancedCard);

    // === Update Card ===
    auto* updateCard = new ElaFrame(contentWidget);
    updateCard->setObjectName(QStringLiteral("EditableCard"));
    auto* updateLayout = new QVBoxLayout(updateCard);
    updateLayout->setContentsMargins(24, 22, 24, 22);
    updateLayout->setSpacing(12);

    auto* updateTitle = new ElaText(QStringLiteral("\u8F6F\u4EF6\u66F4\u65B0"), updateCard);
    updateTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    updateLayout->addWidget(updateTitle);
    auto* updateDesc = new ElaText(QStringLiteral("\u914D\u7F6E\u5347\u7EA7\u670D\u52A1\u5668\u5730\u5740\u548C\u81EA\u52A8\u68C0\u67E5\u7B56\u7565\u3002"), updateCard);
    updateDesc->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    updateDesc->setWordWrap(true);
    updateLayout->addWidget(updateDesc);

    updateServerEdit_ = new ElaLineEdit(updateCard);
    updateServerEdit_->setMinimumHeight(40);
    updateServerEdit_->setMinimumWidth(280);
    updateServerEdit_->setPlaceholderText(QStringLiteral("\\\\server\\leyochat\\updates"));
    {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        updateServerEdit_->setText(cfg.value(QStringLiteral("update/serverPath"),
            QString()).toString());
    }
    updateLayout->addWidget(createSystemSettingRow(
        QStringLiteral("\u5347\u7EA7\u670D\u52A1\u5668\u5730\u5740"),
        QStringLiteral("\u7BA1\u7406\u5458\u5C06\u65B0\u7248\u5B89\u88C5\u5305\u653E\u5728\u6B64\u5171\u4EAB\u76EE\u5F55\uFF0C\u5BA2\u6237\u7AEF\u4F1A\u81EA\u52A8\u68C0\u67E5\u3002"),
        updateServerEdit_, updateCard));

    const RemoteChatServiceSettings remoteSettings =
        RemoteChatServiceSettingsStore::load();

    messageServerEdit_ = new ElaLineEdit(updateCard);
    messageServerEdit_->setMinimumHeight(40);
    messageServerEdit_->setMinimumWidth(280);
    messageServerEdit_->setPlaceholderText(QStringLiteral("https://chat.example.com"));
    messageServerEdit_->setText(
        remoteChatServiceAddressForDisplay(remoteSettings.baseUrl));
    updateLayout->addWidget(createSystemSettingRow(
        QStringLiteral("消息服务器地址"),
        QStringLiteral("留空时仅使用 P2P；填写自建服务地址后，客户端会自动补全协议和 8765 端口。"),
        messageServerEdit_, updateCard));

    messageServerTokenEdit_ = new ElaLineEdit(updateCard);
    messageServerTokenEdit_->setMinimumHeight(40);
    messageServerTokenEdit_->setMinimumWidth(280);
    messageServerTokenEdit_->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    messageServerTokenEdit_->setPlaceholderText(QStringLiteral("由服务管理员提供"));
    messageServerTokenEdit_->setText(remoteSettings.bearerToken);
    updateLayout->addWidget(createSystemSettingRow(
        QStringLiteral("消息服务凭证"),
        QStringLiteral("用于验证当前客户端，由自建服务的安装程序生成。"),
        messageServerTokenEdit_, updateCard));

    messageServerWorkspaceEdit_ = new ElaLineEdit(updateCard);
    messageServerWorkspaceEdit_->setMinimumHeight(40);
    messageServerWorkspaceEdit_->setMinimumWidth(280);
    messageServerWorkspaceEdit_->setPlaceholderText(QStringLiteral("default"));
    messageServerWorkspaceEdit_->setText(remoteSettings.workspaceId);
    updateLayout->addWidget(createSystemSettingRow(
        QStringLiteral("消息工作区"),
        QStringLiteral("客户端只能访问服务端授权的工作区。"),
        messageServerWorkspaceEdit_, updateCard));

    autoCheckUpdateBox_ = new ElaCheckBox(QStringLiteral("\u542F\u7528\u81EA\u52A8\u66F4\u65B0\u68C0\u6D4B"), updateCard);
    checkIntervalSpin_ = new ElaSpinBox(updateCard);
    checkIntervalSpin_->setRange(10, 1440);
    checkIntervalSpin_->setSuffix(QStringLiteral(" \u5206\u949F"));
    checkIntervalSpin_->setMinimumWidth(130);
    {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        autoCheckUpdateBox_->setChecked(cfg.value(QStringLiteral("update/autoCheckEnabled"), true).toBool());
        checkIntervalSpin_->setValue(cfg.value(QStringLiteral("update/checkIntervalMinutes"), 60).toInt());
        checkIntervalSpin_->setEnabled(autoCheckUpdateBox_->isChecked());
    }
    connect(autoCheckUpdateBox_, &ElaCheckBox::toggled, checkIntervalSpin_, &ElaSpinBox::setEnabled);

    auto* autoCheckRow = new QHBoxLayout();
    autoCheckRow->setContentsMargins(18, 0, 18, 0);
    autoCheckRow->setSpacing(12);
    autoCheckRow->addWidget(autoCheckUpdateBox_);
    autoCheckRow->addWidget(new ElaText(QStringLiteral("\u68C0\u6D4B\u95F4\u9694"), updateCard));
    autoCheckRow->addWidget(checkIntervalSpin_);
    autoCheckRow->addStretch();
    updateLayout->addLayout(autoCheckRow);

    layout->addWidget(updateCard);

    // === Data Management Card ===
    auto* dataCard = new ElaFrame(contentWidget);
    dataCard->setObjectName(QStringLiteral("EditableCard"));
    auto* dataLayout = new QVBoxLayout(dataCard);
    dataLayout->setContentsMargins(24, 22, 24, 22);
    dataLayout->setSpacing(12);

    auto* dataTitle = new ElaText(QStringLiteral("\u6570\u636E\u7BA1\u7406"), dataCard);
    dataTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    dataLayout->addWidget(dataTitle);
    auto* dataDesc = new ElaText(QStringLiteral("\u53EF\u5C06\u804A\u5929\u8BB0\u5F55\u5BFC\u51FA\u4E3A\u6570\u636E\u5E93\u6587\u4EF6\u8FDB\u884C\u5907\u4EFD\uFF0C\u6216\u4ECE\u5907\u4EFD\u6587\u4EF6\u6062\u590D\u3002"), dataCard);
    dataDesc->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    dataDesc->setWordWrap(true);
    dataLayout->addWidget(dataDesc);

    auto* dataButtonRow = new QHBoxLayout();
    dataButtonRow->setContentsMargins(0, 0, 0, 0);
    dataButtonRow->setSpacing(12);
    auto* exportBtn = new QPushButton(QStringLiteral("\u5BFC\u51FA\u804A\u5929\u8BB0\u5F55"), dataCard);
    exportBtn->setObjectName(QStringLiteral("GhostButton"));
    auto* importBtn = new QPushButton(QStringLiteral("\u5BFC\u5165\u804A\u5929\u8BB0\u5F55"), dataCard);
    importBtn->setObjectName(QStringLiteral("GhostButton"));
    dataButtonRow->addWidget(exportBtn);
    dataButtonRow->addWidget(importBtn);
    dataButtonRow->addStretch();
    dataLayout->addLayout(dataButtonRow);

    connect(exportBtn, &QPushButton::clicked, this, &SettingsPage::dataExportRequested);
    connect(importBtn, &QPushButton::clicked, this, &SettingsPage::dataImportRequested);

    layout->addWidget(dataCard);

    // === Integration Card: Outlook ===
    auto* outlookCard = new ElaFrame(contentWidget);
    outlookCard->setObjectName(QStringLiteral("EditableCard"));
    auto* outlookLayout = new QVBoxLayout(outlookCard);
    outlookLayout->setContentsMargins(24, 22, 24, 22);
    outlookLayout->setSpacing(12);

    auto* outlookTitle = new ElaText(QStringLiteral("\u96C6\u6210\u670D\u52A1 \u00B7 Outlook"), outlookCard);
    outlookTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    outlookLayout->addWidget(outlookTitle);
    auto* outlookDesc = new ElaText(QStringLiteral("\u4F7F\u7528 EWS \u8FDE\u63A5\u5185\u7F51 Exchange Server\u3002"), outlookCard);
    outlookDesc->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    outlookDesc->setWordWrap(true);
    outlookLayout->addWidget(outlookDesc);

    outlookEnabledCheck_ = new ElaCheckBox(QStringLiteral("\u542F\u7528 Outlook \u96C6\u6210"), outlookCard);
    outlookEnabledCheck_->setChecked(initialOutlookSettings_.enabled);
    outlookLayout->addWidget(outlookEnabledCheck_);

    auto* outlookFormWidget = new QWidget(outlookCard);
    auto* outlookFormLayout = new QFormLayout(outlookFormWidget);
    outlookFormLayout->setContentsMargins(0, 0, 0, 0);
    outlookFormLayout->setSpacing(10);
    outlookFormLayout->setHorizontalSpacing(14);

    outlookServerUrlEdit_ = new ElaLineEdit(outlookFormWidget);
    outlookServerUrlEdit_->setText(initialOutlookSettings_.serverUrl);
    outlookServerUrlEdit_->setMinimumHeight(36);
    outlookServerUrlEdit_->setPlaceholderText(QStringLiteral("https://mail.company.com"));
    outlookFormLayout->addRow(createFormLabel(QStringLiteral("Exchange \u670D\u52A1\u5668"), outlookFormWidget), outlookServerUrlEdit_);

    outlookUsernameEdit_ = new ElaLineEdit(outlookFormWidget);
    outlookUsernameEdit_->setText(initialOutlookSettings_.username);
    outlookUsernameEdit_->setMinimumHeight(36);
    outlookUsernameEdit_->setPlaceholderText(QStringLiteral("\u57DF\u8D26\u53F7"));
    outlookFormLayout->addRow(createFormLabel(QStringLiteral("\u7528\u6237\u540D"), outlookFormWidget), outlookUsernameEdit_);

    outlookPasswordEdit_ = new ElaLineEdit(outlookFormWidget);
    outlookPasswordEdit_->setText(initialOutlookSettings_.password);
    outlookPasswordEdit_->setMinimumHeight(36);
    outlookPasswordEdit_->setEchoMode(QLineEdit::Password);
    outlookPasswordEdit_->setPlaceholderText(QStringLiteral("\u5BC6\u7801"));
    outlookFormLayout->addRow(createFormLabel(QStringLiteral("\u5BC6\u7801"), outlookFormWidget), outlookPasswordEdit_);

    outlookEmailEdit_ = new ElaLineEdit(outlookFormWidget);
    outlookEmailEdit_->setText(initialOutlookSettings_.accountEmail);
    outlookEmailEdit_->setMinimumHeight(36);
    outlookEmailEdit_->setReadOnly(true);
    outlookEmailEdit_->setPlaceholderText(QStringLiteral("\u6388\u6743\u6210\u529F\u540E\u81EA\u52A8\u56DE\u586B"));
    outlookFormLayout->addRow(createFormLabel(QStringLiteral("\u90AE\u7BB1\u8D26\u53F7"), outlookFormWidget), outlookEmailEdit_);

    outlookDisplayNameEdit_ = new ElaLineEdit(outlookFormWidget);
    outlookDisplayNameEdit_->setText(initialOutlookSettings_.displayName);
    outlookDisplayNameEdit_->setMinimumHeight(36);
    outlookDisplayNameEdit_->setReadOnly(true);
    outlookDisplayNameEdit_->setPlaceholderText(QStringLiteral("\u6388\u6743\u6210\u529F\u540E\u81EA\u52A8\u56DE\u586B"));
    outlookFormLayout->addRow(createFormLabel(QStringLiteral("\u663E\u793A\u540D\u79F0"), outlookFormWidget), outlookDisplayNameEdit_);

    outlookLayout->addWidget(outlookFormWidget);

    auto* outlookTestBtn = new QPushButton(QStringLiteral("\u6D4B\u8BD5\u8FDE\u63A5"), outlookCard);
    outlookTestBtn->setObjectName(QStringLiteral("GhostButton"));
    outlookAuthStatusLabel_ = new ElaText(outlookCard);
    outlookAuthStatusLabel_->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    outlookAuthStatusLabel_->setWordWrap(true);
    auto* outlookTestRow = new QHBoxLayout();
    outlookTestRow->setContentsMargins(0, 0, 0, 0);
    outlookTestRow->setSpacing(12);
    outlookTestRow->addWidget(outlookTestBtn);
    outlookTestRow->addWidget(outlookAuthStatusLabel_, 1);
    outlookTestRow->addStretch();
    outlookLayout->addLayout(outlookTestRow);

    connect(outlookTestBtn, &QPushButton::clicked, this, &SettingsPage::outlookTestConnectionRequested);

    outlookNotifyEnabledCheck_ = new ElaCheckBox(QStringLiteral("\u542F\u7528 Outlook \u81EA\u52A8\u901A\u77E5"), outlookCard);
    outlookNotifyEnabledCheck_->setChecked(initialOutlookSettings_.notificationsEnabled);
    outlookLayout->addWidget(outlookNotifyEnabledCheck_);

    outlookPollIntervalSpin_ = new ElaSpinBox(outlookCard);
    outlookPollIntervalSpin_->setRange(1, 60);
    outlookPollIntervalSpin_->setSuffix(QStringLiteral(" \u5206\u949F"));
    outlookPollIntervalSpin_->setValue(qMax(1, initialOutlookSettings_.notificationPollIntervalMinutes));
    auto* outlookPollRow = new QHBoxLayout();
    outlookPollRow->setContentsMargins(18, 0, 18, 0);
    outlookPollRow->addWidget(new ElaText(QStringLiteral("\u8F6E\u8BE2\u95F4\u9694"), outlookCard));
    outlookPollRow->addWidget(outlookPollIntervalSpin_);
    outlookPollRow->addStretch();
    outlookLayout->addLayout(outlookPollRow);

    sectionAnchors_.append(outlookCard); // [3] 集成服务
    layout->addWidget(outlookCard);

    // === Knowledge Service Card ===
    auto* knowledgeCard = new ElaFrame(contentWidget);
    knowledgeCard->setObjectName(QStringLiteral("EditableCard"));
    auto* knowledgeLayout = new QVBoxLayout(knowledgeCard);
    knowledgeLayout->setContentsMargins(24, 22, 24, 22);
    knowledgeLayout->setSpacing(12);

    auto* knowledgeTitle = new ElaText(QStringLiteral("\u77E5\u8BC6\u670D\u52A1"), knowledgeCard);
    knowledgeTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    knowledgeLayout->addWidget(knowledgeTitle);
    auto* knowledgeDesc = new ElaText(QStringLiteral("\u7BA1\u7406\u77E5\u8BC6\u670D\u52A1\u8FDE\u63A5\u914D\u7F6E\u3002"), knowledgeCard);
    knowledgeDesc->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    knowledgeDesc->setWordWrap(true);
    knowledgeLayout->addWidget(knowledgeDesc);

    knowledgeServiceWidget_ = new KnowledgeServiceSettingsWidget(knowledgeCard);
    const QVector<KnowledgeServiceConfig> knowledgeConfigs = KnowledgeServiceSettingsStore::load();
    knowledgeServiceWidget_->setConfigs(knowledgeConfigs);
    knowledgeLayout->addWidget(knowledgeServiceWidget_);

    layout->addWidget(knowledgeCard);

    // === Storage Management Card ===
    auto* storageCard = new ElaFrame(contentWidget);
    storageCard->setObjectName(QStringLiteral("EditableCard"));
    auto* storageLayout = new QVBoxLayout(storageCard);
    storageLayout->setContentsMargins(24, 22, 24, 22);
    storageLayout->setSpacing(12);

    auto* storageTitle = new ElaText(QStringLiteral("\u5B58\u50A8\u7BA1\u7406"), storageCard);
    storageTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    storageLayout->addWidget(storageTitle);
    auto* storageDesc = new ElaText(QStringLiteral("\u7BA1\u7406\u6587\u4EF6\u63A5\u6536\u8DEF\u5F84\u548C\u6E05\u7406\u5386\u53F2\u6570\u636E\u3002"), storageCard);
    storageDesc->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    storageDesc->setWordWrap(true);
    storageLayout->addWidget(storageDesc);

    // --- 文件接收路径 ---
    incomingFilesPathEdit_ = new ElaLineEdit(storageCard);
    incomingFilesPathEdit_->setMinimumHeight(40);
    incomingFilesPathEdit_->setMinimumWidth(280);
    incomingFilesPathEdit_->setPlaceholderText(QStringLiteral("\u9ED8\u8BA4: \u4E0B\u8F7D/LeyoChat/Received"));
    {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        incomingFilesPathEdit_->setText(cfg.value(QStringLiteral("file/incomingFilesPath")).toString());
    }
    auto* browseBtn = new ElaPushButton(storageCard);
    browseBtn->setText(QStringLiteral("\u6D4F\u89C8..."));
    browseBtn->setBorderRadius(10);
    browseBtn->setFixedHeight(36);
    connect(browseBtn, &ElaPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("\u9009\u62E9\u6587\u4EF6\u63A5\u6536\u76EE\u5F55"),
            incomingFilesPathEdit_->text().isEmpty()
                ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                : incomingFilesPathEdit_->text());
        if (!dir.isEmpty()) {
            incomingFilesPathEdit_->setText(dir);
        }
    });
    auto* filePathRow = new QHBoxLayout();
    filePathRow->setSpacing(8);
    filePathRow->addWidget(incomingFilesPathEdit_, 1);
    filePathRow->addWidget(browseBtn);
    auto* filePathWidget = new QWidget(storageCard);
    filePathWidget->setLayout(filePathRow);
    storageLayout->addWidget(createSystemSettingRow(
        QStringLiteral("\u6587\u4EF6\u63A5\u6536\u8DEF\u5F84"),
        QStringLiteral("\u81EA\u5B9A\u4E49\u63A5\u6536\u6587\u4EF6/\u56FE\u7247\u7684\u4FDD\u5B58\u76EE\u5F55\u3002\u7559\u7A7A\u5219\u4F7F\u7528\u9ED8\u8BA4\u8DEF\u5F84\u3002"),
        filePathWidget, storageCard));

    // --- 数据清理（分类管理模式）---
    auto* cleanupSeparator = new ElaText(QStringLiteral("\u6570\u636E\u6E05\u7406"), storageCard);
    cleanupSeparator->setObjectName(QStringLiteral("SettingRowTitle"));
    storageLayout->addWidget(cleanupSeparator);
    auto* cleanupHint = new ElaText(QStringLiteral("\u70B9\u51FB\u201C\u7BA1\u7406\u201D\u67E5\u770B\u5404\u5206\u7C7B\u4E0B\u7684\u5177\u4F53\u5185\u5BB9\uFF0C\u52FE\u9009\u540E\u5220\u9664\u3002"), storageCard);
    cleanupHint->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    cleanupHint->setWordWrap(true);
    storageLayout->addWidget(cleanupHint);

    // 时间范围筛选
    cleanupAgeSpin_ = new ElaComboBox(storageCard);
    cleanupAgeSpin_->addItem(QStringLiteral("7 \u5929\u524D"));
    cleanupAgeSpin_->addItem(QStringLiteral("30 \u5929\u524D"));
    cleanupAgeSpin_->addItem(QStringLiteral("90 \u5929\u524D"));
    cleanupAgeSpin_->addItem(QStringLiteral("\u5168\u90E8"));
    cleanupAgeSpin_->setCurrentIndex(1);
    cleanupAgeSpin_->setMinimumWidth(130);

    auto* ageRow = new QHBoxLayout();
    ageRow->setContentsMargins(0, 0, 0, 0);
    ageRow->setSpacing(12);
    ageRow->addWidget(new ElaText(QStringLiteral("\u663E\u793A\u8303\u56F4:"), storageCard));
    ageRow->addWidget(cleanupAgeSpin_);
    ageRow->addStretch();
    storageLayout->addLayout(ageRow);

    // 日志文件行
    auto* manageLogsBtn = new ElaPushButton(storageCard);
    manageLogsBtn->setText(QStringLiteral("\u7BA1\u7406"));
    manageLogsBtn->setFixedHeight(32);
    manageLogsBtn->setBorderRadius(10);
    storageLayout->addWidget(createSystemSettingRow(
        QStringLiteral("\u65E5\u5FD7\u6587\u4EF6"),
        QStringLiteral("\u5E94\u7528\u8FD0\u884C\u65E5\u5FD7\uFF0C\u53EF\u5B89\u5168\u6E05\u7406\u65E7\u6570\u636E"),
        manageLogsBtn, storageCard));

    // 聊天消息行
    auto* manageMessagesBtn = new ElaPushButton(storageCard);
    manageMessagesBtn->setText(QStringLiteral("\u7BA1\u7406"));
    manageMessagesBtn->setFixedHeight(32);
    manageMessagesBtn->setBorderRadius(10);
    storageLayout->addWidget(createSystemSettingRow(
        QStringLiteral("\u804A\u5929\u6D88\u606F"),
        QStringLiteral("\u6D88\u606F\u5B58\u50A8\u4E8E\u6570\u636E\u5E93\uFF0C\u5EFA\u8BAE\u6E05\u7406\u524D\u5148\u5907\u4EFD"),
        manageMessagesBtn, storageCard));

    // 接收文件行
    auto* manageFilesBtn = new ElaPushButton(storageCard);
    manageFilesBtn->setText(QStringLiteral("\u7BA1\u7406"));
    manageFilesBtn->setFixedHeight(32);
    manageFilesBtn->setBorderRadius(10);
    storageLayout->addWidget(createSystemSettingRow(
        QStringLiteral("\u63A5\u6536\u7684\u6587\u4EF6"),
        QStringLiteral("\u4ED6\u4EBA\u53D1\u9001\u7ED9\u4F60\u7684\u6587\u4EF6\uFF0C\u5220\u9664\u540E\u4E0D\u53EF\u6062\u590D"),
        manageFilesBtn, storageCard));

    // 图片/截图行
    auto* manageImagesBtn = new ElaPushButton(storageCard);
    manageImagesBtn->setText(QStringLiteral("\u7BA1\u7406"));
    manageImagesBtn->setFixedHeight(32);
    manageImagesBtn->setBorderRadius(10);
    storageLayout->addWidget(createSystemSettingRow(
        QStringLiteral("\u56FE\u7247/\u622A\u56FE"),
        QStringLiteral("\u63A5\u6536\u7684\u56FE\u7247\u548C\u622A\u56FE\u6587\u4EF6"),
        manageImagesBtn, storageCard));

    // 状态标签
    cleanupStatusLabel_ = new ElaText(storageCard);
    cleanupStatusLabel_->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    storageLayout->addWidget(cleanupStatusLabel_);

    // 连接管理按钮信号
    connect(manageLogsBtn, &ElaPushButton::clicked, this, [this]() {
        emit storageCategoryManageRequested(StorageCategory::Logs, cleanupAgeSpin_->currentIndex());
    });
    connect(manageMessagesBtn, &ElaPushButton::clicked, this, [this]() {
        emit storageCategoryManageRequested(StorageCategory::Messages, cleanupAgeSpin_->currentIndex());
    });
    connect(manageFilesBtn, &ElaPushButton::clicked, this, [this]() {
        emit storageCategoryManageRequested(StorageCategory::Files, cleanupAgeSpin_->currentIndex());
    });
    connect(manageImagesBtn, &ElaPushButton::clicked, this, [this]() {
        emit storageCategoryManageRequested(StorageCategory::Images, cleanupAgeSpin_->currentIndex());
    });

    sectionAnchors_.append(storageCard); // [4] 存储管理
    layout->addWidget(storageCard);

    // === About Card ===
    auto* aboutCard = new ElaFrame(contentWidget);
    aboutCard->setObjectName(QStringLiteral("EditableCard"));
    auto* aboutLayout = new QVBoxLayout(aboutCard);
    aboutLayout->setContentsMargins(24, 22, 24, 22);
    aboutLayout->setSpacing(12);

    auto* aboutTitle = new ElaText(QStringLiteral("\u5173\u4E8E"), aboutCard);
    aboutTitle->setObjectName(QStringLiteral("SectionTitleLabel"));
    aboutLayout->addWidget(aboutTitle);

    versionLabel_ = new ElaText(
        QStringLiteral("%1 %2").arg(ApplicationInfo::productName(), ApplicationInfo::currentVersion()),
        aboutCard);
    versionLabel_->setObjectName(QStringLiteral("SettingRowTitle"));
    aboutLayout->addWidget(versionLabel_);

    auto* aboutButtonRow = new QHBoxLayout();
    aboutButtonRow->setContentsMargins(0, 0, 0, 0);
    aboutButtonRow->setSpacing(10);
    auto* aboutBtn = new QPushButton(QStringLiteral("\u5173\u4E8E LeyoChat"), aboutCard);
    aboutBtn->setObjectName(QStringLiteral("GhostButton"));
    auto* releaseNotesBtn = new QPushButton(QStringLiteral("\u672C\u7248\u66F4\u65B0"), aboutCard);
    releaseNotesBtn->setObjectName(QStringLiteral("GhostButton"));
    checkUpdateButton_ = new ElaPushButton(aboutCard);
    checkUpdateButton_->setText(QStringLiteral("\u68C0\u67E5\u66F4\u65B0"));
    checkUpdateButton_->setMinimumWidth(100);
    checkUpdateButton_->setFixedHeight(36);
    checkUpdateButton_->setBorderRadius(12);
    checkUpdateStatusLabel_ = new ElaText(aboutCard);
    checkUpdateStatusLabel_->setObjectName(QStringLiteral("SectionSubtitleLabel"));
    aboutButtonRow->addWidget(aboutBtn);
    aboutButtonRow->addWidget(releaseNotesBtn);
    aboutButtonRow->addWidget(checkUpdateButton_);
    aboutButtonRow->addWidget(checkUpdateStatusLabel_);
    aboutButtonRow->addStretch();
    aboutLayout->addLayout(aboutButtonRow);

    auto* archButtonRow = new QHBoxLayout();
    archButtonRow->setContentsMargins(0, 0, 0, 0);
    archButtonRow->setSpacing(10);
    auto* archBtn = new QPushButton(QStringLiteral("\u67E5\u770B\u6DF7\u5408\u67B6\u6784"), aboutCard);
    archBtn->setObjectName(QStringLiteral("GhostButton"));
    auto* diagBtn = new QPushButton(QStringLiteral("\u5BFC\u51FA\u8BCA\u65AD\u5305"), aboutCard);
    diagBtn->setObjectName(QStringLiteral("GhostButton"));
    archButtonRow->addWidget(archBtn);
    archButtonRow->addWidget(diagBtn);
    archButtonRow->addStretch();
    aboutLayout->addLayout(archButtonRow);

    connect(aboutBtn, &QPushButton::clicked, this, &SettingsPage::showAboutDialogRequested);
    connect(releaseNotesBtn, &QPushButton::clicked, this, &SettingsPage::showReleaseNotesRequested);
    connect(checkUpdateButton_, &ElaPushButton::clicked, this, &SettingsPage::checkUpdateRequested);
    connect(archBtn, &QPushButton::clicked, this, &SettingsPage::showRuntimeArchitectureRequested);
    connect(diagBtn, &QPushButton::clicked, this, &SettingsPage::exportDiagnosticsRequested);

    sectionAnchors_.append(aboutCard); // [5] 关于
    layout->addWidget(aboutCard);

    // === Action Row ===
    auto* actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(12);
    saveStatusLabel_ = new ElaText(contentWidget);
    saveStatusLabel_->setObjectName(QStringLiteral("SaveStatusLabel"));
    saveStatusLabel_->setWordWrap(true);
    actionRow->addWidget(saveStatusLabel_, 1);

    saveButton_ = new ElaPushButton(contentWidget);
    saveButton_->setMinimumWidth(160);
    saveButton_->setFixedHeight(42);
    saveButton_->setBorderRadius(12);
    actionRow->addWidget(saveButton_);
    layout->addLayout(actionRow);
    layout->addSpacing(10);

    // === 左侧导航 + 右侧内容 的分栏布局 ===
    auto* splitWidget = new QWidget(this);
    splitWidget->setObjectName(QStringLiteral("settingsPageRoot"));
    auto* splitLayout = new QHBoxLayout(splitWidget);
    splitLayout->setContentsMargins(0, 0, 0, 0);
    splitLayout->setSpacing(0);

    // 左侧导航列表（ElaListView 自带 Ela 主题渲染）
    navList_ = new ElaListView(splitWidget);
    navList_->setFixedWidth(170);
    navList_->setItemHeight(40);
    navList_->setIsTransparent(true);
    auto* navModel = new QStringListModel({
        QStringLiteral("\u6211\u7684\u8D44\u6599"),
        QStringLiteral("\u7CFB\u7EDF\u8BBE\u7F6E"),
        QStringLiteral("\u9AD8\u7EA7\u8BBE\u7F6E"),
        QStringLiteral("\u96C6\u6210\u670D\u52A1"),
        QStringLiteral("\u5B58\u50A8\u7BA1\u7406"),
        QStringLiteral("\u5173\u4E8E")
    }, navList_);
    navList_->setModel(navModel);
    navList_->setCurrentIndex(navModel->index(0, 0));

    // 右侧滚动区域（ElaScrollArea 自带平滑滚动与主题适配）
    contentScrollArea_ = new ElaScrollArea(splitWidget);
    contentScrollArea_->setWidgetResizable(true);
    contentScrollArea_->setFrameShape(QFrame::NoFrame);
    contentScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentWidget->setObjectName(QStringLiteral("settingsContentInner"));
    contentScrollArea_->setWidget(contentWidget);

    splitLayout->addWidget(navList_, 0);
    splitLayout->addWidget(contentScrollArea_, 1);

    // 点击导航项 → 滚动到对应 section
    connect(navList_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex& /*previous*/) {
        int row = current.row();
        if (row >= 0 && row < sectionAnchors_.size()) {
            contentScrollArea_->ensureWidgetVisible(sectionAnchors_[row], 0, 10);
        }
    });

    addCentralWidget(splitWidget);

    // === Connections ===
    connect(chooseAvatarButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("\u9009\u62E9\u5934\u50CF"), QString(),
            QStringLiteral("\u5934\u50CF\u56FE\u7247 (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (!path.isEmpty()) {
            refreshProfilePreview();
            saveStatusLabel_->setText(QStringLiteral("\u5934\u50CF\u5DF2\u66F4\u65B0\u3002"));
        }
    });
    connect(clearAvatarButton, &QPushButton::clicked, this, [this]() {
        refreshProfilePreview();
        saveStatusLabel_->setText(QStringLiteral("\u5DF2\u5207\u6362\u4E3A\u9ED8\u8BA4\u5934\u50CF\u3002"));
    });

    auto previewLambda = [this](int) {
        if (updatingControls_) return;
        preferences_ = collectPreferences();
        previewSystemPreferences();
        persistSystemPreferences();
    };
    connect(themeModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, previewLambda);
    connect(navigationModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, previewLambda);
    connect(stackSwitchModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, previewLambda);
    connect(windowPaintModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, previewLambda);
    connect(windowDisplayModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, previewLambda);
    connect(userCardSwitch_, &ElaToggleSwitch::toggled, this, [this](bool) {
        if (updatingControls_) return;
        preferences_ = collectPreferences();
        previewSystemPreferences();
        persistSystemPreferences();
    });

    connect(saveButton_, &ElaPushButton::clicked, this, [this]() {
        Profile nextProfile = collectProfile();
        if (QString::fromStdWString(nextProfile.displayName).trimmed().isEmpty()) {
            saveStatusLabel_->setText(QStringLiteral("\u663E\u793A\u540D\u79F0\u4E0D\u80FD\u4E3A\u7A7A\u3002"));
            return;
        }
        if (knowledgeServiceWidget_) {
            QString knowledgeError;
            if (!knowledgeServiceWidget_->validate(&knowledgeError)) {
                saveStatusLabel_->setText(knowledgeError.trimmed().isEmpty()
                    ? QStringLiteral("\u77E5\u8BC6\u670D\u52A1\u914D\u7F6E\u65E0\u6548\u3002")
                    : knowledgeError);
                return;
            }
        }

        ClientPreferences nextPreferences = collectPreferences();
        nextPreferences.initialSetupCompleted = true;

        QString errorMessage;
        if (!ClientPreferencesStore::save(dataRoot_, nextPreferences, &errorMessage)) {
            saveStatusLabel_->setText(QStringLiteral("\u7CFB\u7EDF\u8BBE\u7F6E\u4FDD\u5B58\u5931\u8D25: %1").arg(errorMessage));
            return;
        }

        {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            cfg.setValue(QStringLiteral("appearance/themeMode"),
                         AppStyle::themeModeToString(AppStyle::themeModeFromEla(nextPreferences.themeMode)));
            cfg.setValue(QStringLiteral("appearance/navigationDisplayMode"),
                         static_cast<int>(nextPreferences.navigationDisplayMode));
            cfg.setValue(QStringLiteral("appearance/stackSwitchMode"),
                         static_cast<int>(nextPreferences.stackSwitchMode));
            cfg.setValue(QStringLiteral("appearance/windowPaintMode"),
                         static_cast<int>(nextPreferences.windowPaintMode));
            cfg.setValue(QStringLiteral("appearance/windowBackgroundStyle"),
                         static_cast<int>(nextPreferences.windowBackgroundStyle));
            cfg.setValue(QStringLiteral("appearance/windowDisplayMode"),
                         static_cast<int>(nextPreferences.windowDisplayMode));
            cfg.setValue(QStringLiteral("appearance/userInfoCardVisible"),
                         nextPreferences.userInfoCardVisible);
            cfg.setValue(QStringLiteral("notification/trayPopupEnabled"),
                         trayPopupSwitch_ ? trayPopupSwitch_->getIsToggled() : false);
            if (hotkeyEdit_) {
                cfg.setValue(QStringLiteral("screenshotHotkey"),
                             hotkeyEdit_->keySequence().toString(QKeySequence::PortableText));
            }
            if (updateServerEdit_) {
                cfg.setValue(QStringLiteral("update/serverPath"),
                             updateServerEdit_->text().trimmed());
            }
            if (messageServerEdit_
                && messageServerTokenEdit_
                && messageServerWorkspaceEdit_) {
                RemoteChatServiceSettings remoteSettings =
                    RemoteChatServiceSettingsStore::load(&cfg);
                const QString nextBaseUrl =
                    normalizeRemoteChatServiceBaseUrl(messageServerEdit_->text());
                const QString nextToken =
                    messageServerTokenEdit_->text().trimmed();
                const QString nextWorkspace =
                    messageServerWorkspaceEdit_->text().trimmed();
                const bool addressChanged =
                    normalizeRemoteChatServiceBaseUrl(remoteSettings.baseUrl)
                        != nextBaseUrl
                    || remoteSettings.bearerToken.trimmed() != nextToken
                    || remoteSettings.workspaceId.trimmed() != nextWorkspace;
                remoteSettings.enabled = !nextBaseUrl.isEmpty()
                    && !nextToken.isEmpty()
                    && !nextWorkspace.isEmpty();
                remoteSettings.mode = remoteSettings.enabled
                    ? RemoteChatTransportMode::ServerPreferred
                    : RemoteChatTransportMode::P2POnly;
                remoteSettings.baseUrl = nextBaseUrl;
                remoteSettings.bearerToken = nextToken;
                remoteSettings.workspaceId = nextWorkspace;
                remoteSettings.allowP2PFallback = true;
                remoteSettings.allowAutomaticPeerConnections = false;
                if (addressChanged) {
                    remoteSettings.lastHealthCheckAtMs = 0;
                    remoteSettings.lastHealthSuccessAtMs = 0;
                    remoteSettings.lastErrorMessage.clear();
                }
                RemoteChatServiceSettingsStore::save(remoteSettings, &cfg);
            }
            if (incomingFilesPathEdit_) {
                const QString path = incomingFilesPathEdit_->text().trimmed();
                if (path.isEmpty()) {
                    cfg.remove(QStringLiteral("file/incomingFilesPath"));
                } else {
                    cfg.setValue(QStringLiteral("file/incomingFilesPath"), path);
                }
            }
            if (autoCheckUpdateBox_) {
                cfg.setValue(QStringLiteral("update/autoCheckEnabled"), autoCheckUpdateBox_->isChecked());
            }
            if (checkIntervalSpin_) {
                cfg.setValue(QStringLiteral("update/checkIntervalMinutes"), checkIntervalSpin_->value());
            }
            cfg.sync();
        }

        if (knowledgeServiceWidget_) {
            KnowledgeServiceSettingsStore::save(knowledgeServiceWidget_->configs());
        }

        {
            OutlookConnectionSettings outlookSettings = collectOutlookSettings();
            outlookSettings.consecutivePollFailures = 0;
            outlookSettings.lastPollErrorMessage.clear();
            outlookSettings.lastPollErrorCategory.clear();
            OutlookSettingsStore::save(outlookSettings);
        }

        const bool wasSetup = setupMode_;
        profile_ = nextProfile;
        preferences_ = nextPreferences;
        setSetupMode(false);
        refreshProfilePreview();
        refreshPreferenceControls();
        saveStatusLabel_->setText(wasSetup ? QStringLiteral("\u9996\u6B21\u8BBE\u7F6E\u5DF2\u5B8C\u6210\u3002") : QStringLiteral("\u8BBE\u7F6E\u5DF2\u4FDD\u5B58\u3002"));
        emit settingsSaved(profile_, preferences_, wasSetup);
    });

    setProfile(profile_);
    setClientPreferences(preferences_);
    setSetupMode(setupMode_);
    applyThemeStyles();
}

void SettingsPage::changeEvent(QEvent* event)
{
    ElaScrollPage::changeEvent(event);
    if (event && (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange))
        applyThemeStyles();
}

void SettingsPage::applyThemeStyles()
{
    if (applyingThemeStyles_) return;
    applyingThemeStyles_ = true;
    setStyleSheet(settingsPageStyleSheet());
    applyingThemeStyles_ = false;
}

const Profile& SettingsPage::profile() const { return profile_; }

void SettingsPage::setProfile(const Profile& profile)
{
    profile_ = profile;
    if (displayNameEdit_) displayNameEdit_->setText(QString::fromStdWString(profile_.displayName));
    if (titleEdit_) titleEdit_->setText(QString::fromStdWString(profile_.jobTitle));
    if (departmentEdit_) departmentEdit_->setText(QString::fromStdWString(profile_.department));
    if (emailEdit_) emailEdit_->setText(QString::fromStdWString(profile_.email));
    if (phoneEdit_) phoneEdit_->setText(QString::fromStdWString(profile_.phoneNumber));
    if (signatureEdit_) signatureEdit_->setPlainText(QString::fromStdWString(profile_.signature));
    refreshProfilePreview();
}

const ClientPreferences& SettingsPage::preferences() const { return preferences_; }

void SettingsPage::setClientPreferences(const ClientPreferences& preferences)
{
    preferences_ = preferences;
    refreshPreferenceControls();
}

void SettingsPage::setSetupMode(bool setupMode)
{
    setupMode_ = setupMode;
    refreshSetupMode();
    applyThemeStyles();
}

void SettingsPage::refreshProfilePreview()
{
    if (avatarPreviewLabel_) {
        const QString name = QString::fromStdWString(profile_.displayName).trimmed();
        if (!avatarPixmap_.isNull()) {
            // 显示圆形头像
            const int sz = avatarPreviewLabel_->width();
            QPixmap scaled = avatarPixmap_.scaled(sz, sz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            QPixmap rounded(sz, sz);
            rounded.fill(Qt::transparent);
            QPainter painter(&rounded);
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath clipPath;
            clipPath.addEllipse(0, 0, sz, sz);
            painter.setClipPath(clipPath);
            painter.drawPixmap((sz - scaled.width()) / 2, (sz - scaled.height()) / 2, scaled);
            painter.end();
            avatarPreviewLabel_->setPixmap(rounded);
            avatarPreviewLabel_->setText(QString());
            avatarPreviewLabel_->setStyleSheet(QString());
        } else {
            // 显示首字母圆形背景
            avatarPreviewLabel_->setPixmap(QPixmap());
            avatarPreviewLabel_->setText(name.isEmpty() ? QStringLiteral("?") : name.left(1));
            avatarPreviewLabel_->setStyleSheet(QStringLiteral(
                "QLabel { background:%1; color:white; border-radius:48px; font-size:36px; font-weight:700; }")
                .arg(AppStyle::accent()));
        }
    }
    if (accountSummaryLabel_) {
        const QString name = QString::fromStdWString(profile_.displayName);
        const QString dept = QString::fromStdWString(profile_.department);
        accountSummaryLabel_->setText(QStringLiteral("%1 \u00B7 %2").arg(
            name.isEmpty() ? QStringLiteral("\u672A\u8BBE\u7F6E") : name,
            dept.isEmpty() ? QStringLiteral("\u672A\u586B\u5199\u90E8\u95E8") : dept));
    }
}

void SettingsPage::refreshPreferenceControls()
{
    updatingControls_ = true;
    if (themeModeCombo_) themeModeCombo_->setCurrentIndex(themeModeToIndex(preferences_.themeMode));
    if (navigationModeCombo_) navigationModeCombo_->setCurrentIndex(navigationModeToIndex(preferences_.navigationDisplayMode));
    if (stackSwitchModeCombo_) stackSwitchModeCombo_->setCurrentIndex(stackSwitchModeToIndex(preferences_.stackSwitchMode));
    if (windowPaintModeCombo_) windowPaintModeCombo_->setCurrentIndex(windowBackgroundStyleToIndex(preferences_.windowBackgroundStyle));
    if (windowDisplayModeCombo_) windowDisplayModeCombo_->setCurrentIndex(windowDisplayModeToIndex(preferences_.windowDisplayMode));
    if (userCardSwitch_) userCardSwitch_->setIsToggled(preferences_.userInfoCardVisible);
    updatingControls_ = false;
}

void SettingsPage::refreshSetupMode()
{
    if (saveButton_)
        saveButton_->setText(setupMode_ ? QStringLiteral("\u4FDD\u5B58\u5E76\u8FDB\u5165") : QStringLiteral("\u4FDD\u5B58\u8BBE\u7F6E"));
    if (saveStatusLabel_)
        saveStatusLabel_->setText(setupMode_
            ? QStringLiteral("\u9996\u6B21\u8BBE\u7F6E\u671F\u95F4\u4FDD\u5B58\u540E\u8FDB\u5165\u6D88\u606F\u9875\u3002")
            : QStringLiteral("\u8D44\u6599\u548C\u7CFB\u7EDF\u8BBE\u7F6E\u652F\u6301\u4E00\u8D77\u4FDD\u5B58\u3002"));
}

void SettingsPage::previewSystemPreferences()
{
    if (eTheme->getThemeMode() != preferences_.themeMode) {
        eTheme->setThemeMode(preferences_.themeMode);
    }
    auto* hostWindow = qobject_cast<ElaWindow*>(window());
    if (!hostWindow) return;
    applyClientAppearance(hostWindow, preferences_, !setupMode_);
}

void SettingsPage::persistSystemPreferences()
{
    if (setupMode_) {
        return;
    }

    ClientPreferences persistedPreferences = preferences_;
    persistedPreferences.initialSetupCompleted = true;

    QString errorMessage;
    if (!ClientPreferencesStore::save(dataRoot_, persistedPreferences, &errorMessage)) {
        if (saveStatusLabel_) {
            saveStatusLabel_->setText(QStringLiteral("\u7CFB\u7EDF\u8BBE\u7F6E\u4FDD\u5B58\u5931\u8D25: %1")
                                          .arg(errorMessage));
        }
        return;
    }

    preferences_ = persistedPreferences;
    if (saveStatusLabel_) {
        saveStatusLabel_->setText(QStringLiteral("\u7CFB\u7EDF\u8BBE\u7F6E\u5DF2\u5E94\u7528\u5E76\u4FDD\u5B58\u3002"));
    }
}

Profile SettingsPage::collectProfile() const
{
    Profile p = profile_;
    if (displayNameEdit_) p.displayName = displayNameEdit_->text().trimmed().toStdWString();
    if (titleEdit_) p.jobTitle = titleEdit_->text().trimmed().toStdWString();
    if (departmentEdit_) p.department = departmentEdit_->text().trimmed().toStdWString();
    if (emailEdit_) p.email = emailEdit_->text().trimmed().toStdWString();
    if (phoneEdit_) p.phoneNumber = phoneEdit_->text().trimmed().toStdWString();
    if (signatureEdit_) p.signature = signatureEdit_->toPlainText().trimmed().toStdWString();
    return p;
}

ClientPreferences SettingsPage::collectPreferences() const
{
    ClientPreferences prefs = preferences_;
    if (themeModeCombo_) prefs.themeMode = indexToThemeMode(themeModeCombo_->currentIndex());
    if (navigationModeCombo_) prefs.navigationDisplayMode = indexToNavigationMode(navigationModeCombo_->currentIndex());
    if (stackSwitchModeCombo_) prefs.stackSwitchMode = indexToStackSwitchMode(stackSwitchModeCombo_->currentIndex());
    if (windowPaintModeCombo_) {
        prefs.windowBackgroundStyle = indexToWindowBackgroundStyle(windowPaintModeCombo_->currentIndex());
        prefs.windowPaintMode = prefs.windowBackgroundStyle == ClientWindowBackgroundStyle::Standard
            ? ElaWindowType::Normal
            : (prefs.windowBackgroundStyle == ClientWindowBackgroundStyle::Dynamic
                   ? ElaWindowType::Movie
                   : ElaWindowType::Pixmap);
    }
    if (windowDisplayModeCombo_) prefs.windowDisplayMode = indexToWindowDisplayMode(windowDisplayModeCombo_->currentIndex());
    if (userCardSwitch_) prefs.userInfoCardVisible = userCardSwitch_->getIsToggled();
    return prefs;
}

OutlookConnectionSettings SettingsPage::collectOutlookSettings() const
{
    OutlookConnectionSettings settings = initialOutlookSettings_;
    if (outlookEnabledCheck_) settings.enabled = outlookEnabledCheck_->isChecked();
    if (outlookServerUrlEdit_) settings.serverUrl = outlookServerUrlEdit_->text().trimmed();
    if (outlookUsernameEdit_) settings.username = outlookUsernameEdit_->text().trimmed();
    if (outlookPasswordEdit_) settings.password = outlookPasswordEdit_->text();
    if (outlookEmailEdit_) settings.accountEmail = outlookEmailEdit_->text().trimmed();
    if (outlookDisplayNameEdit_) settings.displayName = outlookDisplayNameEdit_->text().trimmed();
    if (outlookNotifyEnabledCheck_) settings.notificationsEnabled = outlookNotifyEnabledCheck_->isChecked();
    if (outlookPollIntervalSpin_) settings.notificationPollIntervalMinutes = outlookPollIntervalSpin_->value();
    return settings;
}

void SettingsPage::setOutlookAuthResult(const QString& email, const QString& displayName)
{
    if (outlookEmailEdit_) outlookEmailEdit_->setText(email);
    if (outlookDisplayNameEdit_) outlookDisplayNameEdit_->setText(displayName);
}

void SettingsPage::setOutlookTestStatus(const QString& text)
{
    if (outlookAuthStatusLabel_)
        outlookAuthStatusLabel_->setText(text);
}

void SettingsPage::setAvatarPixmap(const QPixmap& pixmap)
{
    avatarPixmap_ = pixmap;
    refreshProfilePreview();
}

void SettingsPage::setCheckUpdateStatus(const QString& text, const QString& color)
{
    if (checkUpdateStatusLabel_) {
        checkUpdateStatusLabel_->setText(text);
        if (!color.isEmpty())
            checkUpdateStatusLabel_->setStyleSheet(QStringLiteral("font-size:12px; color:%1;").arg(color));
    }
}

void SettingsPage::setCheckUpdateButtonEnabled(bool enabled)
{
    if (checkUpdateButton_) checkUpdateButton_->setEnabled(enabled);
}

void SettingsPage::setCleanupStatus(const QString& text)
{
    if (cleanupStatusLabel_) {
        cleanupStatusLabel_->setText(text);
    }
}
