#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

enum class RecipientCapabilityStatus {
    LegacyP2P,
    ServerReceiveCapable,
    UnknownTreatAsLegacy
};

enum class RecipientCapabilitySource {
    None,
    LocalObservation,
    ServerProfile
};

struct RecipientCapabilityDecision {
    QString clientId;
    RecipientCapabilityStatus status =
        RecipientCapabilityStatus::UnknownTreatAsLegacy;
    RecipientCapabilitySource source = RecipientCapabilitySource::None;
    qint64 observedAtMs = 0;
};

class RecipientCapabilityResolver {
public:
    explicit RecipientCapabilityResolver(qint64 localObservationTtlMs = 120000,
                                         qint64 serverProfileTtlMs = 300000);

    void rememberLocalObservation(const QString& clientId,
                                  const QStringList& capabilities,
                                  qint64 observedAtMs);
    void rememberServerProfile(const QString& clientId,
                               const QStringList& capabilities,
                               qint64 observedAtMs);
    void rememberServerQueryResult(
        const QStringList& requestedClientIds,
        const QHash<QString, QStringList>& capabilitiesByClientId,
        qint64 observedAtMs);
    RecipientCapabilityDecision resolveFromCacheOnly(const QString& clientId,
                                                     qint64 nowMs) const;
    bool shouldRefreshServerProfile(const RecipientCapabilityDecision& decision,
                                    qint64 nowMs,
                                    qint64 refreshAfterMs) const;

private:
    struct Observation {
        QStringList capabilities;
        qint64 observedAtMs = 0;
    };

    static QString normalizedClientId(const QString& clientId);
    static QStringList normalizedCapabilities(const QStringList& capabilities);
    static bool isFresh(const Observation& observation,
                        qint64 nowMs,
                        qint64 ttlMs);
    static RecipientCapabilityStatus statusForCapabilities(
        const QStringList& capabilities,
        RecipientCapabilityStatus legacyStatus);

    qint64 m_localObservationTtlMs = 120000;
    qint64 m_serverProfileTtlMs = 300000;
    QHash<QString, Observation> m_localObservations;
    QHash<QString, Observation> m_serverProfiles;
};
