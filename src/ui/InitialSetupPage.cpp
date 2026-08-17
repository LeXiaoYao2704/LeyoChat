#include "InitialSetupPage.h"

#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <ElaFrame.h>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <ElaStackedWidget.h>
#include <QVBoxLayout>

#include "ClientAppearance.h"
#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToolButton.h"
#include "ElaWindow.h"

namespace {

constexpr int kWizardCardWidth = 456;
constexpr int kWizardCardHeight = (kWizardCardWidth * 14) / 10;
constexpr int kAvatarPreviewSize = 132;
constexpr int kStepCount = 4;
constexpr int kEntryFlowStepCount = 1;
constexpr int kEntryFlowStepIndex = 3;

QPoint mouseGlobalPosition(QMouseEvent* event)
{
    return event ? event->globalPosition().toPoint() : QPoint();
}

QString stepTitle(int stepIndex)
{
    switch (stepIndex)
    {
    case 0:
        return QStringLiteral("先放一张头像");
    case 1:
        return QStringLiteral("补齐你的身份信息");
    case 2:
        return QStringLiteral("挑一个你喜欢的客户端风格");
    case 3:
    default:
        return QStringLiteral("欢迎使用 LeyoChat");
    }
}

QString stepDescription(int stepIndex)
{
    switch (stepIndex)
    {
    case 0:
        return QStringLiteral("头像会同步到左侧用户卡、会话气泡和发现广播里，先把第一印象定下来。");
    case 1:
        return QStringLiteral("这里只保留最关键的三项资料：名称、职位和所在部门，后续在设置页还能继续补充。");
    case 2:
        return QStringLiteral("第三步只放和界面气质直接相关的配置：主题、页面切换动画和窗口渲染效果。");
    case 3:
    default:
        return QStringLiteral("确认无误后点击开始，LeyoChat 会进入正常工作台并启动在线发现与实时会话。");
    }
}

ElaText* createTitleLabel(const QString& objectName, QWidget* parent)
{
    auto* label = new ElaText(parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

QWidget* createPreferenceRow(const QString& title,
                             const QString& description,
                             QWidget* control,
                             QWidget* parent)
{
    auto* row = new ElaScrollPageArea(parent);
    row->setBorderRadius(18);

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(14);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);

    auto* titleLabel = new ElaText(title, row);
    titleLabel->setObjectName(QStringLiteral("InitialSetupPreferenceTitleLabel"));
    textLayout->addWidget(titleLabel);

    auto* descriptionLabel = new ElaText(description, row);
    descriptionLabel->setObjectName(QStringLiteral("InitialSetupPreferenceDescriptionLabel"));
    descriptionLabel->setWordWrap(true);
    textLayout->addWidget(descriptionLabel);

    layout->addLayout(textLayout, 1);
    layout->addWidget(control, 0, Qt::AlignVCenter);
    return row;
}

QString initialSetupPageStyleSheet()
{
    const ElaThemeType::ThemeMode themeMode = eTheme->getThemeMode();
    const QColor text = ElaThemeColor(themeMode, BasicText);
    const QColor muted = ElaThemeColor(themeMode, BasicDetailsText);
    const QColor primary = ElaThemeColor(themeMode, PrimaryNormal);
    const QColor panel = ElaThemeColor(themeMode, BasicBase);
    const QColor border = ElaThemeColor(themeMode, BasicBorder);
    const QColor borderHover = ElaThemeColor(themeMode, BasicBorderHover);
    QColor cardBorder = primary;
    cardBorder.setAlpha(themeMode == ElaThemeType::Light ? 42 : 68);
    QColor halo = primary;
    halo.setAlpha(themeMode == ElaThemeType::Light ? 26 : 38);

    QString styleSheet;
    styleSheet += QStringLiteral(
        "QWidget#InitialSetupPage { background:transparent; border:none; }");
    styleSheet += QStringLiteral("QFrame#InitialSetupCard { background:transparent; border:none; }");
    styleSheet += QStringLiteral(
        "QLabel#InitialSetupStepCounterLabel { color:%1; font-size:11px; font-weight:700; letter-spacing:1px; }")
                      .arg(primary.name(QColor::HexArgb));
    styleSheet += QStringLiteral(
        "QLabel#InitialSetupTitleLabel { color:%1; font-size:28px; font-weight:800; }")
                      .arg(text.name(QColor::HexArgb));
    styleSheet += QStringLiteral(
        "QLabel#InitialSetupDescriptionLabel, QLabel#InitialSetupStatusLabel, QLabel#InitialSetupMetaLabel, QLabel#InitialSetupWelcomeSummaryLabel { color:%1; font-size:12px; }")
                      .arg(muted.name(QColor::HexArgb));
    styleSheet += QStringLiteral(
        "QFrame#InitialSetupAvatarHalo { background:%1; border:1px solid %2; border-radius:92px; }")
                      .arg(halo.name(QColor::HexArgb), cardBorder.name(QColor::HexArgb));
    styleSheet += QStringLiteral(
        "QLabel#InitialSetupAvatarPreviewLabel { background:%1; border:1px solid %2; border-radius:%3px; }")
                      .arg(panel.name(QColor::HexArgb), border.name(QColor::HexArgb), QString::number(kAvatarPreviewSize / 2));
    styleSheet += QStringLiteral(
        "QLabel#InitialSetupPreferenceTitleLabel { color:%1; font-size:14px; font-weight:700; }")
                      .arg(text.name(QColor::HexArgb));
    styleSheet += QStringLiteral(
        "QLabel#InitialSetupPreferenceDescriptionLabel { color:%1; font-size:12px; }")
                      .arg(muted.name(QColor::HexArgb));
    styleSheet += QStringLiteral(
        "QFrame#InitialSetupWelcomePanel { background:transparent; border:none; }");
    styleSheet += QStringLiteral(
        "QFrame#ElaScrollPageArea { background:%1; border:1px solid %2; border-radius:18px; }")
                      .arg(panel.name(QColor::HexArgb), border.name(QColor::HexArgb));
    styleSheet += QStringLiteral(
        "QFrame#ElaScrollPageArea:hover { border:1px solid %1; }")
                      .arg(borderHover.name(QColor::HexArgb));
    return styleSheet;
}

} // namespace

InitialSetupPage::InitialSetupPage(const Profile& profile,
                                   const ClientPreferences& preferences,
                                   const QString& dataRoot,
                                   QWidget* parent)
    : QWidget(parent),
      dataRoot_(dataRoot),
      profile_(profile),
      preferences_(preferences)
{
    setObjectName(QStringLiteral("InitialSetupPage"));
    setFixedSize(kWizardCardWidth, kWizardCardHeight);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    wizardCard_ = new ElaFrame(this);
    wizardCard_->setObjectName(QStringLiteral("InitialSetupCard"));
    wizardCard_->setFixedSize(kWizardCardWidth, kWizardCardHeight);
    auto* cardLayout = new QVBoxLayout(wizardCard_);
    cardLayout->setContentsMargins(28, 26, 28, 26);
    cardLayout->setSpacing(20);

    // ─── Header bar ───
    headerBar_ = new QWidget(wizardCard_);
    headerBar_->installEventFilter(this);
    auto* headerLayout = new QHBoxLayout(headerBar_);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    previousStepButton_ = new ElaToolButton(wizardCard_);
    previousStepButton_->setFixedSize(30, 30);
    previousStepButton_->setIsTransparent(true);
    previousStepButton_->setElaIcon(ElaIconType::ArrowLeft);
    previousStepButton_->setToolTip(QStringLiteral("上一步"));
    headerLayout->addWidget(previousStepButton_);

    nextStepButton_ = new ElaToolButton(wizardCard_);
    nextStepButton_->setFixedSize(30, 30);
    nextStepButton_->setIsTransparent(true);
    nextStepButton_->setElaIcon(ElaIconType::ArrowRight);
    nextStepButton_->setToolTip(QStringLiteral("下一步"));
    headerLayout->addWidget(nextStepButton_);

    stepCounterLabel_ = createTitleLabel(QStringLiteral("InitialSetupStepCounterLabel"), headerBar_);
    stepCounterLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    headerLayout->addWidget(stepCounterLabel_);

    headerLayout->addStretch(1);

    closeButton_ = new ElaToolButton(wizardCard_);
    closeButton_->setFixedSize(30, 30);
    closeButton_->setIsTransparent(true);
    closeButton_->setElaIcon(ElaIconType::Xmark);
    closeButton_->setToolTip(QStringLiteral("关闭"));
    headerLayout->addWidget(closeButton_);
    cardLayout->addWidget(headerBar_);

    titleLabel_ = createTitleLabel(QStringLiteral("InitialSetupTitleLabel"), wizardCard_);
    titleLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    cardLayout->addWidget(titleLabel_);

    descriptionLabel_ = createTitleLabel(QStringLiteral("InitialSetupDescriptionLabel"), wizardCard_);
    descriptionLabel_->hide();

    stepStack_ = new ElaStackedWidget(wizardCard_);
    cardLayout->addWidget(stepStack_, 1);

    statusLabel_ = createTitleLabel(QStringLiteral("InitialSetupStatusLabel"), wizardCard_);
    statusLabel_->hide();

    rootLayout->addWidget(wizardCard_);
    wizardCard_->installEventFilter(this);

    // ─── Step 0: 头像 ───
    auto* avatarStep = new QWidget(stepStack_);
    auto* avatarLayout = new QVBoxLayout(avatarStep);
    avatarLayout->setContentsMargins(0, 0, 0, 0);
    avatarLayout->setSpacing(18);
    avatarLayout->addStretch(3);

    auto* avatarHalo = new ElaFrame(avatarStep);
    avatarHalo->setObjectName(QStringLiteral("InitialSetupAvatarHalo"));
    avatarHalo->setFixedSize(184, 184);
    auto* avatarHaloLayout = new QVBoxLayout(avatarHalo);
    avatarHaloLayout->setContentsMargins(26, 26, 26, 26);
    avatarHaloLayout->setSpacing(0);

    avatarPreviewLabel_ = new ElaText(avatarHalo);
    avatarPreviewLabel_->setObjectName(QStringLiteral("InitialSetupAvatarPreviewLabel"));
    avatarPreviewLabel_->setAlignment(Qt::AlignCenter);
    avatarPreviewLabel_->setFixedSize(kAvatarPreviewSize, kAvatarPreviewSize);
    avatarHaloLayout->addWidget(avatarPreviewLabel_, 0, Qt::AlignCenter);
    avatarLayout->addWidget(avatarHalo, 0, Qt::AlignHCenter);

    avatarMetaLabel_ = createTitleLabel(QStringLiteral("InitialSetupMetaLabel"), avatarStep);
    avatarMetaLabel_->setAlignment(Qt::AlignCenter);
    avatarLayout->addWidget(avatarMetaLabel_);

    auto* avatarButtons = new QHBoxLayout();
    avatarButtons->setContentsMargins(0, 0, 0, 0);
    avatarButtons->setSpacing(10);
    avatarButtons->addStretch(1);

    chooseAvatarButton_ = new ElaPushButton(avatarStep);
    chooseAvatarButton_->setText(QStringLiteral("选择头像"));
    chooseAvatarButton_->setFixedHeight(40);
    chooseAvatarButton_->setMinimumWidth(132);
    chooseAvatarButton_->setBorderRadius(12);
    avatarButtons->addWidget(chooseAvatarButton_);

    resetAvatarButton_ = new ElaPushButton(avatarStep);
    resetAvatarButton_->setText(QStringLiteral("默认头像"));
    resetAvatarButton_->setFixedHeight(40);
    resetAvatarButton_->setMinimumWidth(112);
    resetAvatarButton_->setBorderRadius(12);
    avatarButtons->addWidget(resetAvatarButton_);
    avatarButtons->addStretch(1);
    avatarLayout->addLayout(avatarButtons);
    avatarLayout->addStretch(2);
    stepStack_->addWidget(avatarStep);

    // ─── Step 1: 身份信息 ───
    auto* infoStep = new QWidget(stepStack_);
    auto* infoLayout = new QVBoxLayout(infoStep);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(14);
    infoLayout->addStretch(1);

    displayNameEdit_ = new ElaLineEdit(infoStep);
    displayNameEdit_->setMinimumHeight(42);
    displayNameEdit_->setMinimumWidth(176);
    displayNameEdit_->setMaxLength(10);
    displayNameEdit_->setPlaceholderText(QStringLiteral("例如：林三撇"));
    infoLayout->addWidget(createPreferenceRow(
        QStringLiteral("显示名称"),
        QStringLiteral("习惯别人如何称呼您"),
        displayNameEdit_, infoStep));

    titleEdit_ = new ElaLineEdit(infoStep);
    titleEdit_->setMinimumHeight(42);
    titleEdit_->setMinimumWidth(176);
    titleEdit_->setPlaceholderText(QStringLiteral("例如：软件工程师"));
    infoLayout->addWidget(createPreferenceRow(
        QStringLiteral("岗位"),
        QStringLiteral("您的角色是什么"),
        titleEdit_, infoStep));

    departmentEdit_ = new ElaLineEdit(infoStep);
    departmentEdit_->setMinimumHeight(42);
    departmentEdit_->setMinimumWidth(176);
    departmentEdit_->setPlaceholderText(QStringLiteral("例如：控制软件与公共设计部"));
    infoLayout->addWidget(createPreferenceRow(
        QStringLiteral("所在部门"),
        QStringLiteral("您所在的组织是什么"),
        departmentEdit_, infoStep));

    infoLayout->addStretch(2);
    stepStack_->addWidget(infoStep);

    // ─── Step 2: 客户端风格 ───
    auto* preferencesStep = new QWidget(stepStack_);
    auto* preferencesLayout = new QVBoxLayout(preferencesStep);
    preferencesLayout->setContentsMargins(0, 0, 0, 0);
    preferencesLayout->setSpacing(14);
    preferencesLayout->addStretch(1);

    themeModeCombo_ = new ElaComboBox(preferencesStep);
    themeModeCombo_->addItem(QStringLiteral("日间模式"));
    themeModeCombo_->addItem(QStringLiteral("夜间模式"));
    themeModeCombo_->setMinimumWidth(166);

    stackSwitchModeCombo_ = new ElaComboBox(preferencesStep);
    stackSwitchModeCombo_->addItem(QStringLiteral("Popup"));
    stackSwitchModeCombo_->addItem(QStringLiteral("Scale"));
    stackSwitchModeCombo_->addItem(QStringLiteral("Flip"));
    stackSwitchModeCombo_->addItem(QStringLiteral("Blur"));
    stackSwitchModeCombo_->addItem(QStringLiteral("None"));
    stackSwitchModeCombo_->setMinimumWidth(166);

    windowPaintModeCombo_ = new ElaComboBox(preferencesStep);
    windowPaintModeCombo_->addItem(QStringLiteral("\u6807\u51C6"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u84DD\u96FE"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u6668\u96FE\u7EFF"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u6696\u767D\u7EB8\u611F"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u6781\u7B80\u51B7\u7070"));
    windowPaintModeCombo_->addItem(QStringLiteral("\u52A8\u6001"));
    windowPaintModeCombo_->setMinimumWidth(166);

    windowDisplayModeCombo_ = new ElaComboBox(preferencesStep);
    windowDisplayModeCombo_->addItem(QStringLiteral("Normal"));
    windowDisplayModeCombo_->addItem(QStringLiteral("ElaMica"));
#if defined(Q_OS_WIN)
    windowDisplayModeCombo_->addItem(QStringLiteral("Mica"));
    windowDisplayModeCombo_->addItem(QStringLiteral("Mica-Alt"));
    windowDisplayModeCombo_->addItem(QStringLiteral("Acrylic"));
    windowDisplayModeCombo_->addItem(QStringLiteral("Dwm-Blur"));
#endif
    windowDisplayModeCombo_->setMinimumWidth(166);

    preferencesLayout->addWidget(createPreferenceRow(
        QStringLiteral("主题模式"), QStringLiteral("选择你喜欢的界面亮度"),
        themeModeCombo_, preferencesStep));
    preferencesLayout->addWidget(createPreferenceRow(
        QStringLiteral("页面切换动画"), QStringLiteral("让页面切换更有层次"),
        stackSwitchModeCombo_, preferencesStep));
    preferencesLayout->addWidget(createPreferenceRow(
        QStringLiteral("窗口背景"), QStringLiteral("多样化你的客户端"),
        windowPaintModeCombo_, preferencesStep));
    preferencesLayout->addWidget(createPreferenceRow(
        QStringLiteral("显示效果"), QStringLiteral("选择窗口透明效果"),
        windowDisplayModeCombo_, preferencesStep));
    preferencesLayout->addStretch(2);
    stepStack_->addWidget(preferencesStep);

    // ─── Step 3: 欢迎页 ───
    auto* welcomeStep = new QWidget(stepStack_);
    auto* welcomeLayout = new QVBoxLayout(welcomeStep);
    welcomeLayout->setContentsMargins(0, 0, 0, 0);
    welcomeLayout->setSpacing(16);
    welcomeLayout->addStretch(2);

    auto* welcomePanel = new ElaFrame(welcomeStep);
    welcomePanel->setObjectName(QStringLiteral("InitialSetupWelcomePanel"));
    auto* welcomePanelLayout = new QVBoxLayout(welcomePanel);
    welcomePanelLayout->setContentsMargins(26, 28, 26, 28);
    welcomePanelLayout->setSpacing(16);

    welcomeLogoLabel_ = new ElaText(welcomePanel);
    welcomeLogoLabel_->setAlignment(Qt::AlignCenter);
    welcomePanelLayout->addWidget(welcomeLogoLabel_);

    welcomeSummaryLabel_ = createTitleLabel(QStringLiteral("InitialSetupWelcomeSummaryLabel"), welcomePanel);
    welcomeSummaryLabel_->setAlignment(Qt::AlignCenter);
    welcomePanelLayout->addWidget(welcomeSummaryLabel_);

    startButton_ = new ElaPushButton(welcomePanel);
    startButton_->setText(QStringLiteral("保存并进入"));
    startButton_->setFixedHeight(44);
    startButton_->setBorderRadius(14);
    welcomePanelLayout->addWidget(startButton_);

    welcomeLayout->addWidget(welcomePanel);
    welcomeLayout->addStretch(1);
    stepStack_->addWidget(welcomeStep);

    // ─── Connections ───
    connect(chooseAvatarButton_, &ElaPushButton::clicked, this, [this]() {
        const QString avatarPath = QFileDialog::getOpenFileName(
            this, QStringLiteral("选择头像"), QString(),
            QStringLiteral("头像图片 (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (!avatarPath.isEmpty())
        {
            // 头像路径暂存到 profile 的 signature 临时字段（LeyoChat 头像由 IdentityService 管理）
            refreshAvatarPreview();
        }
    });
    connect(resetAvatarButton_, &ElaPushButton::clicked, this, [this]() {
        refreshAvatarPreview();
    });
    connect(previousStepButton_, &ElaToolButton::clicked, this, &InitialSetupPage::goToPreviousStep);
    connect(nextStepButton_, &ElaToolButton::clicked, this, &InitialSetupPage::goToNextStep);
    connect(closeButton_, &ElaToolButton::clicked, this, [this]() {
        if (QWidget* hostWindow = window())
        {
            hostWindow->close();
        }
    });
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this](ElaThemeType::ThemeMode) {
        applyThemeStyles();
    });

    connect(themeModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [this](int) {
        if (updatingControls_) return;
        preferences_ = collectPreferences();
        previewPreferences();
    });
    connect(stackSwitchModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [this](int) {
        if (updatingControls_) return;
        preferences_ = collectPreferences();
        previewPreferences();
    });
    connect(windowPaintModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [this](int) {
        if (updatingControls_) return;
        preferences_ = collectPreferences();
        previewPreferences();
    });
    connect(windowDisplayModeCombo_, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [this](int) {
        if (updatingControls_) return;
        preferences_ = collectPreferences();
        previewPreferences();
    });

    connect(startButton_, &ElaPushButton::clicked, this, [this]() {
        Profile nextProfile = collectProfile();
        if (QString::fromStdWString(nextProfile.displayName).trimmed().isEmpty())
        {
            if (statusLabel_) {
                statusLabel_->setText(QStringLiteral("显示名称不能为空。"));
            }
            ElaMessageBar::warning(
                ElaMessageBarType::TopRight,
                QStringLiteral("显示名称不能为空"),
                QStringLiteral("先填写一个用于聊天展示的名称，再进入客户端。"),
                2400, this);
            return;
        }

        ClientPreferences nextPreferences = collectPreferences();
        nextPreferences.initialSetupCompleted = true;

        QString errorMessage;
        if (!ClientPreferencesStore::save(dataRoot_, nextPreferences, &errorMessage))
        {
            ElaMessageBar::warning(
                ElaMessageBarType::TopRight,
                QStringLiteral("偏好保存失败"),
                errorMessage, 2600, this);
            return;
        }

        profile_ = nextProfile;
        preferences_ = nextPreferences;
        emit setupCompleted(profile_, preferences_);
    });

    setProfile(profile_);
    setClientPreferences(preferences_);
    currentStepIndex_ = kEntryFlowStepIndex;
    refreshStepContent();
    applyThemeStyles();
    previewPreferences();
}

const Profile& InitialSetupPage::profile() const { return profile_; }
const ClientPreferences& InitialSetupPage::preferences() const { return preferences_; }

void InitialSetupPage::setProfile(const Profile& profile)
{
    profile_ = profile;
    if (displayNameEdit_)
        displayNameEdit_->setText(QString::fromStdWString(profile_.displayName));
    if (titleEdit_)
        titleEdit_->setText(QString::fromStdWString(profile_.jobTitle));
    if (departmentEdit_)
        departmentEdit_->setText(QString::fromStdWString(profile_.department));
    refreshAvatarPreview();
    refreshWelcomePage();
}

void InitialSetupPage::setClientPreferences(const ClientPreferences& preferences)
{
    preferences_ = preferences;
    refreshPreferenceControls();
    refreshWelcomePage();
}

int InitialSetupPage::currentStep() const { return 1; }
int InitialSetupPage::stepCount() const { return kEntryFlowStepCount; }
bool InitialSetupPage::hasPreviousStep() const { return false; }
bool InitialSetupPage::hasNextStep() const { return false; }

void InitialSetupPage::goToPreviousStep()
{
    return;
}

void InitialSetupPage::goToNextStep()
{
    return;
}

void InitialSetupPage::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event && (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange))
        applyThemeStyles();
}

bool InitialSetupPage::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == wizardCard_ || watched == headerBar_) && event)
    {
        switch (event->type())
        {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && window()) {
                draggingWindow_ = true;
                windowDragOffset_ = mouseGlobalPosition(me) - window()->frameGeometry().topLeft();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* me = static_cast<QMouseEvent*>(event);
            if (draggingWindow_ && window() && !window()->isMaximized() && !window()->isFullScreen()) {
                window()->move(mouseGlobalPosition(me) - windowDragOffset_);
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
            draggingWindow_ = false;
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void InitialSetupPage::applyThemeStyles()
{
    if (applyingThemeStyles_) return;
    applyingThemeStyles_ = true;
    setStyleSheet(initialSetupPageStyleSheet());
    if (welcomeLogoLabel_)
        welcomeLogoLabel_->setPixmap(QPixmap(QStringLiteral(":/icons/app-logo.png")).scaled(108, 108, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    applyingThemeStyles_ = false;
}

void InitialSetupPage::refreshStepContent()
{
    if (stepStack_) stepStack_->setCurrentIndex(currentStepIndex_);
    if (stepCounterLabel_)
        stepCounterLabel_->setText(QStringLiteral("STEP %1 / %2")
                                       .arg(currentStep(), 2, 10, QLatin1Char('0'))
                                       .arg(stepCount(), 2, 10, QLatin1Char('0')));
    if (titleLabel_) titleLabel_->setText(stepTitle(currentStepIndex_));
    if (descriptionLabel_) descriptionLabel_->setText(stepDescription(currentStepIndex_));
    if (previousStepButton_) {
        previousStepButton_->setEnabled(false);
        previousStepButton_->setVisible(false);
    }
    if (nextStepButton_) {
        nextStepButton_->setEnabled(false);
        nextStepButton_->setVisible(false);
    }
    refreshWelcomePage();
}

void InitialSetupPage::refreshAvatarPreview()
{
    if (!avatarPreviewLabel_) return;
    const QString name = QString::fromStdWString(profile_.displayName).trimmed();
    if (name.isEmpty())
    {
        avatarPreviewLabel_->setText(QStringLiteral("?"));
    }
    else
    {
        avatarPreviewLabel_->setText(name.left(1));
    }
    avatarPreviewLabel_->setAlignment(Qt::AlignCenter);
    if (avatarMetaLabel_)
        avatarMetaLabel_->setText(name.isEmpty() ? QStringLiteral("未设置") : name);
}

void InitialSetupPage::refreshPreferenceControls()
{
    updatingControls_ = true;
    if (themeModeCombo_) themeModeCombo_->setCurrentIndex(themeModeToIndex(preferences_.themeMode));
    if (stackSwitchModeCombo_) stackSwitchModeCombo_->setCurrentIndex(stackSwitchModeToIndex(preferences_.stackSwitchMode));
    if (windowPaintModeCombo_) windowPaintModeCombo_->setCurrentIndex(windowBackgroundStyleToIndex(preferences_.windowBackgroundStyle));
    if (windowDisplayModeCombo_) windowDisplayModeCombo_->setCurrentIndex(windowDisplayModeToIndex(preferences_.windowDisplayMode));
    updatingControls_ = false;
}

void InitialSetupPage::refreshWelcomePage()
{
    if (!welcomeSummaryLabel_) return;
    const QString name = QString::fromStdWString(profile_.displayName).trimmed();
    const QString themeText = preferences_.themeMode == ElaThemeType::Dark
        ? QStringLiteral("夜间主题") : QStringLiteral("日间主题");
    welcomeSummaryLabel_->setText(
        QStringLiteral("准备就绪后将以 %1 的身份进入客户端。\n当前外观：%2。")
            .arg(name.isEmpty() ? QStringLiteral("默认用户") : name, themeText));
}

void InitialSetupPage::previewPreferences()
{
    auto* hostWindow = qobject_cast<ElaWindow*>(window());
    if (!hostWindow) return;
    applyClientAppearance(hostWindow, collectPreferences(), false);
}

Profile InitialSetupPage::collectProfile() const
{
    Profile p = profile_;
    if (displayNameEdit_) p.displayName = displayNameEdit_->text().trimmed().toStdWString();
    if (titleEdit_) p.jobTitle = titleEdit_->text().trimmed().toStdWString();
    if (departmentEdit_) p.department = departmentEdit_->text().trimmed().toStdWString();
    return p;
}

ClientPreferences InitialSetupPage::collectPreferences() const
{
    ClientPreferences prefs = preferences_;
    if (themeModeCombo_) prefs.themeMode = indexToThemeMode(themeModeCombo_->currentIndex());
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
    return prefs;
}
