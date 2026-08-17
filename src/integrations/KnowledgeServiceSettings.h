#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVector>

struct KnowledgeServiceConfig {
    QString localId;
    QString displayName;
    QString baseUrl;
    QString teamLabel;
    QString accessToken;
    QString serviceInstanceId;
    QString knowledgeBaseId;
    QString apiVersion;
    QStringList capabilities;
    bool isDefault = false;
    bool lastConnectionOk = false;
    QString lastConnectionMessage;
    qint64 lastUsedAtMs = 0;
};

namespace KnowledgeServiceSettingsStore {

QVector<KnowledgeServiceConfig> load(QSettings* settings = nullptr);
void save(const QVector<KnowledgeServiceConfig>& configs, QSettings* settings = nullptr);
QString defaultServiceId(QSettings* settings = nullptr);
QVector<KnowledgeServiceConfig> fromJson(const QString& json);
QString toJson(const QVector<KnowledgeServiceConfig>& configs);

}  // namespace KnowledgeServiceSettingsStore
