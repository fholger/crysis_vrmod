#pragma once

#include <d3d10_1.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#define CHECK_D3D10(call) { HRESULT hr = call; if (FAILED(hr)) CryLogAlways("D3D10 call failed at %s:%d\n", __FILE__, __LINE__); }

class D3D10StateGuard
{
public:
    explicit D3D10StateGuard(ID3D10Device* device);
    ~D3D10StateGuard();

private:
    ComPtr<ID3D10Device> device;
	ComPtr<ID3D10VertexShader> vertexShader;
	ComPtr<ID3D10PixelShader> pixelShader;
	ComPtr<ID3D10InputLayout> inputLayout;
	D3D10_PRIMITIVE_TOPOLOGY topology;
	ID3D10Buffer *vertexBuffers[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT strides[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT offsets[D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	ComPtr<ID3D10Buffer> indexBuffer;
	DXGI_FORMAT format;
	UINT offset;
	ID3D10RenderTargetView *renderTargets[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ComPtr<ID3D10DepthStencilView> depthStencil;
	ComPtr<ID3D10RasterizerState> rasterizerState;
	ComPtr<ID3D10DepthStencilState> depthStencilState;
	UINT stencilRef;
	D3D10_VIEWPORT viewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	UINT numViewports = 0;
	ComPtr<ID3D10Buffer> vsConstantBuffer;
	ComPtr<ID3D10Buffer> psConstantBuffer;
};

class VRRenderUtils
{
public:
    ~VRRenderUtils();

    void Init(ID3D10Device* device);
    void Shutdown();
    void CopyEyeToScreenMirror(ID3D10ShaderResourceView* eyeTexture);

private:
    ComPtr<ID3D10Device> m_device;
    ComPtr<ID3D10VertexShader> m_fullScreenTriVertexShader;
    ComPtr<ID3D10PixelShader> m_drawTexturePixelShader;
    ComPtr<ID3D10SamplerState> m_sampler;
    ComPtr<ID3D10RasterizerState> m_rasterizerState;
    ComPtr<ID3D10BlendState> m_srcAlphaBlend;
    ComPtr<ID3D10BlendState> m_NoBlend;
    ComPtr<ID3D10DepthStencilState> m_disableDepth;
};

extern VRRenderUtils* gVRRenderUtils;
