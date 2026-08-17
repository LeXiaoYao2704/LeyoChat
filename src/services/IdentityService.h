#pragma once

#include <memory>

#include <QString>

#include "domain/Profile.h"

class ProfileRepository;

struct ValidationResult {
    bool isValid = false;
    QString errorMessage;
};

class IdentityService {
public:
    explicit IdentityService(ProfileRepository* repository = nullptr);

    ValidationResult validateInput(const QString& displayName, const QString& employeeCode) const;
    std::unique_ptr<Profile> loadProfile() const;
    Profile createProfile(const QString& displayName, const QString& employeeCode, quint16 listenPort) const;
    bool saveProfile(const Profile& profile) const;

private:
    QString generateClientId() const;

    ProfileRepository* m_repository = nullptr;
};
