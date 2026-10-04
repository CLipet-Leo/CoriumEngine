#include "stdafx.h"
#include "public/Win32Application.h"
// Memory leak detection
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

// Contenu de démo : caméra, soleil, un cube plein et un cube filaire
static void LoadDemoScene(IRenderer& renderer, Scene& scene)
{
    BoxGeometry cube(1.f);
    uint32_t cubeIdx = renderer.RegisterMesh("cube", cube);

    Entity cam = scene.CreateEntity("Main Camera");
    scene.GetComponent<TransformComponent>(cam.id)->SetPosition(0.f, 1.5f, -4.f);
    auto& camC = scene.AddComponent<CameraComponent>(cam.id);
    camC.isMain = true;
    camC.fovY = 60.f;

    Entity sun = scene.CreateEntity("Sun");
    scene.GetComponent<TransformComponent>(sun.id)->rotation = { 45.f, 30.f, 0.f };
    auto& sunL = scene.AddComponent<LightComponent>(sun.id);
    sunL.type = LightType::Directional;
    sunL.intensity = 1.2f;

    Entity cubeEnt = scene.CreateEntity("Cube");
    scene.AddComponent<MeshComponent>(cubeEnt.id).meshIndex = cubeIdx;

    MaterialDesc wireDesc;
    wireDesc.name = "Wireframe";
    wireDesc.baseColor = { 0.3f, 1.f, 0.4f, 1.f };
    wireDesc.fillMode = FillMode::Wireframe;
    wireDesc.cullMode = CullMode::None;
    uint32_t wireMat = renderer.CreateMaterial(wireDesc);

    Entity wireEnt = scene.CreateEntity("Wire Cube");
    scene.GetComponent<TransformComponent>(wireEnt.id)->SetPosition(2.f, 0.f, 0.f);
    auto& wireM = scene.AddComponent<MeshComponent>(wireEnt.id);
    wireM.meshIndex = cubeIdx;
    wireM.materialIndex = wireMat;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow)
{
    const uint32_t initialWidth = 800;
    const uint32_t initialHeight = 600;

    // Scope : app (et sa Scene) doit être détruite avant le dump des fuites
    {
        Win32Application app;

        if (!app.Initialize(hInstance, nCmdShow, initialWidth, initialHeight))
            return -1;

        LoadDemoScene(app.GetRenderer(), app.GetScene());
        app.Run();

        app.Shutdown();
    }

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtDumpMemoryLeaks();

    return 0;
}
