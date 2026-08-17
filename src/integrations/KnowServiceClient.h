#pragma once

#include <functional>

#include <QObject>

#include "integrations/KnowServiceContracts.h"
#include "integrations/KnowledgeServiceSettings.h"

class QNetworkAccessManager;

class KnowServiceClient : public QObject {
public:
    explicit KnowServiceClient(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~KnowServiceClient() override = default;

    virtual void testConnection(const KnowledgeServiceConfig& config,
                                std::function<void(KnowServiceQueryResponse)> done) = 0;
    virtual void query(const KnowledgeServiceConfig& config,
                       const QString& text,
                       std::function<void(KnowServiceQueryResponse)> done) = 0;
};

class HttpKnowServiceClient final : public KnowServiceClient {
public:
    explicit HttpKnowServiceClient(QObject* parent = nullptr);

    void testConnection(const KnowledgeServiceConfig& config,
                        std::function<void(KnowServiceQueryResponse)> done) override;
    void query(const KnowledgeServiceConfig& config,
               const QString& text,
               std::function<void(KnowServiceQueryResponse)> done) override;

private:
    QNetworkAccessManager* m_network = nullptr;
};
