#include "FerryPage.h"

#include <ElaText.h>
#include <QShowEvent>
#include <QVBoxLayout>

#ifdef LEYOCHAT_HAS_WEBENGINE
#include "ui/FerryBrowserWidget.h"
#endif

FerryPage::FerryPage(QWidget* parent)
    : ElaScrollPage(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

#ifndef LEYOCHAT_HAS_WEBENGINE
    auto* placeholder = new ElaText(QStringLiteral("\u6446\u6E21\u529F\u80FD\u9700\u8981 WebEngine \u652F\u6301"), this);
    placeholder->setTextPixelSize(18);
    rootLayout->addWidget(placeholder, 0, Qt::AlignCenter);
#endif
}

void FerryPage::showEvent(QShowEvent* event)
{
    ElaScrollPage::showEvent(event);
#ifdef LEYOCHAT_HAS_WEBENGINE
    if (!m_initialized) {
        m_initialized = true;
        m_ferryBrowser = new FerryBrowserWidget(this);
        if (auto* rootLayout = qobject_cast<QVBoxLayout*>(layout())) {
            rootLayout->addWidget(m_ferryBrowser);
        }
    }
#endif
}
