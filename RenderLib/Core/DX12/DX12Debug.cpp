#include "pch.h"
#include "DX12Debug.h"

DX12Debug::DX12Debug()
{
	EVAL_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug)), "D3D12 Debug Interface creation failed");
	m_debug->EnableDebugLayer();
}
