#pragma once
#include "common.h"

namespace Security {
    bool SavePassword(const std::wstring& password);
    bool VerifyPassword(const std::wstring& input);
    std::wstring HashPassword(const std::wstring& password);
}
