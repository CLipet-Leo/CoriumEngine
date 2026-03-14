#pragma once

class CORIUM_API DX12Debug
{
	public:
		DX12Debug();
		virtual ~DX12Debug() = default;

	private:
		Microsoft::WRL::ComPtr<ID3D12Debug> m_debug;
};

