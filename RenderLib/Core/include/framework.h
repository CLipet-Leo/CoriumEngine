#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclure les en-têtes Windows rarement utilisés

// Fichiers d'en-tête Windows
#include <windows.h>

// Fichiers d'en-tête C RunTime
#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <cassert>
#include <iostream>

// Fichiers d'en-tête COM (messages d'erreur HRESULT)
#include <comdef.h>
#include <comutil.h>

// Fichiers d'en-tête WRL (Windows Runtime Library) pour les smart pointers COM
#include <wrl.h>
#include <wrl/client.h>
