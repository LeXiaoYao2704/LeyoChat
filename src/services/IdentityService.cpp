#include "services/IdentityService.h"

#include "storage/ProfileRepository.h"

#include <QUuid>

IdentityService::IdentityService(ProfileRepository* repository)
    : m_repository(repository) {}

ValidationResult IdentityService::validateInput(const QString& displayName, const QString& employeeCode) const {
    if (displayName.trimmed().isEmpty()) {
        return {false, QStringLiteral("\u6635\u79F0\u4E0D\u80FD\u4E3A\u7A7A")};
    }
    if (employeeCode.trimmed().isEmpty()) {
        return {false, QStringLiteral("\u5DE5\u53F7\u4E0D\u80FD\u4E3A\u7A7A")};
    }
    return {true, {}};
}

std::unique_ptr<Profile> IdentityService::loadProfile() const {
    if (!m_repository) {
        return nullptr;
    }

    return m_repository->loadProfile();
}

Profile IdentityService::createProfile(const QString& displayName, const QString& employeeCode, quint16 listenPort) const {
    return Profile{
        generateClientId().toStdWString(),
        displayName.trimmed().toStdWString(),
        employeeCode.trimmed().toStdWString(),
        L"",
        listenPort,
        L"",
        L"",
        L"",
        L"",
        L""
    };
}

bool IdentityService::saveProfile(const Profile& profile) const {
    return m_repository && m_repository->saveProfile(profile);
}

QString IdentityService::generateClientId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
