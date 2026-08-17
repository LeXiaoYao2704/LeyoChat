#pragma once

#include <optional>
#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <QVector>

#include "integrations/OutlookAdapterContracts.h"
#include "integrations/OutlookSettings.h"

// ──────────────────────────────────────────────────────────────────────────
// Transport interface (injectable for tests)
// ──────────────────────────────────────────────────────────────────────────

class IOutlookEwsTransport {
public:
    virtual ~IOutlookEwsTransport() = default;

    // POST a SOAP envelope to endpoint, authenticating with settings.username/password.
    // Returns the raw response body on success, nullopt on error.
    virtual std::optional<QByteArray> soapPost(const QUrl& endpoint,
                                               const QByteArray& envelope,
                                               const OutlookConnectionSettings& settings,
                                               QString* errorMessage) const = 0;
};

class NetworkOutlookEwsTransport : public IOutlookEwsTransport {
public:
    std::optional<QByteArray> soapPost(const QUrl& endpoint,
                                       const QByteArray& envelope,
                                       const OutlookConnectionSettings& settings,
                                       QString* errorMessage) const override;
};

// ──────────────────────────────────────────────────────────────────────────
// SOAP envelope builders and XML response parsers
// ──────────────────────────────────────────────────────────────────────────

namespace OutlookEws {

// Returns the EWS endpoint URL: {serverUrl}/EWS/Exchange.asmx
QUrl ewsEndpoint(const OutlookConnectionSettings& settings);

// Wraps body bytes in a complete SOAP envelope with EWS namespaces.
QByteArray buildSoapEnvelope(const QByteArray& body);

// Request builders
QByteArray findUnreadMailEnvelope(int maxItems);
QByteArray findUnreadMailInFoldersEnvelope(const QStringList& folderIds, int maxItems);
QByteArray findInboxSubfoldersEnvelope();
QByteArray findCalendarEnvelope(const QDateTime& startUtc, const QDateTime& endUtc);
QByteArray resolveNamesEnvelope(const QString& nameOrEmail);
QByteArray getFolderInboxEnvelope();
QByteArray getItemBodyEnvelope(const QStringList& itemIds);

// Response parsers — all accept raw SOAP XML response bytes.
struct ResolvedUser {
    QString displayName;
    QString email;
};

// Returns true when the top-level ResponseCode is "NoError" (no SOAP Fault).
// On failure, sets errorMessage to faultstring or ResponseCode value.
bool isSoapResponseOk(const QByteArray& xml, QString* errorMessage = nullptr);

// Parses a FindItem response for mail items. accountEmail is stored in each resource.
QVector<OutlookMailResource> parseMailFindItemResponse(const QByteArray& xml,
                                                       const QString& accountEmail);

// Parses a FindFolder response, returning folder IDs.
QStringList parseFindFolderResponse(const QByteArray& xml);

// Parses a FindItem response for calendar items.
QVector<OutlookCalendarEventResource> parseCalendarFindItemResponse(const QByteArray& xml);

// Parses a ResolveNames response. Returns nullopt on failure or no match.
std::optional<ResolvedUser> parseResolveNamesResponse(const QByteArray& xml);

// Parses a GetItem response, returning a map of ItemId -> body text preview.
QHash<QString, QString> parseGetItemBodyResponse(const QByteArray& xml);

}  // namespace OutlookEws
