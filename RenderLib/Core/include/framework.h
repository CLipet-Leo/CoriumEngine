#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclure les en-têtes Windows rarement utilisés

// Fichiers d'en-tête Windows
#include <windows.h>

// Fichiers d'en-tête C RunTime
#include <string>
#include <list>
#include <vector>
#include <stdexcept>
#include <cassert>

// Fichiers d'en-tête WRL (Windows Runtime Library) pour les smart pointers COM
#include <wrl.h>
#include <wrl/client.h>
