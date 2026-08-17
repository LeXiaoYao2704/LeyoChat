#include "integrations/KnowServiceClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

QString normalizedBaseUrl(const QString& baseUrl)
{
    QString normalized = baseUrl.trimmed();
    while (normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized;
}

QUrl serviceUrl(const KnowledgeServiceConfig& config, const QString& path)
{
    return QUrl(QStringLiteral("%1%2").arg(normalizedBaseUrl(config.baseUrl), path));
}

QNetworkRequest buildJsonRequest(const KnowledgeServiceConfig& config, const QString& path)
{
    QNetworkRequest request(serviceUrl(config, path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!config.accessToken.trimmed().isEmpty()) {
        request.setRawHeader("Authorization",
                             QStringLiteral("Bearer %1").arg(config.accessToken.trimmed()).toUtf8());
    }
    return request;
}

QStringList toStringList(const QJsonArray& array)
{
    QStringList values;
    for (const QJsonValue& value : array) {
        const QString item = value.toString().trimmed();
        if (!item.isEmpty()) {
            values.push_back(item);
        }
    }
    return values;
}

KnowServiceServiceMeta parseServiceMeta(const QJsonObject& root)
{
    const QJsonObject serviceObject = root.value(QStringLiteral("service")).isObject()
        ? root.value(QStringLiteral("service")).toObject()
        : root;

    KnowServiceServiceMeta service;
    service.serviceInstanceId = serviceObject.value(QStringLiteral("serviceInstanceId")).toString().trimmed();
    service.knowledgeBaseId = serviceObject.value(QStringLiteral("knowledgeBaseId")).toString().trimmed();
    service.apiVersion = serviceObject.value(QStringLiteral("apiVersion")).toString().trimmed();
    service.capabilities = toStringList(serviceObject.value(QStringLiteral("capabilities")).toArray());
    return service;
}

QVector<KnowServiceSearchResult> parseResults(const QJsonArray& array)
{
    QVector<KnowServiceSearchResult> results;
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        KnowServiceSearchResult result;
        result.sourceId = object.value(QStringLiteral("sourceId")).toString().trimmed();
        result.title = object.value(QStringLiteral("title")).toString().trimmed();
        result.snippet = object.value(QStringLiteral("snippet")).toString().trimmed();
        result.score = object.value(QStringLiteral("score")).toInt();
        results.push_back(result);
    }
    return results;
}

QVector<KnowServiceSource> parseSources(const QJsonArray& array)
{
    QVector<KnowServiceSource> sources;
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        KnowServiceSource source;
        source.sourceId = object.value(QStringLiteral("sourceId")).toString().trimmed();
        source.title = object.value(QStringLiteral("title")).toString().trimmed();
        source.openUri = object.value(QStringLiteral("openUri")).toString().trimmed();
        source.originalUri = object.value(QStringLiteral("originalUri")).toString().trimmed();
        source.sourceType = object.value(QStringLiteral("sourceType")).toString().trimmed();
        source.score = object.value(QStringLiteral("score")).toDouble(0.0);
        source.searchMode = object.value(QStringLiteral("searchMode")).toString().trimmed();
        sources.push_back(source);
    }
    return sources;
}

KnowServiceAnswer parseAnswer(const QJsonObject& object)
{
    KnowServiceAnswer answer;
    answer.summary = object.value(QStringLiteral("summary")).toString().trimmed();
    answer.citations = toStringList(object.value(QStringLiteral("citations")).toArray());
    return answer;
}

KnowServiceQueryResponse parseQueryResponse(const QJsonObject& object)
{
    KnowServiceQueryResponse response;
    response.service = parseServiceMeta(object);
    response.freshnessState = object.value(QStringLiteral("freshnessState")).toString().trimmed();
    response.degradeMode = object.value(QStringLiteral("degradeMode")).toString().trimmed();
    response.knowledgeLayer = object.value(QStringLiteral("knowledgeLayer")).toString().trimmed();
    response.lastSuccessfulMaintenanceAtMs = static_cast<qint64>(
        object.value(QStringLiteral("lastSuccessfulMaintenanceAtMs")).toDouble());
    response.maintenanceRunning = object.value(QStringLiteral("maintenanceRunning")).toBool(false);
    response.results = parseResults(object.value(QStringLiteral("results")).toArray());
    response.sources = parseSources(object.value(QStringLiteral("sources")).toArray());
    response.answer = parseAnswer(object.value(QStringLiteral("answer")).toObject());
    return response;
}

KnowServiceQueryResponse parseHealthResponse(const QJsonObject& object)
{
    KnowServiceQueryResponse response;
    response.service = parseServiceMeta(object);
    return response;
}

template <typename Parser>
void handleReply(QNetworkReply* reply,
                 std::function<void(KnowServiceQueryResponse)> done,
                 Parser parser)
{
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done = std::move(done), parser]() mutable {
        KnowServiceQueryResponse response;
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            response.errorMessage = reply->errorString();
            done(std::move(response));
            reply->deleteLater();
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            response.errorMessage = QStringLiteral("KnowService 返回了无法解析的 JSON");
            done(std::move(response));
            reply->deleteLater();
            return;
        }

        response = parser(document.object());
        done(std::move(response));
        reply->deleteLater();
    });
}

}

HttpKnowServiceClient::HttpKnowServiceClient(QObject* parent)
    : KnowServiceClient(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setProxy(QNetworkProxy::NoProxy);
}

void HttpKnowServiceClient::testConnection(const KnowledgeServiceConfig& config,
                                           std::function<void(KnowServiceQueryResponse)> done)
{
    QNetworkReply* reply = m_network->get(buildJsonRequest(config, QStringLiteral("/health")));
    handleReply(reply, std::move(done), [](const QJsonObject& object) {
        return parseHealthResponse(object);
    });
}

void HttpKnowServiceClient::query(const KnowledgeServiceConfig& config,
                                  const QString& text,
                                  std::function<void(KnowServiceQueryResponse)> done)
{
    const QJsonObject body{{QStringLiteral("text"), text}};
    QNetworkReply* reply = m_network->post(
        buildJsonRequest(config, QStringLiteral("/v1/query")),
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    handleReply(reply, std::move(done), [](const QJsonObject& object) {
        return parseQueryResponse(object);
    });
}
