#pragma once
#include "ElaScrollPage.h"

#ifdef LEYOCHAT_HAS_WEBENGINE
class FerryBrowserWidget;
#endif

class FerryPage : public ElaScrollPage {
    Q_OBJECT
public:
    explicit FerryPage(QWidget* parent = nullptr);

#ifdef LEYOCHAT_HAS_WEBENGINE
    FerryBrowserWidget* ferryBrowser() const { return m_ferryBrowser; }
#endif

protected:
    void showEvent(QShowEvent* event) override;

private:
#ifdef LEYOCHAT_HAS_WEBENGINE
    FerryBrowserWidget* m_ferryBrowser = nullptr;
    bool m_initialized = false;
#endif
};
