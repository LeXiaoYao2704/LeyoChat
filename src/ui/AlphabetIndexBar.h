#pragma once

#include <QWidget>

class ContactListModel;
class QListView;

class AlphabetIndexBar : public QWidget {
    Q_OBJECT

public:
    explicit AlphabetIndexBar(QWidget* parent = nullptr);

    void setModel(ContactListModel* model);
    void setListView(QListView* listView);
    void refresh();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void scrollToLetterAt(const QPoint& pos);

    ContactListModel* m_model = nullptr;
    QListView* m_listView = nullptr;
    QStringList m_letters;
    int m_hoveredIndex = -1;
    bool m_dragging = false;
};
