#pragma once

#include <string>
#include <vector>

#include <QString>
#include <QVector>

#include "domain/ChatMessage.h"
#include "domain/PeerEndpoint.h"

QString toQString(const std::wstring& value);
std::string toUtf8(const QString& value);
QVector<PeerEndpoint> toPeerVector(const std::vector<PeerEndpoint>& peers);
std::vector<ChatMessage> toMessageVector(const std::vector<ChatMessage>& messages);
QString endpointKey(const QString& host, quint16 port);
QString normalizeHost(const QString& host);
QString displayNameForPeer(const PeerEndpoint& peer);
QString presenceTextForPeer(const PeerEndpoint& peer);
