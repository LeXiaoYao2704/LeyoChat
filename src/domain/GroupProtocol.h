#pragma once

#include <string>
#include <vector>

enum class GroupEventType {
    Create,
    Rename,
    AddMembers,
    RemoveMember,
    SetAdmin,
    UnsetAdmin
};

enum class GroupMessageKind {
    Text,
    SystemNotice,
    FilePlaceholder
};
