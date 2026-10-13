# Copilot Instructions

Les instructions du projet sont dans `AGENTS.md` à la racine du dépôt : les lire et les suivre.

Règle critique : jamais de `#include` dans les `.h`, tout passe par les PCH (`pch.h`, `RenderLib/Core/include/Core.h`, `stdafx.h`).
