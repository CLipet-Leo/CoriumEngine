#pragma once
#include <stdexcept>
#include <sstream>
#include <comdef.h>
#include <comutil.h>

// Exception typée avec HRESULT, fichier et ligne
class DX12Exception : public std::runtime_error
{
public:
    DX12Exception(HRESULT hr, const char* msg, const char* file, int line)
        : std::runtime_error(BuildMessage(hr, msg, file, line))
        , m_hr(hr)
    {}

    HRESULT GetHResult() const { return m_hr; }

private:
    HRESULT m_hr;

    static std::string BuildMessage(HRESULT hr, const char* msg, const char* file, int line)
    {
        _com_error err(hr);
        _bstr_t bstrMsg(err.ErrorMessage());
        const char* details = static_cast<const char*>(bstrMsg);

        std::ostringstream oss;
        oss << "[DX12 Error] " << msg << "\n"
            << "  HRESULT : 0x" << std::hex << static_cast<unsigned long>(hr) << "\n"
            << "  Details : " << (details ? details : "Unknown COM error") << "\n"
            << "  File    : " << file << ":" << std::dec << line;
        return oss.str();
    }
};

// Throw sur erreur — fini les erreurs silencieuses
#define EVAL_HR(hr, msg) \
    do { HRESULT _hr = (hr); \
         if (FAILED(_hr)) throw DX12Exception(_hr, msg, __FILE__, __LINE__); \
    } while(0)

// Assert debug uniquement
#ifdef _DEBUG
#define CE_ASSERT(exp) \
    do { if (!(exp)) { \
        OutputDebugStringA("ASSERT FAILED: " #exp "\nFile: " __FILE__ "\n"); \
        __debugbreak(); \
    }} while(0)
#else
#define CE_ASSERT(exp) ((void)(exp))
#endif

#define PRINT_W_N(msg) std::wcout << msg << std::endl
#define PRINT_N(msg)   std::cout  << msg << std::endl
