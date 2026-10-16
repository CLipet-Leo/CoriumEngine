# CoriumEngine — instructions pour agents IA

Source unique pour tous les outils (Claude Code, Copilot, Codex, Cursor, Junie…).
Tenir ce fichier à jour plutôt que des mémoires propres à un outil.

## Projet

Moteur 3D C++ / DirectX 12, Windows x64 uniquement.

- `CoriumEngine/` : DLL du moteur. Export via `CORIUM_API` (`RENDERER_EXPORTS` défini dans la DLL).
- `CoriumRuntime/` : runtime (`Win32Application` telle quelle, caméra `isMain` de la scène, sans ImGui).
- `CoriumEditor/` : éditeur (`EditorApplication` dans `Main.cpp`, `EditorUI`, `EditorCamera`).
  Les deux exe chargent `Assets/Scenes/Demo.scene`.

## Build

```bash
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" CoriumEngine.sln -p:Configuration=Debug_DX12 -p:Platform=x64 -m -nologo -v:minimal
```

Configs : `Debug_DX12`, `Release`. Sortie : `Build/<Config>/`. Les shaders (`CoriumEngine/Shaders`)
et `Assets/` (scènes, cf. `doc/SceneFormat.md`) sont copiés à côté de l'exe au build
(`CopyFileToFolders`) ; les ressources se résolvent via `GetExecutableDir()` (Helper.h), jamais
relativement au dossier courant.

## Conventions

- **Aucun `#include` dans les `.h`.** Tout passe par les PCH : `pch.h` / `Core/include/Core.h`
  (DLL) et `stdafx.h` (exe, qui n'inclut que `CorePublic.h`). Nouveau header → dans
  `CorePublic.h` s'il est utile aux exe et sans type DirectX/ImGui, sinon dans `Core.h`.
- Spécialisations explicites de templates utilisées hors de la DLL : `inline` dans le header
  (sinon non exportées → LNK2019).
- Erreurs DX12 : `EVAL_HR(hr, msg)` (lève `DX12Exception`).
- Nouveaux fichiers source : les ajouter au `.vcxproj` **et** au `.vcxproj.filters`.
- Fins de ligne : voir `.gitattributes` (.h/.cpp en LF, .sln/.vcxproj en CRLF).
  `Win32Application.h/.cpp` (`CoriumEngine/Core/Platform`) sont encodés cp1252 : ne pas les réécrire
  en UTF-8.
- Commentaires de code en français.
- Repère, matrices, angles, profondeur, layout GPU : voir `doc/Conventions.md` (à respecter et à
  tenir à jour).

## Architecture

- API publique : `IRenderer` (`CoriumEngine/Core/public/Renderer.h`), créé par `CreateRenderer()`.
- Fenêtre + boucle partagées dans la DLL (`CoriumEngine/Core/Platform/Win32Application`), qui possède
  renderer, `Scene` (ECS, `CoriumEngine/Scene`), `Clock` et `Input`. Chaque exe dérive et surcharge
  `Update(dt)` et `GetView()`. Boucle (`Run`) : `Clock::Tick` → `Update(dt)` → `Input::EndFrame` →
  `TransformSystem::Update(scene)` → `IRenderer::Render(scene, GetView())`.
  Messages fenêtre interceptables par `OnWindowMessage` (backend Win32 d'ImGui dans l'éditeur).
- `Input` (état brut, Raw Input) est alimenté par le WndProc, sans dépendance à ImGui ; la caméra
  éditeur ne démarre qu'au survol de l'image du panneau Viewport.
- Entités : `EntityID` runtime (clé des `ComponentMap`, jamais écrit) + `uuid` 64 bits persistant.
  Scènes chargées/sauvées par `SceneSerializer`.
- Le renderer possède Camera/Light/Render systems et les registres mesh/matériaux.
- ImGui (lib statique vcpkg, une copie par module) : l'éditeur crée le contexte et le backend
  Win32, puis `IRenderer::InitUI` y rattache la DLL (même contexte + allocateurs de l'exe) et
  ajoute le backend DX12. Avec l'UI, la scène est rendue dans une cible offscreen
  (`DX12RenderTarget`, SRV slot 1, `GetSceneTexture`/`SetSceneSize`) et ImGui dans la swapchain.
  Frame éditeur : `BeginUIFrame` → `ImGui_ImplWin32_NewFrame` → `ImGui::NewFrame` → widgets →
  `Render` (termine la frame ImGui). Arrêt : `ShutdownUI` avant `Shutdown`.

## Git

- Ne jamais `git push` : commit uniquement sur demande, l'utilisateur pousse.
- Aucun trailer d'attribution IA (`Co-Authored-By: <IA>`, "Generated with…") dans les commits/PR.
- Messages au format conventionnel en anglais : `fix: …`, `refactor: …`, `feat: …`.

## Feuille de route

Voir `doc/Roadmap.md` (à tenir à jour quand un chantier avance ou change de priorité).
