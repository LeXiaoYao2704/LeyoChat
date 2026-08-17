#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct KnowServiceSearchResult {
    QString sourceId;
    QString title;
    QString snippet;
    int score = 0;
};

struct KnowServiceSource {
    QString sourceId;
    QString title;
    QString openUri;
    QString originalUri;
    QString sourceType;
    double score = 0.0;
    QString searchMode;
};

struct KnowServiceAnswer {
    QString summary;
    QStringList citations;
};

struct KnowServiceServiceMeta {
    QString serviceInstanceId;
    QString knowledgeBaseId;
    QString apiVersion;
    QStringList capabilities;
};

struct KnowServiceQueryResponse {
    KnowServiceServiceMeta service;
    QString freshnessState;
    QString degradeMode;
    QString knowledgeLayer;
    qint64 lastSuccessfulMaintenanceAtMs = 0;
    bool maintenanceRunning = false;
    QVector<KnowServiceSearchResult> results;
    QVector<KnowServiceSource> sources;
    KnowServiceAnswer answer;
    QString errorMessage;
};
