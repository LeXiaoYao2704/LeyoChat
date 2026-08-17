#include "services/RecipientCapabilityResolver.h"

#include "services/MessageRoutingCapabilities.h"

#include <QSet>

#include <algorithm>

RecipientCapabilityResolver::RecipientCapabilityResolver(
    qint64 localObservationTtlMs,
    qint64 serverProfileTtlMs)
    : m_localObservationTtlMs(std::max<qint64>(1, localObservationTtlMs))
    , m_serverProfileTtlMs(std::max<qint64>(1, serverProfileTtlMs))
{
}

void RecipientCapabilityResolver::rememberLocalObservation(
    const QString& clientId,
    const QStringList& capabilities,
    qint64 observedAtMs)
{
    const QString normalizedId = normalizedClientId(clientId);
    if (normalizedId.isEmpty() || observedAtMs <= 0) {
        return;
    }

    m_localObservations.insert(
        normalizedId,
        Observation{normalizedCapabilities(capabilities), observedAtMs});
}

void RecipientCapabilityResolver::rememberServerProfile(
    const QString& clientId,
    const QStringList& capabilities,
    qint64 observedAtMs)
{
    const QString normalizedId = normalizedClientId(clientId);
    if (normalizedId.isEmpty() || observedAtMs <= 0) {
        return;
    }

    m_serverProfiles.insert(
        normalizedId,
        Observation{normalizedCapabilities(capabilities), observedAtMs});
}

void RecipientCapabilityResolver::rememberServerQueryResult(
    const QStringList& requestedClientIds,
    const QHash<QString, QStringList>& capabilitiesByClientId,
    qint64 observedAtMs)
{
    if (observedAtMs <= 0) {
        return;
    }

    QHash<QString, QStringList> normalizedCapabilitiesByClientId;
    for (auto it = capabilitiesByClientId.cbegin();
         it != capabilitiesByClientId.cend();
         ++it) {
        const QString normalizedId = normalizedClientId(it.key());
        if (!normalizedId.isEmpty()) {
            normalizedCapabilitiesByClientId.insert(normalizedId, it.value());
        }
    }

    QSet<QString> seen;
    for (const QString& rawClientId : requestedClientIds) {
        const QString normalizedId = normalizedClientId(rawClientId);
        if (normalizedId.isEmpty() || seen.contains(normalizedId)) {
            continue;
        }
        seen.insert(normalizedId);

        const auto capabilities =
            normalizedCapabilitiesByClientId.constFind(normalizedId);
        rememberServerProfile(
            normalizedId,
            capabilities == normalizedCapabilitiesByClientId.cend()
                ? QStringList{}
                : capabilities.value(),
            observedAtMs);
    }
}

RecipientCapabilityDecision RecipientCapabilityResolver::resolveFromCacheOnly(
    const QString& clientId,
    qint64 nowMs) const
{
    const QString normalizedId = normalizedClientId(clientId);
    RecipientCapabilityDecision decision;
    decision.clientId = normalizedId;
    if (normalizedId.isEmpty()) {
        return decision;
    }

    const auto local = m_localObservations.constFind(normalizedId);
    if (local != m_localObservations.cend()
        && isFresh(local.value(), nowMs, m_localObservationTtlMs)) {
        decision.source = RecipientCapabilitySource::LocalObservation;
        decision.observedAtMs = local->observedAtMs;
        decision.status = statusForCapabilities(
            local->capabilities,
            RecipientCapabilityStatus::LegacyP2P);
        return decision;
    }

    const auto server = m_serverProfiles.constFind(normalizedId);
    if (server != m_serverProfiles.cend()
        && isFresh(server.value(), nowMs, m_serverProfileTtlMs)) {
        decision.source = RecipientCapabilitySource::ServerProfile;
        decision.observedAtMs = server->observedAtMs;
        decision.status = statusForCapabilities(
            server->capabilities,
            RecipientCapabilityStatus::UnknownTreatAsLegacy);
        return decision;
    }

    return decision;
}

bool RecipientCapabilityResolver::shouldRefreshServerProfile(
    const RecipientCapabilityDecision& decision,
    qint64 nowMs,
    qint64 refreshAfterMs) const
{
    if (decision.clientId.trimmed().isEmpty()
        || decision.status == RecipientCapabilityStatus::ServerReceiveCapable) {
        return false;
    }

    if (decision.source == RecipientCapabilitySource::None) {
        return true;
    }
    if (decision.source != RecipientCapabilitySource::ServerProfile) {
        return false;
    }

    const qint64 effectiveRefreshMs = std::max<qint64>(1, refreshAfterMs);
    return decision.observedAtMs > 0
        && nowMs >= decision.observedAtMs
        && nowMs - decision.observedAtMs >= effectiveRefreshMs;
}

QString RecipientCapabilityResolver::normalizedClientId(const QString& clientId)
{
    return clientId.trimmed();
}

QStringList RecipientCapabilityResolver::normalizedCapabilities(
    const QStringList& capabilities)
{
    QStringList result;
    QSet<QString> seen;
    for (const QString& raw : capabilities) {
        const QString capability = raw.trimmed();
        const QString key = capability.toLower();
        if (capability.isEmpty() || seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        result.push_back(capability);
    }
    return result;
}

bool RecipientCapabilityResolver::isFresh(const Observation& observation,
                                          qint64 nowMs,
                                          qint64 ttlMs)
{
    return observation.observedAtMs > 0
        && nowMs >= observation.observedAtMs
        && nowMs - observation.observedAtMs <= ttlMs;
}

RecipientCapabilityStatus
RecipientCapabilityResolver::statusForCapabilities(
    const QStringList& capabilities,
    RecipientCapabilityStatus legacyStatus)
{
    return MessageRoutingCapabilities::hasServerReceiveV1(capabilities)
        ? RecipientCapabilityStatus::ServerReceiveCapable
        : legacyStatus;
}
