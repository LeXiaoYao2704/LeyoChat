#include "integrations/KnowledgeServiceSettings.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "app/AppSettings.h"

namespace {

constexpr auto kKnowledgeServicesGroup = "integrations/knowledgeServices";
constexpr auto kServicesJsonKey = "servicesJson";

QString trimmedOrEmpty(const QString& value)
{
    return value.trimmed();
}

KnowledgeServiceConfig normalizeConfig(KnowledgeServiceConfig config)
{
    config.localId = trimmedOrEmpty(config.localId);
    config.displayName = trimmedOrEmpty(config.displayName);
    config.baseUrl = trimmedOrEmpty(config.baseUrl);
    config.teamLabel = trimmedOrEmpty(config.teamLabel);
    config.accessToken = trimmedOrEmpty(config.accessToken);
    config.serviceInstanceId = trimmedOrEmpty(config.serviceInstanceId);
    config.knowledgeBaseId = trimmedOrEmpty(config.knowledgeBaseId);
    config.apiVersion = trimmedOrEmpty(config.apiVersion);
    config.lastConnectionMessage = trimmedOrEmpty(config.lastConnectionMessage);
    config.lastUsedAtMs = qMax<qint64>(0, config.lastUsedAtMs);

    QStringList normalizedCapabilities;
    for (const QString& capability : config.capabilities) {
        const QString normalized = capability.trimmed();
        if (!normalized.isEmpty() && !normalizedCapabilities.contains(normalized)) {
            normalizedCapabilities.push_back(normalized);
        }
    }
    config.capabilities = normalizedCapabilities;
    return config;
}

bool isValidConfig(const KnowledgeServiceConfig& config)
{
    return !config.localId.isEmpty() && !config.baseUrl.isEmpty();
}

bool containsLocalId(const QVector<KnowledgeServiceConfig>& configs, const QString& localId)
{
    for (const KnowledgeServiceConfig& config : configs) {
        if (config.localId == localId) {
            return true;
        }
    }
    return false;
}

QJsonObject toJson(const KnowledgeServiceConfig& config)
{
    QJsonObject object;
    object.insert(QStringLiteral("localId"), config.localId);
    object.insert(QStringLiteral("displayName"), config.displayName);
    object.insert(QStringLiteral("baseUrl"), config.baseUrl);
    object.insert(QStringLiteral("teamLabel"), config.teamLabel);
    object.insert(QStringLiteral("accessToken"), config.accessToken);
    object.insert(QStringLiteral("serviceInstanceId"), config.serviceInstanceId);
    object.insert(QStringLiteral("knowledgeBaseId"), config.knowledgeBaseId);
    object.insert(QStringLiteral("apiVersion"), config.apiVersion);
    object.insert(QStringLiteral("isDefault"), config.isDefault);
    object.insert(QStringLiteral("lastConnectionOk"), config.lastConnectionOk);
    object.insert(QStringLiteral("lastConnectionMessage"), config.lastConnectionMessage);
    object.insert(QStringLiteral("lastUsedAtMs"), static_cast<double>(config.lastUsedAtMs));

    QJsonArray capabilities;
    for (const QString& capability : config.capabilities) {
        capabilities.push_back(capability);
    }
    object.insert(QStringLiteral("capabilities"), capabilities);
    return object;
}

KnowledgeServiceConfig fromJson(const QJsonObject& object)
{
    KnowledgeServiceConfig config;
    config.localId = object.value(QStringLiteral("localId")).toString();
    config.displayName = object.value(QStringLiteral("displayName")).toString();
    config.baseUrl = object.value(QStringLiteral("baseUrl")).toString();
    config.teamLabel = object.value(QStringLiteral("teamLabel")).toString();
    config.accessToken = object.value(QStringLiteral("accessToken")).toString();
    config.serviceInstanceId = object.value(QStringLiteral("serviceInstanceId")).toString();
    config.knowledgeBaseId = object.value(QStringLiteral("knowledgeBaseId")).toString();
    config.apiVersion = object.value(QStringLiteral("apiVersion")).toString();
    config.isDefault = object.value(QStringLiteral("isDefault")).toBool(false);
    config.lastConnectionOk = object.value(QStringLiteral("lastConnectionOk")).toBool(false);
    config.lastConnectionMessage = object.value(QStringLiteral("lastConnectionMessage")).toString();
    config.lastUsedAtMs = qMax<qint64>(0, static_cast<qint64>(object.value(QStringLiteral("lastUsedAtMs")).toDouble()));

    for (const QJsonValue& capability : object.value(QStringLiteral("capabilities")).toArray()) {
        const QString value = capability.toString().trimmed();
        if (!value.isEmpty()) {
            config.capabilities.push_back(value);
        }
    }

    return normalizeConfig(config);
}

QVector<KnowledgeServiceConfig> normalizeConfigs(const QVector<KnowledgeServiceConfig>& configs)
{
    QVector<KnowledgeServiceConfig> normalized;
    bool defaultAssigned = false;

    for (KnowledgeServiceConfig config : configs) {
        config = normalizeConfig(config);
        if (!isValidConfig(config) || containsLocalId(normalized, config.localId)) {
            continue;
        }

        if (config.isDefault) {
            if (defaultAssigned) {
                config.isDefault = false;
            } else {
                defaultAssigned = true;
            }
        }

        normalized.push_back(config);
    }

    return normalized;
}

QVector<KnowledgeServiceConfig> servicesFromJson(const QString& json)
{
    QVector<KnowledgeServiceConfig> configs;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        return configs;
    }

    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        configs.push_back(fromJson(value.toObject()));
    }

    return normalizeConfigs(configs);
}

QString servicesToJson(const QVector<KnowledgeServiceConfig>& configs)
{
    QJsonArray array;
    for (const KnowledgeServiceConfig& config : normalizeConfigs(configs)) {
        array.push_back(toJson(config));
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QSettings* effectiveSettings(QSettings* settings, QSettings& ownedSettings)
{
    return settings ? settings : &ownedSettings;
}

}

namespace KnowledgeServiceSettingsStore {

QVector<KnowledgeServiceConfig> load(QSettings* settings)
{
    QSettings ownedSettings = AppSettings::createSettings();
    QSettings* activeSettings = effectiveSettings(settings, ownedSettings);
    activeSettings->beginGroup(QString::fromLatin1(kKnowledgeServicesGroup));
    const QString servicesJson = activeSettings->value(QString::fromLatin1(kServicesJsonKey)).toString();
    activeSettings->endGroup();
    return servicesFromJson(servicesJson);
}

void save(const QVector<KnowledgeServiceConfig>& configs, QSettings* settings)
{
    QSettings ownedSettings = AppSettings::createSettings();
    QSettings* activeSettings = effectiveSettings(settings, ownedSettings);
    activeSettings->beginGroup(QString::fromLatin1(kKnowledgeServicesGroup));
    activeSettings->setValue(QString::fromLatin1(kServicesJsonKey), servicesToJson(configs));
    activeSettings->endGroup();
    activeSettings->sync();
}

QString defaultServiceId(QSettings* settings)
{
    const QVector<KnowledgeServiceConfig> configs = load(settings);
    for (const KnowledgeServiceConfig& config : configs) {
        if (config.isDefault) {
            return config.localId;
        }
    }
    return {};
}

QVector<KnowledgeServiceConfig> fromJson(const QString& json)
{
    return servicesFromJson(json);
}

QString toJson(const QVector<KnowledgeServiceConfig>& configs)
{
    return servicesToJson(configs);
}

}  // namespace KnowledgeServiceSettingsStore
