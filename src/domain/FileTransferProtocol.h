#pragma once

#include <string>
#include <vector>

#include <QtGlobal>

enum class FileControlType {
    Offer,
    Accept,
    Reject,
    ResumeRequest,
    ResumeResponse,
    Progress,
    Complete,
    Fail,
    Cancel
};

struct FileControlPayload {
    FileControlType type = FileControlType::Offer;
    std::string taskId;
    std::string conversationId;
    std::string groupId;
    std::string senderId;
    std::string targetId;
    std::string fileName;
    std::string fileHash;
    std::string dataHost;
    std::string reason;
    qint64 fileSize = 0;
    qint64 chunkSize = 0;
    int chunkCount = 0;
    quint16 dataPort = 0;
    std::vector<int> completedChunks;
};
