#include "ElaListWidget.h"

#include "ElaListWidgetPrivate.h"
#include "ElaListViewStyle.h"
#include "ElaScrollBar.h"

ElaListWidget::ElaListWidget(QWidget* parent)
    : QListWidget(parent), d_ptr(new ElaListWidgetPrivate())
{
    Q_D(ElaListWidget);
    d->q_ptr = this;
    setObjectName("ElaListWidget");
    setStyleSheet("#ElaListWidget{background-color:transparent;}");
    d->_listViewStyle = new ElaListViewStyle(style());
    setStyle(d->_listViewStyle);
    setMouseTracking(true);
    setVerticalScrollBar(new ElaScrollBar(this));
    setHorizontalScrollBar(new ElaScrollBar(this));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

ElaListWidget::~ElaListWidget()
{
    Q_D(ElaListWidget);
    delete d->_listViewStyle;
}

void ElaListWidget::setIsTransparent(bool isTransparent)
{
    Q_D(ElaListWidget);
    d->_listViewStyle->setIsTransparent(isTransparent);
    update();
}

bool ElaListWidget::getIsTransparent() const
{
    Q_D(const ElaListWidget);
    return d->_listViewStyle->getIsTransparent();
}
