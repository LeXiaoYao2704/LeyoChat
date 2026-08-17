#include "ui/AiKnowledgePanel.h"

#include "ui/AppStyle.h"
#include "ui/MarkdownRenderer.h"

#include <ElaComboBox.h>
#include <QColor>
#include <QDesktopServices>
#include <QEvent>
#include <QFrame>
#include <ElaFrame.h>
#include <QGridLayout>
#include <QHBoxLayout>
#include <ElaText.h>
#include <QLineEdit>
#include <QPushButton>
#include <ElaScrollArea.h>
#include <ElaPushButton.h>
#include <ElaLineEdit.h>
#include <QStackedLayout>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString aiSourceDisplayText(const KnowServiceSource& source)
{
    if (!source.title.trimmed().isEmpty()) {
        return source.title.trimmed();
    }
    if (!source.sourceId.trimmed().isEmpty()) {
        return source.sourceId.trimmed();
    }
    if (!source.openUri.trimmed().isEmpty()) {
        return source.openUri.trimmed();
    }
    return QStringLiteral("未命名来源");
}

QString aiSourceOpenUrl(const KnowServiceSource& source)
{
    const QString openUri = source.openUri.trimmed();
    if (!openUri.isEmpty()) {
        return openUri;
    }

    return source.originalUri.trimmed();
}

QString searchModeDisplayText(const QString& mode)
{
    if (mode == QStringLiteral("hybrid"))
        return QStringLiteral("\u6DF7\u5408");
    if (mode == QStringLiteral("vector"))
        return QStringLiteral("\u5411\u91CF");
    if (mode == QStringLiteral("keyword"))
        return QStringLiteral("\u5173\u952E\u8BCD");
    return mode;
}

const QString kSearchBarStyle = QStringLiteral(
    "QFrame#aiSearchBar {"
    "  background:%1;"
    "  border:1px solid %2;"
    "  border-radius:26px;"
    "}"
    "QFrame#aiSearchBar:focus-within {"
    "  border:1px solid %3;"
    "}"
    "QLineEdit, ElaLineEdit { border:none; background:transparent; font-size:14px; color:%4; padding:0 4px; }"
    "QPushButton#aiSearchBtn, ElaPushButton#aiSearchBtn {"
    "  background:%3; color:#FFFFFF; border:none; border-radius:18px;"
    "  font-size:13px; font-weight:700; padding:6px 22px;"
    "  min-height:36px; min-width:72px;"
    "}"
    "QPushButton#aiSearchBtn:hover, ElaPushButton#aiSearchBtn:hover { background:%5; }"
    "QPushButton#aiSearchBtn:disabled, ElaPushButton#aiSearchBtn:disabled { background:%6; }");

QString searchBarStylesheet()
{
    const QString shell = AppStyle::isDarkTheme()
        ? QStringLiteral("rgba(30,34,40,170)")
        : QStringLiteral("rgba(255,255,255,150)");
    const QString border = AppStyle::isDarkTheme()
        ? QStringLiteral("rgba(255,255,255,54)")
        : QStringLiteral("rgba(255,255,255,190)");

    return kSearchBarStyle.arg(
        shell,
        border,
        AppStyle::accent(),
        AppStyle::textPrimary(),
        AppStyle::accentHover(),
        AppStyle::textMuted());
}

const QString kTopBarStyle = QStringLiteral(
    "QFrame#aiTopSearchBar {"
    "  background:%1;"
    "  border:1px solid %2;"
    "  border-radius:20px;"
    "}"
    "QLineEdit, ElaLineEdit { border:none; background:transparent; font-size:13px; color:%3; padding:0 4px; }"
    "QPushButton#aiTopSearchBtn, ElaPushButton#aiTopSearchBtn {"
    "  background:%4; color:#FFFFFF; border:none; border-radius:16px;"
    "  font-size:12px; font-weight:600; padding:4px 14px;"
    "  min-height:32px;"
    "}"
    "QPushButton#aiTopSearchBtn:hover, ElaPushButton#aiTopSearchBtn:hover { background:%5; }"
    "QPushButton#aiTopSearchBtn:disabled, ElaPushButton#aiTopSearchBtn:disabled { background:%6; }");

QString topBarStylesheet()
{
    const QString shell = AppStyle::isDarkTheme()
        ? QStringLiteral("rgba(30,34,40,170)")
        : QStringLiteral("rgba(255,255,255,150)");
    const QString border = AppStyle::isDarkTheme()
        ? QStringLiteral("rgba(255,255,255,54)")
        : QStringLiteral("rgba(255,255,255,190)");
    return kTopBarStyle.arg(
        shell,
        border,
        AppStyle::textPrimary(),
        AppStyle::accent(),
        AppStyle::accentHover(),
        AppStyle::textMuted());
}

QString cardStylesheet(const QString& objectName)
{
    return QStringLiteral("QFrame#%1 { background:%2; border:1px solid %3; border-radius:12px; }")
        .arg(objectName,
             AppStyle::isDarkTheme() ? QStringLiteral("rgba(30,34,40,180)") : QStringLiteral("rgba(255,255,255,185)"),
             AppStyle::border());
}

QString comboStylesheet()
{
    return QStringLiteral(
        "QComboBox { font-size:12px; border:1px solid %1; border-radius:11px;"
        " padding:6px 30px 6px 12px; background:%2; color:%3; min-height:22px; }"
        "QComboBox:hover { border-color:%4; background:%5; }"
        "QComboBox::drop-down { width:26px; border:none; background:transparent; }")
        .arg(AppStyle::border(),
             AppStyle::isDarkTheme() ? QStringLiteral("rgba(30,34,40,150)") : QStringLiteral("rgba(255,255,255,135)"),
             AppStyle::textSecondary(),
             AppStyle::accent(),
             AppStyle::isDarkTheme() ? QStringLiteral("rgba(38,43,50,180)") : QStringLiteral("rgba(255,255,255,190)"));
}

QString suggestionChipStylesheet()
{
    const QColor accentColor(AppStyle::accent());
    const QString accentFill = QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(accentColor.red())
        .arg(accentColor.green())
        .arg(accentColor.blue())
        .arg(AppStyle::isDarkTheme() ? 42 : 24);
    const QString accentFillHover = QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(accentColor.red())
        .arg(accentColor.green())
        .arg(accentColor.blue())
        .arg(AppStyle::isDarkTheme() ? 70 : 42);
    return QStringLiteral(
        "QPushButton#aiSuggestionChip {"
        "  background:%1; color:%2; border:1px solid %3; border-radius:14px;"
        "  padding:6px 12px; font-size:12px; font-weight:500; min-height:28px;"
        "}"
        "QPushButton#aiSuggestionChip:hover { background:%4; border-color:%5; color:%6; }"
        "QPushButton#aiSuggestionChip:pressed { background:%4; }")
        .arg(accentFill,
             AppStyle::textSecondary(),
             AppStyle::isDarkTheme() ? QStringLiteral("rgba(255,255,255,40)") : QStringLiteral("rgba(0,0,0,22)"),
             accentFillHover,
             AppStyle::accent(),
             AppStyle::textPrimary());
}

QColor rgbaColor(const QString& color, int alpha)
{
    QColor value(color);
    if (!value.isValid()) {
        value = QColor(AppStyle::accent());
    }
    value.setAlpha(alpha);
    return value;
}

void applySearchButtonStyle(ElaPushButton* button, int radius)
{
    if (!button) return;
    button->setBorderRadius(radius);
    button->setMinimumWidth(radius >= 18 ? 78 : 64);
    button->setLightDefaultColor(QColor(AppStyle::accent()));
    button->setDarkDefaultColor(QColor(AppStyle::accent()));
    button->setLightHoverColor(QColor(AppStyle::accentHover()));
    button->setDarkHoverColor(QColor(AppStyle::accentHover()));
    button->setLightPressColor(QColor(AppStyle::accentPressed()));
    button->setDarkPressColor(QColor(AppStyle::accentPressed()));
    button->setLightTextColor(Qt::white);
    button->setDarkTextColor(Qt::white);
}

void applySuggestionButtonStyle(ElaPushButton* button)
{
    if (!button) return;
    button->setBorderRadius(14);
    button->setLightDefaultColor(rgbaColor(AppStyle::accent(), 24));
    button->setDarkDefaultColor(rgbaColor(AppStyle::accent(), 42));
    button->setLightHoverColor(rgbaColor(AppStyle::accent(), 42));
    button->setDarkHoverColor(rgbaColor(AppStyle::accent(), 70));
    button->setLightPressColor(rgbaColor(AppStyle::accent(), 56));
    button->setDarkPressColor(rgbaColor(AppStyle::accent(), 88));
    button->setLightTextColor(QColor(AppStyle::textPrimary()));
    button->setDarkTextColor(QColor(AppStyle::textPrimary()));
}

QString answerDocumentStylesheet()
{
    return QStringLiteral(
        "body { color:%1; background:%2; font-family:'Microsoft YaHei','Segoe UI',sans-serif; font-size:14px; line-height:1.65; }"
        "p { margin:0 0 10px 0; }"
        "a { color:%3; text-decoration:none; }"
        "pre, code { background:%4; border-radius:6px; }"
        "blockquote { border-left:3px solid %3; margin:8px 0; padding:4px 10px; color:%5; background:%4; }")
        .arg(AppStyle::textPrimary(),
             QStringLiteral("transparent"),
             AppStyle::accent(),
             AppStyle::isDarkTheme() ? QStringLiteral("rgba(30,34,40,120)") : QStringLiteral("rgba(255,255,255,120)"),
             AppStyle::textSecondary());
}

}

AiKnowledgePanel::AiKnowledgePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("aiKnowledgePanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);
    setStyleSheet(QStringLiteral("#aiKnowledgePanel { background:transparent; }"));

    auto* stack = new QStackedLayout(this);

    // ═══════════════════════════════════════════════
    // 模式 1: 居中搜索首页（Google 风格）
    // ═══════════════════════════════════════════════
    m_heroContainer = new QWidget(this);
    m_heroContainer->setObjectName(QStringLiteral("knowledgeBrowserShell"));
    auto* heroRootLayout = new QVBoxLayout(m_heroContainer);
    heroRootLayout->setContentsMargins(0, 0, 0, 0);
    heroRootLayout->addStretch(3);

    // Logo / 标题
    m_heroTitle = new ElaText(QStringLiteral("Knowledge"), m_heroContainer);
    m_heroTitle->setAlignment(Qt::AlignCenter);
    m_heroTitle->setStyleSheet(
        QStringLiteral("font-size:36px; font-weight:700; color:%1; letter-spacing:1px;")
            .arg(AppStyle::accent()));
    heroRootLayout->addWidget(m_heroTitle);

    m_heroSubtitle = new ElaText(QStringLiteral("输入问题，从知识库中获取答案"), m_heroContainer);
    m_heroSubtitle->setAlignment(Qt::AlignCenter);
    m_heroSubtitle->setStyleSheet(
        QStringLiteral("font-size:13px; color:%1; margin-bottom:20px;")
            .arg(AppStyle::textMuted()));
    heroRootLayout->addWidget(m_heroSubtitle);

    // 搜索栏
    m_searchBarFrame = new ElaFrame(m_heroContainer);
    m_searchBarFrame->setObjectName(QStringLiteral("aiSearchBar"));
    m_searchBarFrame->setStyleSheet(searchBarStylesheet());
    m_searchBarFrame->setFixedHeight(52);
    auto* searchBarLayout = new QHBoxLayout(m_searchBarFrame);
    searchBarLayout->setContentsMargins(16, 4, 6, 4);
    searchBarLayout->setSpacing(8);

    m_queryEdit = new ElaLineEdit(m_searchBarFrame);
    m_queryEdit->setObjectName(QStringLiteral("aiQueryEdit"));
    m_queryEdit->setPlaceholderText(QStringLiteral("例如：这个项目的诊断包导出链路在哪里？"));
    m_submitButton = new ElaPushButton(QStringLiteral("搜索"), m_searchBarFrame);
    m_submitButton->setObjectName(QStringLiteral("aiSearchBtn"));
    m_submitButton->setCursor(Qt::PointingHandCursor);
    applySearchButtonStyle(m_submitButton, 18);
    searchBarLayout->addWidget(m_queryEdit, 1);
    searchBarLayout->addWidget(m_submitButton);

    auto* heroSearchRow = new QHBoxLayout;
    heroSearchRow->setContentsMargins(60, 0, 60, 0);
    heroSearchRow->addWidget(m_searchBarFrame);
    heroRootLayout->addLayout(heroSearchRow);

    // 服务选择器（居中小字）
    auto* heroServiceRow = new QHBoxLayout;
    heroServiceRow->setContentsMargins(60, 10, 60, 0);
    heroServiceRow->addStretch();
    auto* heroServiceLabel = new ElaText(QStringLiteral("知识服务："), m_heroContainer);
    heroServiceLabel->setStyleSheet(
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted()));
    m_serviceCombo = new ElaComboBox(m_heroContainer);
    m_serviceCombo->setObjectName(QStringLiteral("aiServiceCombo"));
    m_serviceCombo->setMinimumWidth(200);
    m_serviceCombo->setStyleSheet(comboStylesheet());
    heroServiceRow->addWidget(heroServiceLabel);
    heroServiceRow->addWidget(m_serviceCombo);
    heroServiceRow->addStretch();
    heroRootLayout->addLayout(heroServiceRow);

    // 推荐搜索
    auto* suggestRow = new QHBoxLayout;
    suggestRow->setContentsMargins(60, 16, 60, 0);
    suggestRow->setSpacing(0);
    suggestRow->addStretch();
    auto* suggestWrap = new QWidget(m_heroContainer);
    suggestWrap->setObjectName(QStringLiteral("aiSuggestionWrap"));
    suggestWrap->setAttribute(Qt::WA_StyledBackground, true);
    suggestWrap->setStyleSheet(QStringLiteral("QWidget#aiSuggestionWrap { background:transparent; }"));
    auto* suggestFlow = new QHBoxLayout(suggestWrap);
    suggestFlow->setContentsMargins(0, 0, 0, 0);
    suggestFlow->setSpacing(8);
    const QStringList suggestions = {
        QStringLiteral("报警确认流程"),
        QStringLiteral("诊断包导出"),
        QStringLiteral("组态服务器"),
        QStringLiteral("操作员站部署"),
    };
    for (const QString& text : suggestions) {
        auto* chip = new ElaPushButton(text, suggestWrap);
        chip->setObjectName(QStringLiteral("aiSuggestionChip"));
        chip->setStyleSheet(suggestionChipStylesheet());
        chip->setCursor(Qt::PointingHandCursor);
        applySuggestionButtonStyle(chip);
        connect(chip, &QAbstractButton::clicked, this, [this, text]() {
            m_queryEdit->setText(text);
            m_topQueryEdit->setText(text);
            switchToResultMode();
            emit querySubmitted(text);
        });
        suggestFlow->addWidget(chip);
    }
    suggestRow->addWidget(suggestWrap);
    suggestRow->addStretch();
    heroRootLayout->addLayout(suggestRow);

    heroRootLayout->addStretch(5);

    stack->addWidget(m_heroContainer);

    // ═══════════════════════════════════════════════
    // 模式 2: 结果页（搜索栏移到顶部，下方显示结果）
    // ═══════════════════════════════════════════════
    m_resultContainer = new QWidget(this);
    m_resultContainer->setObjectName(QStringLiteral("knowledgeResultShell"));
    auto* resultRootLayout = new QVBoxLayout(m_resultContainer);
    resultRootLayout->setContentsMargins(0, 0, 0, 0);
    resultRootLayout->setSpacing(0);

    // 顶部搜索栏
    m_resultToolbar = new QWidget(m_resultContainer);
    m_resultToolbar->setObjectName(QStringLiteral("knowledgeResultToolbar"));
    auto* topBarOuter = new QHBoxLayout(m_resultToolbar);
    topBarOuter->setContentsMargins(20, 10, 20, 10);
    topBarOuter->setSpacing(10);

    // 返回首页按钮
    auto* backButton = new ElaPushButton(QStringLiteral("← 首页"), m_resultToolbar);
    backButton->setObjectName(QStringLiteral("aiBackBtn"));
    backButton->setCursor(Qt::PointingHandCursor);
    connect(backButton, &QAbstractButton::clicked, this, &AiKnowledgePanel::switchToSearchMode);
    topBarOuter->addWidget(backButton);

    m_topServiceCombo = new ElaComboBox(m_resultToolbar);
    m_topServiceCombo->setObjectName(QStringLiteral("aiTopServiceCombo"));
    m_topServiceCombo->setMinimumWidth(140);
    m_topServiceCombo->setStyleSheet(comboStylesheet());
    topBarOuter->addWidget(m_topServiceCombo);

    m_topSearchBarFrame = new ElaFrame(m_resultToolbar);
    m_topSearchBarFrame->setObjectName(QStringLiteral("aiTopSearchBar"));
    m_topSearchBarFrame->setStyleSheet(topBarStylesheet());
    m_topSearchBarFrame->setFixedHeight(40);
    auto* topSearchBarLayout = new QHBoxLayout(m_topSearchBarFrame);
    topSearchBarLayout->setContentsMargins(12, 2, 4, 2);
    topSearchBarLayout->setSpacing(6);

    m_topQueryEdit = new ElaLineEdit(m_topSearchBarFrame);
    m_topQueryEdit->setObjectName(QStringLiteral("aiTopQueryEdit"));
    m_topQueryEdit->setPlaceholderText(QStringLiteral("继续提问..."));
    m_topSubmitButton = new ElaPushButton(QStringLiteral("搜索"), m_topSearchBarFrame);
    m_topSubmitButton->setObjectName(QStringLiteral("aiTopSearchBtn"));
    m_topSubmitButton->setCursor(Qt::PointingHandCursor);
    applySearchButtonStyle(m_topSubmitButton, 16);
    topSearchBarLayout->addWidget(m_topQueryEdit, 1);
    topSearchBarLayout->addWidget(m_topSubmitButton);

    topBarOuter->addWidget(m_topSearchBarFrame, 1);
    resultRootLayout->addWidget(m_resultToolbar);

    // 结果区（可滚动）
    auto* resultScroll = new ElaScrollArea(m_resultContainer);
    resultScroll->setWidgetResizable(true);
    resultScroll->setFrameShape(QFrame::NoFrame);
    resultScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* resultBody = new QWidget(resultScroll);
    resultBody->setObjectName(QStringLiteral("knowledgeResultBody"));
    resultScroll->setWidget(resultBody);
    auto* resultBodyLayout = new QVBoxLayout(resultBody);
    resultBodyLayout->setContentsMargins(40, 20, 40, 20);
    resultBodyLayout->setSpacing(16);

    // AI 回答卡片
    m_answerCard = new ElaFrame(resultBody);
    m_answerCard->setObjectName(QStringLiteral("knowledgeAnswerCard"));
    m_answerCard->setStyleSheet(
        QStringLiteral("QFrame#knowledgeAnswerCard { background:%1; border:1px solid %2; "
                       "border-left:3px solid %3; border-radius:12px; }")
            .arg(AppStyle::surface(), AppStyle::border(), AppStyle::accent()));
    auto* answerLayout = new QVBoxLayout(m_answerCard);
    answerLayout->setContentsMargins(20, 16, 20, 16);
    answerLayout->setSpacing(10);

    auto* answerIcon = new ElaText(QStringLiteral("📝  AI 回答"), m_answerCard);
    answerIcon->setStyleSheet(
        QStringLiteral("font-size:14px; font-weight:700; color:%1;")
            .arg(AppStyle::textPrimary()));
    m_answerBody = new QTextBrowser(m_answerCard);
    m_answerBody->setObjectName(QStringLiteral("aiAnswerBody"));
    m_answerBody->setOpenExternalLinks(true);
    m_answerBody->setReadOnly(true);
    m_answerBody->setFrameShape(QFrame::NoFrame);
    m_answerBody->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_answerBody->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_answerBody->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_answerBody->setStyleSheet(
        QStringLiteral("QTextBrowser { font-size:14px; color:%1; padding:4px 0; background:transparent; border:none; }")
            .arg(AppStyle::textPrimary()));
    connect(m_answerBody->document(), &QTextDocument::contentsChanged, this, [this]() {
        const int docHeight = static_cast<int>(m_answerBody->document()->size().height()) + 8;
        m_answerBody->setMinimumHeight(docHeight);
    });
    answerLayout->addWidget(answerIcon);
    answerLayout->addWidget(m_answerBody, 1);

    resultBodyLayout->addWidget(m_answerCard);

    // 命中来源卡片
    m_sourcesCard = new ElaFrame(resultBody);
    m_sourcesCard->setObjectName(QStringLiteral("aiSourcesCard"));
    m_sourcesCard->setStyleSheet(cardStylesheet(QStringLiteral("aiSourcesCard")));
    auto* sourcesLayout = new QVBoxLayout(m_sourcesCard);
    sourcesLayout->setContentsMargins(20, 16, 20, 16);
    sourcesLayout->setSpacing(10);

    m_sourcesHeader = new ElaText(QStringLiteral("📚  命中来源"), m_sourcesCard);
    m_sourcesHeader->setStyleSheet(
        QStringLiteral("font-size:14px; font-weight:700; color:%1;")
            .arg(AppStyle::textPrimary()));
    m_sourcesGrid = new QWidget(m_sourcesCard);
    m_sourcesGrid->hide();
    m_sourcesExpanded = false;
    sourcesLayout->addWidget(m_sourcesHeader);
    sourcesLayout->addWidget(m_sourcesGrid);

    resultBodyLayout->addWidget(m_sourcesCard);

    resultRootLayout->addWidget(resultScroll, 1);
    stack->addWidget(m_resultContainer);

    // 初始状态：搜索首页
    stack->setCurrentWidget(m_heroContainer);
    setAvailableServices({}, {});

    // ═══════════════════════════════════════════════
    // 信号连接
    // ═══════════════════════════════════════════════

    // 同步两个 combo 选中状态
    auto syncCombos = [this](QComboBox* source, QComboBox* target) {
        const QSignalBlocker blocker(target);
        target->clear();
        for (int i = 0; i < source->count(); ++i) {
            target->addItem(source->itemText(i), source->itemData(i));
        }
        target->setCurrentIndex(source->currentIndex());
    };

    connect(m_serviceCombo, &QComboBox::currentIndexChanged, this, [this, syncCombos](int) {
        syncCombos(m_serviceCombo, m_topServiceCombo);
        emit serviceChanged(selectedServiceName());
    });
    connect(m_topServiceCombo, &QComboBox::currentIndexChanged, this, [this, syncCombos](int) {
        syncCombos(m_topServiceCombo, m_serviceCombo);
        emit serviceChanged(selectedServiceName());
    });

    // 搜索提交（首页 + 结果页）
    auto submitQuery = [this]() {
        const QString text = m_queryEdit->text().trimmed();
        if (text.isEmpty()) {
            return;
        }
        m_topQueryEdit->setText(text);
        switchToResultMode();
        emit querySubmitted(text);
    };

    auto submitTopQuery = [this]() {
        const QString text = m_topQueryEdit->text().trimmed();
        if (text.isEmpty()) {
            return;
        }
        m_queryEdit->setText(text);
        emit querySubmitted(text);
    };

    connect(m_submitButton, &QAbstractButton::clicked, this, submitQuery);
    connect(m_queryEdit, &QLineEdit::returnPressed, this, submitQuery);
    connect(m_topSubmitButton, &QAbstractButton::clicked, this, submitTopQuery);
    connect(m_topQueryEdit, &QLineEdit::returnPressed, this, submitTopQuery);

    // 来源卡片折叠/展开
    m_sourcesHeader->installEventFilter(this);
    refreshTheme();
}

// ═══════════════════════════════════════════════
// 公共接口
// ═══════════════════════════════════════════════

void AiKnowledgePanel::refreshTheme()
{
    setStyleSheet(QStringLiteral(
        "#aiKnowledgePanel, #knowledgeBrowserShell, #knowledgeResultShell, #knowledgeResultBody { background:transparent; }"
        "#knowledgeResultToolbar { background:transparent; border-bottom:1px solid %1; }")
        .arg(AppStyle::border()));

    if (m_heroTitle) {
        m_heroTitle->setStyleSheet(
            QStringLiteral("font-size:36px; font-weight:700; color:%1; letter-spacing:1px;")
                .arg(AppStyle::accent()));
    }
    if (m_heroSubtitle) {
        m_heroSubtitle->setStyleSheet(
            QStringLiteral("font-size:13px; color:%1; margin-bottom:20px;")
                .arg(AppStyle::textMuted()));
    }
    if (m_searchBarFrame) {
        m_searchBarFrame->setStyleSheet(searchBarStylesheet());
    }
    applySearchButtonStyle(m_submitButton, 18);
    if (m_topSearchBarFrame) {
        m_topSearchBarFrame->setStyleSheet(topBarStylesheet());
    }
    applySearchButtonStyle(m_topSubmitButton, 16);
    if (m_serviceCombo) {
        m_serviceCombo->setStyleSheet(comboStylesheet());
    }
    if (m_topServiceCombo) {
        m_topServiceCombo->setStyleSheet(comboStylesheet());
    }
    const QString chipStyle = suggestionChipStylesheet();
    for (auto* chip : findChildren<ElaPushButton*>(QStringLiteral("aiSuggestionChip"))) {
        chip->setStyleSheet(chipStyle);
        applySuggestionButtonStyle(chip);
    }
    if (m_answerCard) {
        m_answerCard->setStyleSheet(
            QStringLiteral("QFrame#knowledgeAnswerCard { background:%1; border:1px solid %2; "
                           "border-left:3px solid %3; border-radius:12px; }")
                .arg(AppStyle::isDarkTheme() ? QStringLiteral("rgba(30,34,40,180)") : QStringLiteral("rgba(255,255,255,185)"),
                     AppStyle::border(),
                     AppStyle::accent()));
    }
    if (m_answerBody) {
        m_answerBody->document()->setDefaultStyleSheet(answerDocumentStylesheet());
        m_answerBody->setStyleSheet(
            QStringLiteral("QTextBrowser { font-size:14px; color:%1; padding:4px 0; background:transparent; border:none; }")
                .arg(AppStyle::textPrimary()));
    }
    if (m_sourcesCard) {
        m_sourcesCard->setStyleSheet(cardStylesheet(QStringLiteral("aiSourcesCard")));
    }
    if (m_sourcesHeader) {
        m_sourcesHeader->setStyleSheet(
            QStringLiteral("font-size:14px; font-weight:700; color:%1;")
                .arg(AppStyle::textPrimary()));
    }
    const QString sourceCardStyle = QStringLiteral(
        "QFrame#srcCard { background:%1; border:1px solid %2; border-radius:8px; }"
        "QFrame#srcCard:hover { background:%3; border-color:%4; }")
        .arg(AppStyle::isDarkTheme() ? QStringLiteral("rgba(38,43,50,160)") : QStringLiteral("rgba(255,255,255,150)"),
             AppStyle::border(),
             AppStyle::hoverBg(),
             AppStyle::accent());
    for (auto* card : findChildren<ElaFrame*>(QStringLiteral("srcCard"))) {
        card->setStyleSheet(sourceCardStyle);
    }
}

void AiKnowledgePanel::setAvailableServices(const QStringList& serviceNames, const QStringList& serviceIds, int selectedIndex)
{
    for (QComboBox* combo : {m_serviceCombo, m_topServiceCombo}) {
        const QSignalBlocker blocker(combo);
        combo->clear();
        for (int i = 0; i < serviceNames.size(); ++i) {
            const QString name = serviceNames.at(i).trimmed();
            const QString id = (i < serviceIds.size()) ? serviceIds.at(i) : QString();
            if (name.isEmpty()) {
                continue;
            }
            combo->addItem(name, id);
        }
        if (combo->count() > 0) {
            combo->setCurrentIndex(qBound(0, selectedIndex, combo->count() - 1));
            combo->setEnabled(true);
        } else {
            combo->addItem(QStringLiteral("未配置知识服务"), QString());
            combo->setCurrentIndex(0);
            combo->setEnabled(false);
        }
    }
}

QString AiKnowledgePanel::selectedServiceName() const
{
    return m_serviceCombo ? m_serviceCombo->currentText().trimmed() : QString();
}

QString AiKnowledgePanel::selectedServiceId() const
{
    return m_serviceCombo ? m_serviceCombo->currentData().toString().trimmed() : QString();
}

void AiKnowledgePanel::setQueryPending(bool pending)
{
    m_submitButton->setEnabled(!pending);
    m_topSubmitButton->setEnabled(!pending);
    const bool hasService = !selectedServiceId().isEmpty();
    m_serviceCombo->setEnabled(!pending && hasService);
    m_topServiceCombo->setEnabled(!pending && hasService);

    if (pending) {
        m_pendingQueryServiceId = selectedServiceId();
        switchToResultMode();
        m_answerBody->setPlainText(QStringLiteral("正在从知识库获取回答..."));
        clearSourcesGrid();
        return;
    }

    m_pendingQueryServiceId.clear();
}

void AiKnowledgePanel::showQueryResponse(const KnowServiceQueryResponse& response)
{
    m_submitButton->setEnabled(true);
    m_topSubmitButton->setEnabled(true);
    const bool hasService = !selectedServiceId().isEmpty();
    m_serviceCombo->setEnabled(hasService);
    m_topServiceCombo->setEnabled(hasService);

    switchToResultMode();

    // AI 回答 → 富文本渲染
    const QString summary = response.answer.summary.trimmed();
    m_answerBody->setHtml(MarkdownRenderer::renderMarkdownToHtml(
        summary,
        {.emptyPlaceholder = QStringLiteral("知识库已返回结果，但未生成摘要。"),
         .highlightCitations = true}));

    clearSourcesGrid();
    if (response.sources.isEmpty()) {
        m_sourcesCard->hide();
        return;
    }

    // 计算平均分
    double totalScore = 0.0;
    for (const KnowServiceSource& s : response.sources) {
        totalScore += s.score;
    }
    const int avgPercent = qBound(0, static_cast<int>(totalScore / response.sources.size() * 100), 100);
    m_sourcesHeader->setText(
        QStringLiteral("\u25BC  \u547D\u4E2D\u6765\u6E90 (%1\u6761, \u5E73\u5747\u5339\u914D %2%)")
            .arg(response.sources.size())
            .arg(avgPercent));

    m_sourcesCard->show();
    m_sourcesGrid->show();
    m_sourcesExpanded = true;

    auto* grid = new QGridLayout(m_sourcesGrid);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);
    const int columns = 3;

    const QString cardStyle = QStringLiteral(
        "QFrame#srcCard { background:%1; border:1px solid %2; border-radius:8px; }"
        "QFrame#srcCard:hover { background:%3; border-color:%4; }")
        .arg(AppStyle::surfaceAlt(), AppStyle::border(),
             AppStyle::hoverBg(), AppStyle::accent());

    for (int i = 0; i < response.sources.size(); ++i) {
        const KnowServiceSource& source = response.sources.at(i);
        const int row = i / columns;
        const int col = i % columns;

        auto* card = new ElaFrame(m_sourcesGrid);
        card->setObjectName(QStringLiteral("srcCard"));
        card->setStyleSheet(cardStyle);
        card->setCursor(Qt::PointingHandCursor);
        card->setFixedHeight(64);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        cardLayout->setSpacing(4);

        // 序号 + 标题
        auto* titleRow = new QHBoxLayout;
        titleRow->setSpacing(6);
        auto* indexLabel = new ElaText(QStringLiteral("%1").arg(i + 1), card);
        indexLabel->setFixedSize(20, 20);
        indexLabel->setAlignment(Qt::AlignCenter);
        indexLabel->setStyleSheet(
            QStringLiteral("background:%1; color:#FFFFFF; border-radius:10px; font-size:11px; font-weight:700;")
                .arg(AppStyle::accent()));
        auto* titleLabel = new ElaText(card);
        titleLabel->setText(aiSourceDisplayText(source));
        titleLabel->setStyleSheet(
            QStringLiteral("font-size:12px; font-weight:600; color:%1; background:transparent;")
                .arg(AppStyle::textPrimary()));
        titleLabel->setWordWrap(false);
        titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        titleRow->addWidget(indexLabel);
        titleRow->addWidget(titleLabel, 1);
        cardLayout->addLayout(titleRow);

        // 分数行
        const int scorePercent = qBound(0, static_cast<int>(source.score * 100), 100);
        auto* scoreLabel = new ElaText(
            QStringLiteral("\u5339\u914D %1%").arg(scorePercent), card);
        scoreLabel->setStyleSheet(
            QStringLiteral("font-size:11px; color:%1; background:transparent;")
                .arg(AppStyle::textMuted()));
        cardLayout->addWidget(scoreLabel);

        // 点击打开来源
        const QString openUrl = aiSourceOpenUrl(source);
        if (!openUrl.isEmpty()) {
            connect(card, &QFrame::objectNameChanged, this, [](const QString&) {});
            card->installEventFilter(this);
            card->setProperty("sourceUrl", openUrl);
        }

        grid->addWidget(card, row, col);
    }
    // 填充末行空列
    const int remainder = response.sources.size() % columns;
    if (remainder != 0) {
        for (int c = remainder; c < columns; ++c) {
            grid->addWidget(new QWidget(m_sourcesGrid), (response.sources.size() - 1) / columns, c);
        }
    }
}

void AiKnowledgePanel::showQueryError(const QString& message)
{
    m_submitButton->setEnabled(true);
    m_topSubmitButton->setEnabled(true);
    const bool hasService = !selectedServiceId().isEmpty();
    m_serviceCombo->setEnabled(hasService);
    m_topServiceCombo->setEnabled(hasService);

    switchToResultMode();

    m_answerBody->setPlainText(message.trimmed().isEmpty()
                                   ? QStringLiteral("查询失败，请稍后重试。")
                                   : message.trimmed());
    clearSourcesGrid();
    m_sourcesCard->hide();
}

// ═══════════════════════════════════════════════
// 私有
// ═══════════════════════════════════════════════

void AiKnowledgePanel::switchToSearchMode()
{
    if (auto* stack = qobject_cast<QStackedLayout*>(layout())) {
        stack->setCurrentWidget(m_heroContainer);
    }
}

void AiKnowledgePanel::switchToResultMode()
{
    if (auto* stack = qobject_cast<QStackedLayout*>(layout())) {
        stack->setCurrentWidget(m_resultContainer);
    }
}

void AiKnowledgePanel::clearSourcesGrid()
{
    if (!m_sourcesGrid) {
        return;
    }
    delete m_sourcesGrid->layout();
    QList<QWidget*> children = m_sourcesGrid->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* child : children) {
        delete child;
    }
}

bool AiKnowledgePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_sourcesHeader && event && event->type() == QEvent::MouseButtonRelease) {
        m_sourcesExpanded = !m_sourcesExpanded;
        m_sourcesGrid->setVisible(m_sourcesExpanded);
        QString text = m_sourcesHeader->text();
        if (m_sourcesExpanded) {
            text.replace(QStringLiteral("\u25B6"), QStringLiteral("\u25BC"));
        } else {
            text.replace(QStringLiteral("\u25BC"), QStringLiteral("\u25B6"));
        }
        m_sourcesHeader->setText(text);
        return true;
    }
    // 来源卡片点击
    if (event && event->type() == QEvent::MouseButtonRelease) {
        const QString url = watched->property("sourceUrl").toString();
        if (!url.isEmpty()) {
            const QUrl parsed(url);
            if (parsed.isValid()) {
                QDesktopServices::openUrl(parsed);
            }
            emit sourceOpenRequested(url);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
