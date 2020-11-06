#pragma once
#if defined(_WIN64) || defined(_WIN32)
bool win = true;
#else
bool win = false;
#endif
