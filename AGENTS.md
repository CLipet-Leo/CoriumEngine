# CoriumEngine — instructions pour agents IA

Source unique pour tous les outils (Claude Code, Copilot, Codex, Cursor, Junie…).
Tenir ce fichier à jour plutôt que des mémoires propres à un outil.

## Projet

Moteur 3D C++ / DirectX 12, Windows x64 uniquement.

- `RenderLib/` : DLL du moteur. Export via `CORIUM_API` (`RENDERER_EXPORTS` défini dans la DLL).
- `CoriumEngine/` : exe Win32 (boucle, fenêtre, scène de démo dans `Main.cpp`).

## Build

```bash
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" CoriumEngine.sln -p:Configuration=Debug_DX12 -p:Platform=x64 -m -nologo -v:minimal
```

Configs : `Debug_DX12`, `Release`. Sortie : `Build/<Config>/`. Les shaders (`RenderLib/Shaders`)
et `Assets/` (scènes, cf. `doc/SceneFormat.md`) sont copiés à côté de l'exe au build
(`CopyFileToFolders`) ; les ressources se résolvent via `GetExecutableDir()` (Helper.h), jamais
relativement au dossier courant.

## Conventions

- **Aucun `#include` dans les `.h`.** Tout passe par les PCH : `pch.h` / `Core/include/Core.h`
  (RenderLib) et `stdafx.h` (exe, qui inclut Core.h). Nouveau header → l'ajouter dans Core.h.
- Spécialisations explicites de templates utilisées hors de la DLL : `inline` dans le header
  (sinon non exportées → LNK2019).
- Erreurs DX12 : `EVAL_HR(hr, msg)` (lève `DX12Exception`).
- Nouveaux fichiers source : les ajouter au `.vcxproj` **et** au `.vcxproj.filters`.
- Fins de ligne : voir `.gitattributes` (.h/.cpp en LF, .sln/.vcxproj en CRLF).
  `Win32Application.h/.cpp` sont encodés cp1252 : ne pas les réécrire en UTF-8.
- Commentaires de code en français.
- Repère, matrices, angles, profondeur, layout GPU : voir `doc/Conventions.md` (à respecter et à
  tenir à jour).

## Architecture

- API publique : `IRenderer` (`RenderLib/Core/public/Renderer.h`), créé par `CreateRenderer()`.
- La `Scene` (ECS, `RenderLib/Scene`) appartient à l'application. Boucle (`Win32Application::Run`) :
  `Clock::Tick` → `EditorCamera::Update(dt, input)` → `Input::EndFrame` →
  `TransformSystem::Update(scene)` → `IRenderer::Render(scene, &editorView)`.
- `Input` (état brut, Raw Input) est alimenté par le WndProc de l'exe, sans dépendance à ImGui ;
  le filtrage UI passe par `IRenderer::WantCaptureMouse`.
- Entités : `EntityID` runtime (clé des `ComponentMap`, jamais écrit) + `uuid` 64 bits persistant.
  Scènes chargées/sauvées par `SceneSerializer`.
- Le renderer possède Camera/Light/Render systems, les registres mesh/matériaux et l'UI
  éditeur ImGui (`RenderLib/Editor/EditorUI`).

## Git

- Ne jamais `git push` : commit uniquement sur demande, l'utilisateur pousse.
- Aucun trailer d'attribution IA (`Co-Authored-By: <IA>`, "Generated with…") dans les commits/PR.
- Messages au format conventionnel en anglais : `fix: …`, `refactor: …`, `feat: …`.

## Feuille de route

Voir `doc/Roadmap.md` (à tenir à jour quand un chantier avance ou change de priorité).
