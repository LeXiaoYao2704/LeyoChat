#include "ui/ChatComposerWidget.h"

#include "ui/AppStyle.h"
#include "ui/StickerManager.h"
#include "ui/UiIcons.h"

#include <QComboBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFont>
#include <QFontComboBox>
#include <QFontMetrics>
#include <QFrame>
#include <ElaFrame.h>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPointer>
#include <QSettings>
#include <QSignalBlocker>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextEdit>
#include <ElaTextEdit.h>
#include <QTimer>
#include <QToolButton>
#include <QStackedLayout>
#include <QVBoxLayout>

#include <ElaComboBox.h>
#include <ElaMenu.h>
#include <ElaPushButton.h>
#include <ElaScrollArea.h>
#include <ElaText.h>
#include <ElaTheme.h>
#include <ElaToolButton.h>

namespace {

struct EmojiCategory {
    QString label;
    QString icon;
    QStringList emojis;
};

QVector<EmojiCategory> emojiCategories()
{
    return {
        { QStringLiteral("😀 表情"),
          QStringLiteral("😀"),
          { QStringLiteral("😀"), QStringLiteral("😁"), QStringLiteral("😂"), QStringLiteral("🤣"),
            QStringLiteral("😊"), QStringLiteral("😍"), QStringLiteral("🥰"), QStringLiteral("😘"),
            QStringLiteral("😗"), QStringLiteral("😙"), QStringLiteral("😚"), QStringLiteral("😋"),
            QStringLiteral("😛"), QStringLiteral("😜"), QStringLiteral("🤪"), QStringLiteral("😝"),
            QStringLiteral("🤑"), QStringLiteral("🤗"), QStringLiteral("🤭"), QStringLiteral("🤫"),
            QStringLiteral("🤔"), QStringLiteral("🤐"), QStringLiteral("🤨"), QStringLiteral("😐"),
            QStringLiteral("😑"), QStringLiteral("😶"), QStringLiteral("😏"), QStringLiteral("😒"),
            QStringLiteral("🙄"), QStringLiteral("😬"), QStringLiteral("🤥"), QStringLiteral("😌"),
            QStringLiteral("😔"), QStringLiteral("😪"), QStringLiteral("🤤"), QStringLiteral("😴"),
            QStringLiteral("😷"), QStringLiteral("🤒"), QStringLiteral("🤕"), QStringLiteral("🤢"),
            QStringLiteral("🤮"), QStringLiteral("🥴"), QStringLiteral("😵"), QStringLiteral("🤯"),
            QStringLiteral("🥳"), QStringLiteral("😎"), QStringLiteral("🤓"), QStringLiteral("🧐"),
            QStringLiteral("😕"), QStringLiteral("😟"), QStringLiteral("🙁"), QStringLiteral("😮"),
            QStringLiteral("😯"), QStringLiteral("😲"), QStringLiteral("😳"), QStringLiteral("🥺"),
            QStringLiteral("😦"), QStringLiteral("😧"), QStringLiteral("😨"), QStringLiteral("😰"),
            QStringLiteral("😥"), QStringLiteral("😢"), QStringLiteral("😭"), QStringLiteral("😱"),
            QStringLiteral("😖"), QStringLiteral("😣"), QStringLiteral("😞"), QStringLiteral("😓"),
            QStringLiteral("😩"), QStringLiteral("😫"), QStringLiteral("🥱"), QStringLiteral("😤"),
            QStringLiteral("😡"), QStringLiteral("😠"), QStringLiteral("🤬"), QStringLiteral("😈"),
            QStringLiteral("👿"), QStringLiteral("💀"), QStringLiteral("💩"), QStringLiteral("🤡"),
            QStringLiteral("👹"), QStringLiteral("😺"), QStringLiteral("😸"), QStringLiteral("😹"),
            QStringLiteral("😻"), QStringLiteral("😼"), QStringLiteral("😽"), QStringLiteral("🙀"),
            QStringLiteral("😿"), QStringLiteral("😾"), QStringLiteral("🥲"), QStringLiteral("😇") }
        },
        { QStringLiteral("👋 手势"),
          QStringLiteral("👋"),
          { QStringLiteral("👋"), QStringLiteral("🤚"), QStringLiteral("🖐️"), QStringLiteral("✋"),
            QStringLiteral("🖖"), QStringLiteral("👌"), QStringLiteral("🤌"), QStringLiteral("🤏"),
            QStringLiteral("✌️"), QStringLiteral("🤞"), QStringLiteral("🤟"), QStringLiteral("🤘"),
            QStringLiteral("🤙"), QStringLiteral("👈"), QStringLiteral("👉"), QStringLiteral("👆"),
            QStringLiteral("🖕"), QStringLiteral("👇"), QStringLiteral("☝️"), QStringLiteral("👍"),
            QStringLiteral("👎"), QStringLiteral("✊"), QStringLiteral("👊"), QStringLiteral("🤛"),
            QStringLiteral("🤜"), QStringLiteral("👏"), QStringLiteral("🙌"), QStringLiteral("👐"),
            QStringLiteral("🤲"), QStringLiteral("🤝"), QStringLiteral("🙏"), QStringLiteral("💪"),
            QStringLiteral("👀"), QStringLiteral("👁️"), QStringLiteral("👅"), QStringLiteral("👄") }
        },
        { QStringLiteral("❤️ 心/符号"),
          QStringLiteral("❤️"),
          { QStringLiteral("❤️"), QStringLiteral("🧡"), QStringLiteral("💛"), QStringLiteral("💚"),
            QStringLiteral("💙"), QStringLiteral("💜"), QStringLiteral("🖤"), QStringLiteral("🤍"),
            QStringLiteral("🤎"), QStringLiteral("💔"), QStringLiteral("❣️"), QStringLiteral("💕"),
            QStringLiteral("💞"), QStringLiteral("💓"), QStringLiteral("💗"), QStringLiteral("💖"),
            QStringLiteral("💘"), QStringLiteral("💝"), QStringLiteral("💟"), QStringLiteral("💯"),
            QStringLiteral("💢"), QStringLiteral("💥"), QStringLiteral("💫"), QStringLiteral("💦"),
            QStringLiteral("✅"), QStringLiteral("❌"), QStringLiteral("⚠️"), QStringLiteral("🔥"),
            QStringLiteral("⭐"), QStringLiteral("🌟"), QStringLiteral("✨"), QStringLiteral("⚡") }
        },
        { QStringLiteral("🐱 动物"),
          QStringLiteral("🐱"),
          { QStringLiteral("🐶"), QStringLiteral("🐱"), QStringLiteral("🐭"), QStringLiteral("🐹"),
            QStringLiteral("🐰"), QStringLiteral("🦊"), QStringLiteral("🐻"), QStringLiteral("🐼"),
            QStringLiteral("🐨"), QStringLiteral("🐯"), QStringLiteral("🦁"), QStringLiteral("🐮"),
            QStringLiteral("🐷"), QStringLiteral("🐸"), QStringLiteral("🐵"), QStringLiteral("🙈"),
            QStringLiteral("🙉"), QStringLiteral("🙊"), QStringLiteral("🐔"), QStringLiteral("🐧"),
            QStringLiteral("🐦"), QStringLiteral("🐤"), QStringLiteral("🦄"), QStringLiteral("🐝"),
            QStringLiteral("🐛"), QStringLiteral("🦋"), QStringLiteral("🐌"), QStringLiteral("🐙"),
            QStringLiteral("🦀"), QStringLiteral("🐠"), QStringLiteral("🐬"), QStringLiteral("🐳") }
        },
        { QStringLiteral("🍕 食物"),
          QStringLiteral("🍕"),
          { QStringLiteral("🍎"), QStringLiteral("🍊"), QStringLiteral("🍋"), QStringLiteral("🍌"),
            QStringLiteral("🍉"), QStringLiteral("🍇"), QStringLiteral("🍓"), QStringLiteral("🍒"),
            QStringLiteral("🍑"), QStringLiteral("🥝"), QStringLiteral("🍅"), QStringLiteral("🥑"),
            QStringLiteral("🍔"), QStringLiteral("🍟"), QStringLiteral("🍕"), QStringLiteral("🌭"),
            QStringLiteral("🍿"), QStringLiteral("🍩"), QStringLiteral("🍪"), QStringLiteral("🎂"),
            QStringLiteral("🍰"), QStringLiteral("🧁"), QStringLiteral("🍫"), QStringLiteral("🍬"),
            QStringLiteral("☕"), QStringLiteral("🍵"), QStringLiteral("🍺"), QStringLiteral("🍻"),
            QStringLiteral("🥂"), QStringLiteral("🍷"), QStringLiteral("🧃"), QStringLiteral("🥤") }
        },
        { QStringLiteral("🚀 物品"),
          QStringLiteral("🚀"),
          { QStringLiteral("🚀"), QStringLiteral("🎉"), QStringLiteral("🎊"), QStringLiteral("🎈"),
            QStringLiteral("🎁"), QStringLiteral("🏆"), QStringLiteral("🥇"), QStringLiteral("🥈"),
            QStringLiteral("📌"), QStringLiteral("📎"), QStringLiteral("💡"), QStringLiteral("🔔"),
            QStringLiteral("⌛"), QStringLiteral("⏰"), QStringLiteral("📞"), QStringLiteral("💬"),
            QStringLiteral("🧠"), QStringLiteral("📣"), QStringLiteral("📝"), QStringLiteral("📁"),
            QStringLiteral("📂"), QStringLiteral("📊"), QStringLiteral("📈"), QStringLiteral("🔑"),
            QStringLiteral("🔒"), QStringLiteral("🔓"), QStringLiteral("🛠️"), QStringLiteral("⚙️"),
            QStringLiteral("🌈"), QStringLiteral("☀️"), QStringLiteral("🌙"), QStringLiteral("⛅") }
        },
    };
}

QStringList allEmojiChoices()
{
    QStringList all;
    for (const auto& cat : emojiCategories())
        all.append(cat.emojis);
    return all;
}

// ── 最近使用表情持久化 ──
constexpr int kMaxRecentEmojis = 32;
const auto kRecentEmojisKey = QStringLiteral("Chat/RecentEmojis");

QStringList loadRecentEmojis()
{
    return QSettings().value(kRecentEmojisKey).toStringList();
}

void saveRecentEmoji(const QString& emoji)
{
    QStringList recent = loadRecentEmojis();
    recent.removeAll(emoji);
    recent.prepend(emoji);
    if (recent.size() > kMaxRecentEmojis)
        recent = recent.mid(0, kMaxRecentEmojis);
    QSettings().setValue(kRecentEmojisKey, recent);
}

QStringList curatedFontFamilies()
{
    return {
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Segoe UI"),
        QStringLiteral("HarmonyOS Sans SC"),
        QStringLiteral("Noto Sans SC"),
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("Consolas")
    };
}

QList<int> curatedFontSizes()
{
    return {10, 11, 12, 13, 14, 16, 18, 20, 24, 28, 32};
}

void mergeCurrentFormat(ElaTextEdit* editor, const QTextCharFormat& format)
{
    if (!editor) {
        return;
    }

    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
        cursor.select(QTextCursor::WordUnderCursor);
    }
    cursor.mergeCharFormat(format);
    editor->mergeCurrentCharFormat(format);
    editor->setTextCursor(cursor);
    editor->setFocus();
}

QTextCharFormat currentFormatOrDefault(ElaTextEdit* editor)
{
    if (!editor) {
        return QTextCharFormat{};
    }
    return editor->currentCharFormat();
}

} // namespace

ChatComposerWidget::ChatComposerWidget(QWidget* parent)
    : QWidget(parent)
{
    const QFont baseFont = font();
    const int toolbarIconSize = qMax(28, AppStyle::iconButtonSizeForFont(baseFont) - 8);
    const int actionHeight = qMax(32, QFontMetrics(AppStyle::bodyFont(baseFont)).height() + 8);
    const int editorMinHeight = qMax(82, QFontMetrics(baseFont).height() * 3 + 28);

    setObjectName(QStringLiteral("chatComposerWidget"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 8, 18, 12);
    root->setSpacing(0);

    auto* composerSurface = new ElaFrame(this);
    composerSurface->setObjectName(QStringLiteral("composerSurface"));
    composerSurface->setFrameShape(QFrame::NoFrame);
    composerSurface->setAttribute(Qt::WA_StyledBackground, true);
    auto* surfaceLayout = new QVBoxLayout(composerSurface);
    surfaceLayout->setContentsMargins(16, 12, 16, 12);
    surfaceLayout->setSpacing(6);

    auto* controlBand = new ElaFrame(composerSurface);
    m_controlBand = controlBand;
    controlBand->setObjectName(QStringLiteral("composerControlBand"));
    controlBand->setFrameShape(QFrame::NoFrame);
    controlBand->hide();
    auto* controlBandLayout = new QHBoxLayout(controlBand);
    controlBandLayout->setContentsMargins(10, 5, 10, 5);
    controlBandLayout->setSpacing(6);

    auto* modeChip = new ElaText(QStringLiteral("当前草稿"), controlBand);
    modeChip->setObjectName(QStringLiteral("composerModeChip"));
    m_metaChipLabel = new ElaText(QStringLiteral("截图 / 文件 / 待发"), controlBand);
    m_metaChipLabel->setObjectName(QStringLiteral("composerMetaChip"));
    auto* hintLabel = new ElaText(QStringLiteral("编辑区"), controlBand);
    hintLabel->setObjectName(QStringLiteral("composerHintLabel"));
    auto* editCloseButton = new ElaToolButton(controlBand);
    editCloseButton->setObjectName(QStringLiteral("composerEditCloseButton"));
    editCloseButton->setIsTransparent(true);
    editCloseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    editCloseButton->setElaIcon(ElaIconType::Xmark);
    editCloseButton->setToolTip(QStringLiteral("\u53D6\u6D88\u7F16\u8F91"));
    editCloseButton->setFixedSize(20, 20);
    editCloseButton->setCursor(Qt::PointingHandCursor);
    editCloseButton->hide();

    controlBandLayout->addWidget(modeChip);
    controlBandLayout->addWidget(m_metaChipLabel);
    controlBandLayout->addStretch();
    controlBandLayout->addWidget(hintLabel);
    controlBandLayout->addWidget(editCloseButton);
    surfaceLayout->addWidget(controlBand);

    m_toolbarHost = new QWidget(composerSurface);
    auto* toolbarLayout = new QHBoxLayout(m_toolbarHost);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(6);

    m_boldButton = new ElaToolButton(m_toolbarHost);
    m_boldButton->setIsTransparent(true);
    m_boldButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_boldButton->setText(QStringLiteral("B"));
    m_boldButton->setObjectName(QStringLiteral("composerBoldButton"));
    m_boldButton->setToolTip(QStringLiteral("加粗"));
    m_boldButton->setCheckable(true);
    m_boldButton->setFixedSize(toolbarIconSize + 6, toolbarIconSize + 6);

    m_italicButton = new ElaToolButton(m_toolbarHost);
    m_italicButton->setIsTransparent(true);
    m_italicButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_italicButton->setText(QStringLiteral("I"));
    m_italicButton->setObjectName(QStringLiteral("composerItalicButton"));
    m_italicButton->setToolTip(QStringLiteral("斜体"));
    QFont italicFont = m_italicButton->font();
    italicFont.setItalic(true);
    m_italicButton->setFont(italicFont);
    m_italicButton->setCheckable(true);
    m_italicButton->setFixedSize(toolbarIconSize + 6, toolbarIconSize + 6);

    m_underlineButton = new ElaToolButton(m_toolbarHost);
    m_underlineButton->setIsTransparent(true);
    m_underlineButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_underlineButton->setText(QStringLiteral("U"));
    m_underlineButton->setObjectName(QStringLiteral("composerUnderlineButton"));
    m_underlineButton->setToolTip(QStringLiteral("下划线"));
    QFont underlineFont = m_underlineButton->font();
    underlineFont.setUnderline(true);
    m_underlineButton->setFont(underlineFont);
    m_underlineButton->setCheckable(true);
    m_underlineButton->setFixedSize(toolbarIconSize + 6, toolbarIconSize + 6);

    m_fontButton = new ElaToolButton(m_toolbarHost);
    m_fontButton->setIsTransparent(true);
    m_fontButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_fontButton->setText(QStringLiteral("Aa"));
    m_fontButton->setObjectName(QStringLiteral("composerFontButton"));
    m_fontButton->setToolTip(QStringLiteral("字体与字号"));
    m_fontButton->setFixedSize(toolbarIconSize + 10, toolbarIconSize + 6);

    m_emojiButton = new ElaToolButton(m_toolbarHost);
    m_emojiButton->setIsTransparent(true);
    m_emojiButton->setElaIcon(ElaIconType::FaceSmile);
    m_emojiButton->setObjectName(QStringLiteral("composerEmojiButton"));
    m_emojiButton->setToolTip(QStringLiteral("插入表情"));
    m_emojiButton->setFixedSize(toolbarIconSize + 6, toolbarIconSize + 6);

    m_fileButton = new ElaToolButton(m_toolbarHost);
    m_fileButton->setIsTransparent(true);
    m_fileButton->setElaIcon(ElaIconType::Paperclip);
    m_fileButton->setToolTip(QStringLiteral("发送文件"));
    m_fileButton->setFixedSize(toolbarIconSize + 6, toolbarIconSize + 6);

    m_screenshotButton = new ElaToolButton(m_toolbarHost);
    m_screenshotButton->setIsTransparent(true);
    m_screenshotButton->setElaIcon(ElaIconType::Scissors);
    m_screenshotButton->setToolTip(QStringLiteral("截图"));
    m_screenshotButton->setFixedSize(toolbarIconSize + 6, toolbarIconSize + 6);

    m_nudgeButton = new ElaToolButton(m_toolbarHost);
    m_nudgeButton->setIsTransparent(true);
    m_nudgeButton->setElaIcon(ElaIconType::BellRing);
    m_nudgeButton->setObjectName(QStringLiteral("composerNudgeButton"));
    m_nudgeButton->setToolTip(QStringLiteral("窗口抖动提醒"));
    m_nudgeButton->setFixedSize(toolbarIconSize + 6, toolbarIconSize + 6);

    m_devOpsButton = new ElaToolButton(m_toolbarHost);
    m_devOpsButton->setIsTransparent(true);
    m_devOpsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_devOpsButton->setText(QStringLiteral("ADO"));
    m_devOpsButton->hide();
    m_devOpsButton->setToolTip(QStringLiteral("插入 Azure DevOps 卡片"));
    m_devOpsButton->setFixedSize(toolbarIconSize + 18, toolbarIconSize + 6);

    for (auto* btn : {m_boldButton, m_italicButton, m_underlineButton,
                      m_fontButton, m_emojiButton, m_fileButton,
                      m_screenshotButton, m_nudgeButton, m_devOpsButton}) {
        btn->setCursor(Qt::PointingHandCursor);
    }

    toolbarLayout->addWidget(m_boldButton);
    toolbarLayout->addWidget(m_italicButton);
    toolbarLayout->addWidget(m_underlineButton);
    toolbarLayout->addSpacing(2);
    toolbarLayout->addWidget(m_fontButton);
    toolbarLayout->addWidget(m_emojiButton);
    toolbarLayout->addSpacing(6);
    toolbarLayout->addWidget(m_fileButton);
    toolbarLayout->addWidget(m_screenshotButton);
    toolbarLayout->addWidget(m_nudgeButton);
    toolbarLayout->addStretch();

    m_messageEditor = new ElaTextEdit(composerSurface);
    m_messageEditor->setObjectName(QStringLiteral("composerMessageEditor"));
    m_messageEditor->setAcceptRichText(true);
    m_messageEditor->setMinimumHeight(editorMinHeight);
    m_messageEditor->setPlaceholderText(QStringLiteral("输入消息，Enter 发送，Shift+Enter 换行"));

    // 右键菜单中文化
    m_messageEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_messageEditor, &QTextEdit::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* menu = new ElaMenu(m_messageEditor);
        menu->addAction(QStringLiteral("\u64A4\u9500"), m_messageEditor, &QTextEdit::undo)->setEnabled(m_messageEditor->document()->isUndoAvailable());
        menu->addAction(QStringLiteral("\u91CD\u505A"), m_messageEditor, &QTextEdit::redo)->setEnabled(m_messageEditor->document()->isRedoAvailable());
        menu->addSeparator();
        menu->addAction(QStringLiteral("\u526A\u5207"), m_messageEditor, &QTextEdit::cut)->setEnabled(m_messageEditor->textCursor().hasSelection());
        menu->addAction(QStringLiteral("\u590D\u5236"), m_messageEditor, &QTextEdit::copy)->setEnabled(m_messageEditor->textCursor().hasSelection());
        menu->addAction(QStringLiteral("\u7C98\u8D34"), m_messageEditor, &QTextEdit::paste)->setEnabled(m_messageEditor->canPaste());
        menu->addSeparator();
        auto* selectAllAction = menu->addAction(QStringLiteral("\u5168\u9009"), m_messageEditor, &QTextEdit::selectAll);
        selectAllAction->setEnabled(!m_messageEditor->document()->isEmpty());
        menu->exec(m_messageEditor->mapToGlobal(pos));
        delete menu;
    });

    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(0, 6, 0, 0);
    footer->setSpacing(8);

    auto* shortcutHint = new ElaText(QStringLiteral("支持拖拽文件发送"), composerSurface);
    shortcutHint->setObjectName(QStringLiteral("composerShortcutHint"));
    footer->addWidget(shortcutHint);
    footer->addStretch();

    // 输入框有内容时隐藏拖拽提示
    connect(m_messageEditor, &QTextEdit::textChanged, shortcutHint, [this, shortcutHint]() {
        shortcutHint->setVisible(m_messageEditor->toPlainText().trimmed().isEmpty());
    });

    m_sendButton = new ElaPushButton(QStringLiteral("发送"), composerSurface);
    m_sendButton->setBorderRadius(10);
    m_sendButton->setLightDefaultColor(QColor(AppStyle::accent()));
    m_sendButton->setDarkDefaultColor(QColor(AppStyle::accent()));
    m_sendButton->setLightHoverColor(QColor(AppStyle::accentHover()));
    m_sendButton->setDarkHoverColor(QColor(AppStyle::accentHover()));
    m_sendButton->setLightPressColor(QColor(AppStyle::accentPressed()));
    m_sendButton->setDarkPressColor(QColor(AppStyle::accentPressed()));
    m_sendButton->setLightTextColor(Qt::white);
    m_sendButton->setDarkTextColor(Qt::white);
    m_sendButton->setFixedSize(
        qMax(82, QFontMetrics(AppStyle::strongFont(baseFont)).horizontalAdvance(QStringLiteral("发送")) + 36),
        actionHeight);

    m_sendModeButton = new ElaPushButton(composerSurface);
    m_sendModeButton->setText(QStringLiteral("Enter \u53d1\u9001 \u25be"));
    m_sendModeButton->setObjectName(QStringLiteral("composerSendModeButton"));
    m_sendModeButton->setFixedSize(104, actionHeight);
    m_sendModeButton->setToolTip(QStringLiteral("发送方式"));
    m_sendModeButton->setFocusPolicy(Qt::NoFocus);

    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendModeButton->setCursor(Qt::PointingHandCursor);

    footer->addWidget(m_sendButton);
    footer->addWidget(m_sendModeButton);

    // Reply preview bar (hidden by default)
    m_replyPreviewBar = new QWidget(composerSurface);
    m_replyPreviewBar->setObjectName(QStringLiteral("composerReplyPreviewBar"));
    m_replyPreviewBar->hide();
    auto* replyBarLayout = new QHBoxLayout(m_replyPreviewBar);
    replyBarLayout->setContentsMargins(10, 5, 10, 5);
    replyBarLayout->setSpacing(6);

    auto* replyIndicator = new ElaFrame(m_replyPreviewBar);
    replyIndicator->setFixedWidth(3);
    replyIndicator->setFrameShape(QFrame::NoFrame);
    replyIndicator->setObjectName(QStringLiteral("composerReplyIndicator"));
    replyBarLayout->addWidget(replyIndicator);

    m_replyPreviewLabel = new ElaText(m_replyPreviewBar);
    m_replyPreviewLabel->setObjectName(QStringLiteral("composerReplyPreviewLabel"));
    m_replyPreviewLabel->setWordWrap(false);
    replyBarLayout->addWidget(m_replyPreviewLabel, 1);

    auto* replyCloseButton = new ElaToolButton(m_replyPreviewBar);
    replyCloseButton->setIsTransparent(true);
    replyCloseButton->setFixedSize(20, 20);
    replyCloseButton->setElaIcon(ElaIconType::Xmark);
    replyCloseButton->setObjectName(QStringLiteral("composerReplyCloseButton"));
    replyBarLayout->addWidget(replyCloseButton);
    connect(replyCloseButton, &QAbstractButton::clicked, this, &ChatComposerWidget::clearReplyContext);
    connect(editCloseButton, &QAbstractButton::clicked, this, [this]() { exitEditMode(); });

    surfaceLayout->addWidget(m_toolbarHost);
    surfaceLayout->addWidget(m_replyPreviewBar);
    surfaceLayout->addWidget(m_messageEditor, 1);
    surfaceLayout->addLayout(footer);
    root->addWidget(composerSurface);

    for (auto* button : {m_fileButton, m_screenshotButton}) {
        button->setFixedHeight(qMax(actionHeight, button->height()));
    }

    connect(m_sendButton, &QAbstractButton::clicked, this, &ChatComposerWidget::sendTriggered);
    connect(m_fileButton, &QAbstractButton::clicked, this, &ChatComposerWidget::fileTriggered);
    connect(m_nudgeButton, &QAbstractButton::clicked, this, &ChatComposerWidget::nudgeTriggered);
    connect(m_devOpsButton, &QAbstractButton::clicked, this, &ChatComposerWidget::devOpsTriggered);

    // 正在输入指示器：每3秒最多发一次信号
    auto* typingThrottle = new QTimer(this);
    typingThrottle->setSingleShot(true);
    connect(m_messageEditor, &QTextEdit::textChanged, this, [this, typingThrottle]() {
        emit recoveryContextChanged();
        if (m_messageEditor->toPlainText().isEmpty()) {
            emit recoveryContextCommitted();
        }
        if (!typingThrottle->isActive() && !m_messageEditor->toPlainText().trimmed().isEmpty()) {
            emit typingActivity();
            typingThrottle->start(3000);
        }
    });

    const auto syncToggleButtons = [this]() {
        if (!m_messageEditor) {
            return;
        }
        const QTextCharFormat fmt = m_messageEditor->currentCharFormat();
        {
            QSignalBlocker blocker(m_boldButton);
            m_boldButton->setChecked(fmt.fontWeight() >= QFont::Bold);
        }
        {
            QSignalBlocker blocker(m_italicButton);
            m_italicButton->setChecked(fmt.fontItalic());
        }
        {
            QSignalBlocker blocker(m_underlineButton);
            m_underlineButton->setChecked(fmt.fontUnderline());
        }
    };

    connect(m_messageEditor, &QTextEdit::currentCharFormatChanged, this,
            [syncToggleButtons](const QTextCharFormat&) { syncToggleButtons(); });
    connect(m_messageEditor, &QTextEdit::cursorPositionChanged, this, syncToggleButtons);

    connect(m_boldButton, &QAbstractButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontWeight(m_boldButton->isChecked() ? QFont::Bold : QFont::Normal);
        mergeCurrentFormat(m_messageEditor, format);
    });
    connect(m_italicButton, &QAbstractButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontItalic(m_italicButton->isChecked());
        mergeCurrentFormat(m_messageEditor, format);
    });
    connect(m_underlineButton, &QAbstractButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontUnderline(m_underlineButton->isChecked());
        mergeCurrentFormat(m_messageEditor, format);
    });
    connect(m_fontButton, &QAbstractButton::clicked, this, [this]() {
        auto* popup = new QFrame(this);
        popup->setObjectName(QStringLiteral("fontPopupPanel"));
        popup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->setAttribute(Qt::WA_StyledBackground);
        popup->setStyleSheet(QStringLiteral(
            "QFrame#fontPopupPanel {"
            "  background:%1;"
            "  border:1px solid %2;"
            "  border-radius:10px;"
            "}"
            "QFrame#fontPopupPanel QWidget {"
            "  background:transparent;"
            "}").arg(AppStyle::chatCardBg(), AppStyle::border()));

        auto* panelLayout = new QHBoxLayout(popup);
        panelLayout->setContentsMargins(12, 10, 12, 10);
        panelLayout->setSpacing(8);

        auto* familyCombo = new QFontComboBox(popup);
        familyCombo->setObjectName(QStringLiteral("composerFontFamilyCombo"));
        familyCombo->setMinimumWidth(200);
        familyCombo->setCurrentFont(m_messageEditor->currentFont());

        auto* sizeCombo = new ElaComboBox(popup);
        sizeCombo->setObjectName(QStringLiteral("composerFontSizeCombo"));
        sizeCombo->setMinimumWidth(84);
        sizeCombo->setEditable(true);
        for (const int size : curatedFontSizes()) {
            sizeCombo->addItem(QString::number(size));
        }
        const QTextCharFormat currentFormat = currentFormatOrDefault(m_messageEditor);
        const qreal currentSize = currentFormat.fontPointSize() > 0.0 ? currentFormat.fontPointSize() : 11.0;
        sizeCombo->setCurrentText(QString::number(qRound(currentSize)));

        panelLayout->addWidget(familyCombo, 1);
        panelLayout->addWidget(sizeCombo);

        connect(familyCombo, &QFontComboBox::currentFontChanged, popup, [this](const QFont& font) {
            QTextCharFormat format;
            format.setFontFamilies({font.family()});
            mergeCurrentFormat(m_messageEditor, format);
        });
        connect(sizeCombo, &QComboBox::textActivated, popup, [this](const QString& text) {
            bool ok = false;
            const int size = text.toInt(&ok);
            if (!ok) {
                return;
            }
            QTextCharFormat format;
            format.setFontPointSize(qBound(9, size, 48));
            mergeCurrentFormat(m_messageEditor, format);
        });

        popup->adjustSize();
        popup->move(m_fontButton->mapToGlobal(QPoint(0, m_fontButton->height() + 4)));
        popup->show();
    });
    connect(m_emojiButton, &QAbstractButton::clicked, this, [this]() {
        showEmojiPanel();
    });

    syncToggleButtons();
    refreshTheme();

    connect(eTheme, &ElaTheme::themeModeChanged, this, [this]() { refreshTheme(); });
}

void ChatComposerWidget::showEmojiPanel()
{
    ensureEmojiPanel();
    m_emojiPanel->adjustSize();
    m_emojiPanel->move(m_emojiButton->mapToGlobal(QPoint(0, -m_emojiPanel->sizeHint().height() - 4)));
    m_emojiPanel->show();
    m_emojiPanel->raise();
    m_emojiPanel->activateWindow();
}

void ChatComposerWidget::ensureEmojiPanel()
{
    if (m_emojiPanel)
        return;

    auto* panel = new ElaFrame(this);
    m_emojiPanel = panel;
    panel->setObjectName(QStringLiteral("emojiPopupPanel"));
    panel->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setStyleSheet(QStringLiteral(
        "QFrame#emojiPopupPanel {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:10px;"
        "}"
        "QFrame#emojiPopupPanel QWidget {"
        "  background:transparent;"
        "}").arg(AppStyle::chatCardBg(), AppStyle::border()));
    QPointer<QWidget> popupGuard(panel);

    auto closePopupLater = [popupGuard]() {
        if (popupGuard)
            popupGuard->hide();
    };

    auto sendStickerAfterMenuClose = [this, closePopupLater](const QString& packId,
                                                       const QString& stickerId) {
        QPointer<ChatComposerWidget> self(this);
        closePopupLater();
        QTimer::singleShot(0, this, [self, packId, stickerId]() {
            if (self)
                emit self->stickerSelected(packId, stickerId);
        });
    };

    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(6, 6, 6, 6);
    panelLayout->setSpacing(4);

    // ─── 顶层切换栏: 表情 | 贴纸 ───
    auto* modeBar = new QWidget(panel);
    auto* modeLayout = new QHBoxLayout(modeBar);
    modeLayout->setContentsMargins(4, 0, 4, 4);
    modeLayout->setSpacing(4);

    const QString modeBtnStyle = QStringLiteral(
        "QToolButton { border:none; border-radius:10px; background:transparent;"
        "  font-size:13px; font-weight:700; padding:4px 14px; color:%1; }"
        "QToolButton:hover { background:%2; }"
        "QToolButton:checked { background:%3; color:%4; }")
        .arg(AppStyle::textSecondary(), AppStyle::hoverBg(),
             AppStyle::accentSoft(), AppStyle::accent());

    auto* emojiModeBtn = new ElaToolButton(modeBar);
    emojiModeBtn->setText(QStringLiteral("😀 表情"));
    emojiModeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    emojiModeBtn->setCheckable(true);
    emojiModeBtn->setStyleSheet(modeBtnStyle);
    modeLayout->addWidget(emojiModeBtn);

    auto* stickerModeBtn = new ElaToolButton(modeBar);
    stickerModeBtn->setText(QStringLiteral("🎭 贴纸"));
    stickerModeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    stickerModeBtn->setCheckable(true);
    stickerModeBtn->setStyleSheet(modeBtnStyle);
    modeLayout->addWidget(stickerModeBtn);

    auto* customModeBtn = new ElaToolButton(modeBar);
    customModeBtn->setText(QStringLiteral("✨ 自定义"));
    customModeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    customModeBtn->setCheckable(true);
    customModeBtn->setStyleSheet(modeBtnStyle);
    modeLayout->addWidget(customModeBtn);

    modeLayout->addStretch();

    panelLayout->addWidget(modeBar);

    // ─── 内容堆叠容器 ───
    auto* contentStack = new QWidget(panel);
    auto* stackLayout = new QStackedLayout(contentStack);
    stackLayout->setContentsMargins(0, 0, 0, 0);

    // ============ 表情面板 ============
    auto* emojiSubPanel = new QWidget(contentStack);
    auto* emojiLayout = new QVBoxLayout(emojiSubPanel);
    emojiLayout->setContentsMargins(0, 0, 0, 0);
    emojiLayout->setSpacing(4);

    // ─── 分类标签栏 ───
    auto* tabBar = new QWidget(emojiSubPanel);
    auto* tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setContentsMargins(4, 0, 4, 0);
    tabLayout->setSpacing(2);

    const QString tabBtnStyle = QStringLiteral(
        "QToolButton { border:none; border-radius:8px; background:transparent;"
        "  font-size:18px; padding:0px; }"
        "QToolButton:hover { background:%1; }"
        "QToolButton:checked { background:%2; }")
        .arg(AppStyle::hoverBg(), AppStyle::surfaceMuted());

    // ─── 表情网格显示区 ───
    auto* scrollArea = new ElaScrollArea(emojiSubPanel);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFixedSize(380, 280);
    scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background:transparent; border:none; }"
        "QScrollBar:vertical { width:6px; background:transparent; }"
        "QScrollBar::handle:vertical { background:%1; border-radius:3px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
        .arg(AppStyle::border()));

    auto* gridHost = new QWidget(scrollArea);
    auto* grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);

    const QString emojiButtonStyle = QStringLiteral(
        "QToolButton { border:none; border-radius:10px; background:%1; font-size:22px; }"
        "QToolButton:hover { background:%2; }"
        "QToolButton:pressed { background:%3; }")
        .arg(AppStyle::surfaceAlt(), AppStyle::hoverBg(), AppStyle::surfaceMuted());

    constexpr int columns = 8;
    constexpr int cellSize = 42;

    // 填充网格的 lambda
    auto populateGrid = [this, grid, gridHost, cellSize, columns, emojiButtonStyle,
                         closePopupLater](const QStringList& emojis) {
        // 清空旧内容
        while (grid->count() > 0) {
            auto* item = grid->takeAt(0);
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        for (int i = 0; i < emojis.size(); ++i) {
            const QString emoji = emojis.at(i);
            auto* btn = new ElaToolButton(gridHost);
            btn->setText(emoji);
            btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
            btn->setFixedSize(cellSize, cellSize);
            btn->setStyleSheet(emojiButtonStyle);
            connect(btn, &QToolButton::clicked, gridHost, [this, closePopupLater, emoji]() {
                if (!m_messageEditor)
                    return;
                m_messageEditor->insertPlainText(emoji);
                m_messageEditor->setFocus();
                saveRecentEmoji(emoji);
                closePopupLater();
            });
            grid->addWidget(btn, i / columns, i % columns);
        }
        gridHost->adjustSize();
    };

    scrollArea->setWidget(gridHost);

    // ─── "最近使用" Tab ───
    auto* recentBtn = new ElaToolButton(tabBar);
    recentBtn->setText(QStringLiteral("🕐"));
    recentBtn->setToolTip(QStringLiteral("最近使用"));
    recentBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    recentBtn->setCheckable(true);
    recentBtn->setFixedSize(36, 36);
    recentBtn->setStyleSheet(tabBtnStyle);
    tabLayout->addWidget(recentBtn);

    // ─── 分类 Tabs ───
    const auto categories = emojiCategories();
    QList<ElaToolButton*> tabButtons;
    tabButtons.append(recentBtn);

    for (int ci = 0; ci < categories.size(); ++ci) {
        auto* tabBtn = new ElaToolButton(tabBar);
        tabBtn->setText(categories[ci].icon);
        tabBtn->setToolTip(categories[ci].label);
        tabBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        tabBtn->setCheckable(true);
        tabBtn->setFixedSize(36, 36);
        tabBtn->setStyleSheet(tabBtnStyle);
        tabLayout->addWidget(tabBtn);
        tabButtons.append(tabBtn);
    }
    tabLayout->addStretch();

    // tab 切换逻辑
    auto switchTab = [tabButtons, populateGrid, categories](int index) {
        for (int i = 0; i < tabButtons.size(); ++i)
            tabButtons[i]->setChecked(i == index);
        if (index == 0) {
            const QStringList recent = loadRecentEmojis();
            if (recent.isEmpty())
                populateGrid(categories.first().emojis);
            else
                populateGrid(recent);
        } else {
            populateGrid(categories[index - 1].emojis);
        }
    };

    connect(recentBtn, &QToolButton::clicked, emojiSubPanel, [switchTab]() { switchTab(0); });
    for (int ci = 0; ci < categories.size(); ++ci) {
        connect(tabButtons[ci + 1], &QToolButton::clicked, emojiSubPanel,
                [switchTab, ci]() { switchTab(ci + 1); });
    }

    emojiLayout->addWidget(tabBar);
    emojiLayout->addWidget(scrollArea);

    // 默认显示最近使用（有记录时）或第一个分类
    switchTab(0);

    stackLayout->addWidget(emojiSubPanel);

    // ─── 共享常量 & StickerManager ───
    constexpr int stickerColumns = 4;
    constexpr int stickerCellSize = 80;

    const QString stickerCellStyle = QStringLiteral(
        "QToolButton { border:none; border-radius:10px; background:%1; padding:4px; }"
        "QToolButton:hover { background:%2; }"
        "QToolButton:pressed { background:%3; }")
        .arg(AppStyle::surfaceAlt(), AppStyle::hoverBg(), AppStyle::surfaceMuted());

    const QString scrollStyle = QStringLiteral(
        "QScrollArea { background:transparent; border:none; }"
        "QScrollBar:vertical { width:6px; background:transparent; }"
        "QScrollBar::handle:vertical { background:%1; border-radius:3px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }")
        .arg(AppStyle::border());

    auto& sm = StickerManager::instance();
    auto* stickerManager = &sm;

    // ============ 贴纸面板（内置 default 包）============
    auto* stickerPanel = new QWidget(contentStack);
    auto* stickerLayout = new QVBoxLayout(stickerPanel);
    stickerLayout->setContentsMargins(0, 0, 0, 0);
    stickerLayout->setSpacing(0);

    auto* stickerScroll = new ElaScrollArea(stickerPanel);
    stickerScroll->setFrameShape(QFrame::NoFrame);
    stickerScroll->setWidgetResizable(true);
    stickerScroll->setFixedSize(380, 280);
    stickerScroll->setStyleSheet(scrollStyle);

    auto* stickerGridHost = new QWidget(stickerScroll);
    auto* stickerGrid = new QGridLayout(stickerGridHost);
    stickerGrid->setContentsMargins(4, 4, 4, 4);
    stickerGrid->setHorizontalSpacing(6);
    stickerGrid->setVerticalSpacing(6);
    stickerScroll->setWidget(stickerGridHost);

    // 填充内置贴纸
    {
        const QString packId = QStringLiteral("default");
        for (const auto& pack : sm.packs()) {
            if (pack.id != packId) continue;
            for (int i = 0; i < pack.stickers.size(); ++i) {
                const auto& sticker = pack.stickers[i];
                auto* btn = new ElaToolButton(stickerGridHost);
                btn->setFixedSize(stickerCellSize, stickerCellSize);
                btn->setIconSize(QSize(stickerCellSize - 12, stickerCellSize - 12));
                btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
                btn->setStyleSheet(stickerCellStyle);
                btn->setToolTip(sticker.label);
                QPixmap thumb = sm.stickerThumbnail(packId, sticker.id, stickerCellSize - 12);
                if (!thumb.isNull()) btn->setIcon(QIcon(thumb));
                else btn->setText(sticker.emoji);
                const QString sid = sticker.id;
                connect(btn, &QToolButton::clicked, stickerGridHost,
                        [sendStickerAfterMenuClose, packId, sid]() {
                            sendStickerAfterMenuClose(packId, sid);
                        });
                stickerGrid->addWidget(btn, i / stickerColumns, i % stickerColumns);
            }
            break;
        }
        stickerGridHost->adjustSize();
    }

    stickerLayout->addWidget(stickerScroll);
    stackLayout->addWidget(stickerPanel);

    // ============ 自定义面板（导入 + 收藏）============
    auto* customPanel = new QWidget(contentStack);
    auto* customLayout = new QVBoxLayout(customPanel);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->setSpacing(4);

    // 顶部导入按钮栏
    auto* customTopBar = new QWidget(customPanel);
    auto* customTopLayout = new QHBoxLayout(customTopBar);
    customTopLayout->setContentsMargins(4, 2, 4, 2);
    customTopLayout->setSpacing(4);
    customTopLayout->addStretch();

    auto* importBtn = new ElaToolButton(customTopBar);
    importBtn->setText(QStringLiteral("+ 添加"));
    importBtn->setToolTip(QStringLiteral("添加表情图片"));
    importBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    importBtn->setFixedHeight(28);
    importBtn->setStyleSheet(tabBtnStyle);
    customTopLayout->addWidget(importBtn);

    auto* customScroll = new ElaScrollArea(customPanel);
    customScroll->setFrameShape(QFrame::NoFrame);
    customScroll->setWidgetResizable(true);
    customScroll->setFixedSize(380, 280);
    customScroll->setStyleSheet(scrollStyle);

    auto* customGridHost = new QWidget(customScroll);
    auto* customGrid = new QGridLayout(customGridHost);
    customGrid->setContentsMargins(4, 4, 4, 4);
    customGrid->setHorizontalSpacing(6);
    customGrid->setVerticalSpacing(6);
    customScroll->setWidget(customGridHost);

    // 填充自定义贴纸 lambda（需要刷新时也能调用）
    auto populateCustomGrid = [customGrid, customGridHost, stickerCellSize,
                               stickerColumns, stickerCellStyle, stickerManager,
                               sendStickerAfterMenuClose]() {
        while (customGrid->count() > 0) {
            auto* item = customGrid->takeAt(0);
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        const auto stickers = stickerManager->customStickers();
        constexpr int kDelSize = 16;
        for (int i = 0; i < stickers.size(); ++i) {
            const auto& sticker = stickers[i];
            const QString sid = sticker.id;
            const QString pid = sticker.packId;

            // 容器 cell：贴纸按钮 + 右上角 × 删除按钮叠加
            auto* cell = new QWidget(customGridHost);
            cell->setFixedSize(stickerCellSize, stickerCellSize);

            auto* btn = new ElaToolButton(cell);
            btn->setFixedSize(stickerCellSize, stickerCellSize);
            btn->move(0, 0);
            btn->setIconSize(QSize(stickerCellSize - 12, stickerCellSize - 12));
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            btn->setContextMenuPolicy(Qt::NoContextMenu);
            btn->setStyleSheet(stickerCellStyle);
            btn->setToolTip(sticker.label);
            QPixmap thumb = stickerManager->stickerThumbnail(sticker.packId, sticker.id, stickerCellSize - 12);
            if (!thumb.isNull()) {
                btn->setIcon(QIcon(thumb));
            } else {
                btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
                const QString label = sticker.label.isEmpty() ? sticker.id : sticker.label;
                btn->setText(label.length() > 5 ? label.left(5) + QStringLiteral("…") : label);
                btn->setFont(QFont(QString(), 9));
            }
            connect(btn, &QToolButton::clicked, cell,
                    [sendStickerAfterMenuClose, pid, sid]() {
                        sendStickerAfterMenuClose(pid, sid);
                    });

            // 右上角 × 删除按钮
            auto* delBtn = new ElaToolButton(cell);
            delBtn->setFixedSize(kDelSize, kDelSize);
            delBtn->move(stickerCellSize - kDelSize, 0);
            delBtn->setText(QStringLiteral("×"));
            delBtn->setToolTip(QStringLiteral("删除此表情"));
            delBtn->setStyleSheet(
                QStringLiteral("QToolButton {"
                               "  background: rgba(60,60,60,0.70);"
                               "  color: rgba(255,255,255,0.85);"
                               "  border: none;"
                               "  border-radius: 8px;"
                               "  font-size: 11px;"
                               "  font-weight: bold;"
                               "}"
                               "QToolButton:hover {"
                               "  background: #e74c3c;"
                               "  color: white;"
                               "}"));
            delBtn->raise();
            connect(delBtn, &QToolButton::clicked, cell, [pid, sid]() {
                StickerManager::instance().removeSticker(pid, sid);
            });

            customGrid->addWidget(cell, i / stickerColumns, i % stickerColumns);
        }
        if (stickers.isEmpty()) {
            auto* hint = new ElaText(QStringLiteral("点击 \"+ 添加\" 导入表情图片"), customGridHost);
            hint->setAlignment(Qt::AlignCenter);
            hint->setStyleSheet(QStringLiteral("color:%1; font-size:13px; padding:40px;")
                                    .arg(AppStyle::textSecondary()));
            customGrid->addWidget(hint, 0, 0, 1, stickerColumns);
        }
        customGridHost->adjustSize();
    };

    populateCustomGrid();

    // 监听 packsChanged 以刷新自定义面板
    connect(stickerManager, &StickerManager::packsChanged, customPanel, populateCustomGrid);

    connect(importBtn, &QToolButton::clicked, customPanel, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(
            this, QStringLiteral("选择表情图片"), QString(),
            QStringLiteral("图片文件 (*.gif *.png *.jpg *.jpeg *.webp)"));
        if (files.isEmpty())
            return;
        StickerManager::instance().importToCustomPack(files);
        m_lastEmojiMode = 2;
    });

    customLayout->addWidget(customTopBar);
    customLayout->addWidget(customScroll);

    stackLayout->addWidget(customPanel);

    panelLayout->addWidget(contentStack);

    // ─── 顶层三标签切换逻辑 ───
    auto switchMode = [emojiModeBtn, stickerModeBtn, customModeBtn,
                       stackLayout, panel](int mode) {
        emojiModeBtn->setChecked(mode == 0);
        stickerModeBtn->setChecked(mode == 1);
        customModeBtn->setChecked(mode == 2);
        stackLayout->setCurrentIndex(mode);
        panel->adjustSize();
    };

    connect(emojiModeBtn, &QToolButton::clicked, panel, [this, switchMode]() { m_lastEmojiMode = 0; switchMode(0); });
    connect(stickerModeBtn, &QToolButton::clicked, panel, [this, switchMode]() { m_lastEmojiMode = 1; switchMode(1); });
    connect(customModeBtn, &QToolButton::clicked, panel, [this, switchMode]() { m_lastEmojiMode = 2; switchMode(2); });
    switchMode(m_lastEmojiMode);

    panel->adjustSize();
}

void ChatComposerWidget::refreshTheme()
{
    setStyleSheet(QStringLiteral(
                      "QWidget#chatComposerWidget {"
                      "  background:transparent;"
                      "}"
                      "QFrame#composerSurface {"
                      "  background:%1;"
                      "  border:1px solid %9;"
                      "  border-radius:20px;"
                      "}"
                      "QFrame#composerControlBand {"
                      "  background:transparent;"
                      "  border:none;"
                      "}"
                      "QLabel#composerModeChip {"
                      "  background:%4;"
                      "  color:%5;"
                      "  border:none;"
                      "  border-radius:999px;"
                      "  font-size:11px;"
                      "  font-weight:700;"
                      "  padding:4px 10px;"
                      "}"
                      "QLabel#composerMetaChip {"
                      "  background:%2;"
                      "  color:%6;"
                      "  border:none;"
                      "  border-radius:999px;"
                      "  font-size:11px;"
                      "  font-weight:600;"
                      "  padding:4px 10px;"
                      "}"
                      "QLabel#composerHintLabel, QLabel#composerShortcutHint {"
                      "  color:%7;"
                      "  font-size:11px;"
                      "  font-weight:600;"
                      "}"
                      "QTextEdit {"
                      "  border:none;"
                      "  background:transparent;"
                      "  color:%8;"
                      "  font-size:14px;"
                      "  padding:0;"
                      "}"
                      "QFrame#composerReplyIndicator {"
                      "  background:%5;"
                      "  border-radius:1px;"
                      "}"
                      "QLabel#composerReplyPreviewLabel {"
                      "  color:%6;"
                      "  font-size:12px;"
                      "}"
                      "QPushButton#composerReplyCloseButton {"
                      "  border:none; background:transparent;"
                      "  color:%6; font-size:14px;"
                      "}"
                      "QPushButton#composerReplyCloseButton:hover {"
                      "  color:%8;"
                      "}"
                      "QPushButton#composerSendModeButton {"
                      "  background:%3;"
                      "  border:none;"
                      "  border-radius:10px;"
                      "  color:%6;"
                      "  font-size:12px;"
                      "  font-weight:600;"
                      "}"
                      "QPushButton#composerSendModeButton:hover {"
                      "  background:%4;"
                      "  color:%8;"
                      "}"
                      "QPushButton#composerNudgeButton {"
                      "  border:none;"
                      "  border-radius:10px;"
                      "  background:transparent;"
                      "  color:%8;"
                      "  font-size:14px;"
                      "  font-weight:700;"
                      "  padding:0;"
                      "}"
                      "QPushButton#composerNudgeButton:hover {"
                      "  background:%3;"
                      "}"
                      "QPushButton#composerNudgeButton:disabled {"
                      "  color:%7;"
                      "}")
                      .arg(AppStyle::chatCardBg(),
                          AppStyle::chatCardBg(),
                          AppStyle::surfaceMuted(),
                          AppStyle::accentSoft(),
                          AppStyle::accent(),
                          AppStyle::textSecondary(),
                          AppStyle::textMuted(),
                           AppStyle::textPrimary(),
                           AppStyle::border()));

    // Belt-and-suspenders: fix ElaTextEdit viewport palette for dark theme.
    // ElaTextEditPrivate::onThemeChanged handles this via ElaTheme signal,
    // but we also set it here so the palette is in sync with AppStyle.
    if (m_messageEditor && m_messageEditor->viewport()) {
        QPalette vp = m_messageEditor->viewport()->palette();
        vp.setColor(QPalette::Base, Qt::transparent);
        vp.setColor(QPalette::Window, Qt::transparent);
        m_messageEditor->viewport()->setPalette(vp);
        m_messageEditor->viewport()->update();
    }
}

QStringList ChatComposerWidget::emojiChoicesForTesting()
{
    return allEmojiChoices();
}

QList<int> ChatComposerWidget::fontSizeChoicesForTesting()
{
    return curatedFontSizes();
}

QStringList ChatComposerWidget::fontFamilyChoicesForTesting()
{
    return curatedFontFamilies();
}

void ChatComposerWidget::setDraftMetaText(const QString& text)
{
    if (!m_metaChipLabel) {
        return;
    }

    const QString trimmed = text.trimmed();
    m_metaChipLabel->setText(trimmed.isEmpty() ? QStringLiteral("截图 / 文件 / 待发") : trimmed);
}

QWidget* ChatComposerWidget::toolbarHost() const { return m_toolbarHost; }
ElaTextEdit* ChatComposerWidget::messageEditor() const { return m_messageEditor; }
ElaPushButton* ChatComposerWidget::sendButton() const { return m_sendButton; }
ElaToolButton* ChatComposerWidget::fileButton() const { return m_fileButton; }
ElaToolButton* ChatComposerWidget::screenshotButton() const { return m_screenshotButton; }
ElaPushButton* ChatComposerWidget::sendModeButton() const { return m_sendModeButton; }
ElaToolButton* ChatComposerWidget::nudgeButton() const { return m_nudgeButton; }
QWidget* ChatComposerWidget::moreActionsButton() const { return nullptr; }
ElaToolButton* ChatComposerWidget::devOpsButton() const { return nullptr; }
ElaToolButton* ChatComposerWidget::boldButton() const { return m_boldButton; }
ElaToolButton* ChatComposerWidget::italicButton() const { return m_italicButton; }
ElaToolButton* ChatComposerWidget::underlineButton() const { return m_underlineButton; }
ElaToolButton* ChatComposerWidget::fontButton() const { return m_fontButton; }
ElaToolButton* ChatComposerWidget::emojiButton() const { return m_emojiButton; }

QStringList ChatComposerWidget::secondaryActionLabelsForTesting() const
{
    return {};
}

void ChatComposerWidget::enterEditMode(const QString& messageId, const QString& currentBody)
{
    m_editingMessageId = messageId;
    if (m_messageEditor) {
        if (Qt::mightBeRichText(currentBody)) {
            m_messageEditor->setHtml(currentBody);
        } else {
            m_messageEditor->setPlainText(currentBody);
        }
        m_messageEditor->setFocus();
    }
    if (m_controlBand) {
        m_controlBand->setVisible(true);
    }
    if (auto* editCloseButton = findChild<ElaToolButton*>(QStringLiteral("composerEditCloseButton"))) {
        editCloseButton->show();
    }
    setDraftMetaText(QStringLiteral("正在编辑消息"));
    if (m_sendButton) {
        m_sendButton->setText(QStringLiteral("保存"));
    }
    emit recoveryContextChanged();
}

void ChatComposerWidget::exitEditMode()
{
    m_editingMessageId.clear();
    if (m_messageEditor) {
        m_messageEditor->clear();
    }
    if (m_controlBand) {
        m_controlBand->setVisible(false);
    }
    if (auto* editCloseButton = findChild<ElaToolButton*>(QStringLiteral("composerEditCloseButton"))) {
        editCloseButton->hide();
    }
    setDraftMetaText(QString());
    if (m_sendButton) {
        m_sendButton->setText(QStringLiteral("发送"));
    }
    emit recoveryContextChanged();
    emit recoveryContextCommitted();
}

QString ChatComposerWidget::editingMessageId() const { return m_editingMessageId; }
bool ChatComposerWidget::isInEditMode() const { return !m_editingMessageId.isEmpty(); }

void ChatComposerWidget::setReplyContext(const QString& messageId, const QString& senderId,
                                          const QString& senderName, const QString& bodyPreview) {
    m_replyToMessageId = messageId;
    m_replyToSenderId = senderId;
    m_replyToSenderName = senderName;
    // 截取前100个字符
    const QString truncated = bodyPreview.length() > 100
        ? bodyPreview.left(100) + QStringLiteral("...")
        : bodyPreview;
    m_replyToBody = truncated;
    if (m_replyPreviewLabel) {
        m_replyPreviewLabel->setText(
            QStringLiteral("\u56DE\u590D %1\uFF1A%2").arg(senderName, truncated));
    }
    if (m_replyPreviewBar) {
        m_replyPreviewBar->setVisible(true);
    }
    if (m_messageEditor) {
        m_messageEditor->setFocus();
    }
    emit recoveryContextChanged();
}

void ChatComposerWidget::clearReplyContext() {
    m_replyToMessageId.clear();
    m_replyToSenderId.clear();
    m_replyToSenderName.clear();
    m_replyToBody.clear();
    if (m_replyPreviewBar) {
        m_replyPreviewBar->setVisible(false);
    }
    emit recoveryContextChanged();
    emit recoveryContextCommitted();
}

QString ChatComposerWidget::replyToMessageId() const { return m_replyToMessageId; }
QString ChatComposerWidget::replyToSenderId() const { return m_replyToSenderId; }
QString ChatComposerWidget::replyToSenderName() const { return m_replyToSenderName; }
QString ChatComposerWidget::replyToBody() const { return m_replyToBody; }

ComposerRecoveryContext ChatComposerWidget::recoveryContext() const
{
    ComposerRecoveryContext context;
    if (m_messageEditor) {
        context.composerHtml = m_messageEditor->toHtml();
    }
    context.replyMessageId = m_replyToMessageId;
    context.replySenderId = m_replyToSenderId;
    context.replySenderName = m_replyToSenderName;
    context.replyBody = m_replyToBody;
    context.editingMessageId = m_editingMessageId;
    if (!m_editingMessageId.isEmpty()) {
        context.editingBody = context.composerHtml;
    }
    return context;
}

void ChatComposerWidget::restoreRecoveryContext(const ComposerRecoveryContext& context)
{
    QSignalBlocker blocker(this);

    clearReplyContext();
    exitEditMode();
    if (!context.editingMessageId.isEmpty()) {
        enterEditMode(context.editingMessageId, context.editingBody);
    } else if (m_messageEditor) {
        m_messageEditor->setHtml(context.composerHtml);
    }
    if (!context.replyMessageId.isEmpty()) {
        setReplyContext(context.replyMessageId, context.replySenderId,
                        context.replySenderName, context.replyBody);
    }

    blocker.unblock();
    emit recoveryContextChanged();
}
