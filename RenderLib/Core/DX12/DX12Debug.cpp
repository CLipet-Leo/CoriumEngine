#include "pch.h"
#include "DX12Debug.h"

DX12Debug::DX12Debug()
{
    Microsoft::WRL::ComPtr<ID3D12Debug> debug0;
    EVAL_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&debug0)),
        "D3D12 Debug Interface creation failed");

    // Upgrade vers Debug1 pour la GPU-Based Validation
    EVAL_HR(debug0.As(&m_debug), "Debug1 interface query failed");

    m_debug->EnableDebugLayer();
    m_debug->SetEnableGPUBasedValidation(TRUE);   // Détecte les erreurs côté GPU
    m_debug->SetEnableSynchronizedCommandQueueValidation(TRUE);
}

void DX12Debug::SetupInfoQueue(ID3D12Device* device)
{
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
        return; // Non disponible sur ce GPU, pas critique

    // Breakpoint automatique sur corruption et erreur — tu ne rateras plus rien
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

    // Filtrer les messages verbeux mais inoffensifs
    D3D12_MESSAGE_ID denyIds[] = {
        // Faux positif courant sur certains drivers Intel/AMD
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
        // Warnings de resource state attendus lors du resize
        D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
        D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
    };

    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumIDs = _countof(denyIds);
    filter.DenyList.pIDList = denyIds;
    infoQueue->PushStorageFilter(&filter);
}
