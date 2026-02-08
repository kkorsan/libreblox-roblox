#pragma once

namespace RBX {
void initAntiMemDump();
void initLuaReadOnly();
void clearLuaReadOnly();
void initHwbpVeh();
}

bool isSecure();