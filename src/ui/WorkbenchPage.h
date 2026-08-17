#pragma once

#include <QHash>
#include <QWidget>

class WorkbenchPage : public QWidget {
    Q_OBJECT

public:
    explicit WorkbenchPage(QWidget* parent = nullptr);

    bool hasCardForTesting(const QString& key) const;

private:
    QWidget* createFeatureCard(const QString& key,
                               const QString& title,
                               const QString& description,
                               QWidget* parent);

    QHash<QString, QWidget*> m_featureCards;
};