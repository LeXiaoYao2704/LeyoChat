#include "ElaTextEditPrivate.h"

#include "ElaApplication.h"
#include "ElaTextEdit.h"
#include "ElaTheme.h"

ElaTextEditPrivate::ElaTextEditPrivate(QObject* parent)
    : QObject{parent}
{
}

ElaTextEditPrivate::~ElaTextEditPrivate()
{
}

void ElaTextEditPrivate::onWMWindowClickedEvent(QVariantMap data)
{
    Q_Q(ElaTextEdit);
    ElaAppBarType::WMMouseActionType actionType = data.value("WMClickType").value<ElaAppBarType::WMMouseActionType>();
    if (actionType == ElaAppBarType::WMLBUTTONDOWN)
    {
        if (!q->toPlainText().isEmpty() && q->hasFocus())
        {
            q->clearFocus();
        }
    }
    else if (actionType == ElaAppBarType::WMLBUTTONUP || actionType == ElaAppBarType::WMNCLBUTTONDOWN)
    {
        if (ElaApplication::containsCursorToItem(q) || (actionType == ElaAppBarType::WMLBUTTONUP && !q->toPlainText().isEmpty()))
        {
            return;
        }
        if (q->hasFocus())
        {
            q->clearFocus();
        }
    }
}

void ElaTextEditPrivate::onThemeChanged(ElaThemeType::ThemeMode themeMode)
{
    Q_Q(ElaTextEdit);
    _themeMode = themeMode;
    QPalette palette = q->palette();
    palette.setColor(QPalette::Text, ElaThemeColor(_themeMode, BasicText));
    palette.setColor(QPalette::PlaceholderText, _themeMode == ElaThemeType::Light ? QColor(0x00, 0x00, 0x00, 128) : QColor(0xBA, 0xBA, 0xBA));
    palette.setColor(QPalette::Base, Qt::transparent);
    q->setPalette(palette);
    if (q->viewport()) {
        QPalette vp = q->viewport()->palette();
        vp.setColor(QPalette::Base, Qt::transparent);
        q->viewport()->setPalette(vp);
    }
}
