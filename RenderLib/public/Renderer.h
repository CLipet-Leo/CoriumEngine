#pragma once

class CORIUM_API IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Init(HWND hWnd, uint32_t width, uint32_t height) = 0;
    virtual void OnResize(uint32_t w, uint32_t h) = 0;
    virtual void Render() = 0;
    virtual void Shutdown() = 0;
};
