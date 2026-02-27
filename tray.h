#pragma once
#include "common.h"

namespace Tray {
    void Create(HWND hWnd);
    void Destroy();
    void Update(bool locked);
}
