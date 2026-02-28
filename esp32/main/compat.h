#ifndef COMPAT_H
#define COMPAT_H

// Force-included via -include by CMakeLists.txt for all game source files.

#define ESP32_BADGE 1

#ifndef __ASSEMBLER__
#include <strings.h>
#define strcmpi strcasecmp
#define strnicmp strncasecmp
#endif

#endif
