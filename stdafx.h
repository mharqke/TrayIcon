// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef _WIN32_IE           // Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600
#endif

#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers

// Windows Header Files:
#include <windows.h>
#include <Windowsx.h>
#include <commctrl.h>
#include <Shellapi.h>
#include <Shlwapi.h>
#include <tchar.h>
#include <strsafe.h>


// C RunTime Header Files
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include <string>
#include <string_view>
#include <cassert>
#include <memory>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <list>
#include <regex>
#include <ranges>
#include <optional>