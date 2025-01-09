#include "StdAfx.h"
#include "VRRenderUtils.h"

#include "VRManager.h"
#include "VRRenderer.h"
#include "Shaders/generated/ShaderDrawTexturePS.h"
#include "Shaders/generated/ShaderFullScreenTriVS.h"

namespace
{
	VRRenderUtils s_VRRenderUtils;
}

VRRenderUtils *gVRRenderUtils = &s_VRRenderUtils;

D3D10StateGuard::D3D10StateGuard(ID3D10Device* dvc)
{
	device = dvc;
	device->VSGetShader(vertexShader.ReleaseAndGetAddressOf());
	device->PSGetShader(pixelShader.ReleaseAndGetAddressOf());
	device->IAGetInputLayout(inputLayout.ReleaseAndGetAddressOf());
	device->IAGetPrimitiveTopology(&topology);
	device->IAGetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, vertexBuffers, strides, offsets);
	device->IAGetIndexBuffer(indexBuffer.ReleaseAndGetAddressOf(), &format, &offset);
	device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargets, depthStencil.GetAddressOf());
	device->RSGetState(rasterizerState.ReleaseAndGetAddressOf() );
	device->OMGetDepthStencilState(depthStencilState.ReleaseAndGetAddressOf(), &stencilRef);
	device->RSGetViewports(&numViewports, nullptr);
	device->RSGetViewports(&numViewports, viewports);
	device->VSGetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
	device->PSGetConstantBuffers(0, 1, psConstantBuffer.GetAddressOf());
}

D3D10StateGuard::~D3D10StateGuard()
{
	device->VSSetShader(vertexShader.Get());
	device->PSSetShader(pixelShader.Get());
	device->IASetInputLayout(inputLayout.Get());
	device->IASetPrimitiveTopology(topology );
	device->IASetVertexBuffers(0, D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, vertexBuffers, strides, offsets);
	for (auto& vertexBuffer : vertexBuffers)
	{
		if (vertexBuffer)
			vertexBuffer->Release();
	}
	device->IASetIndexBuffer(indexBuffer.Get(), format, offset);
	device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargets, depthStencil.Get());
	for (auto& renderTarget : renderTargets)
	{
		if (renderTarget) {
			renderTarget->Release();
		}
	}
	device->RSSetState(rasterizerState.Get());
	device->OMSetDepthStencilState(depthStencilState.Get(), stencilRef);
	device->RSSetViewports(numViewports, viewports);
	device->VSSetConstantBuffers(0, 1, vsConstantBuffer.GetAddressOf());
	device->PSSetConstantBuffers(0, 1, psConstantBuffer.GetAddressOf());
}

VRRenderUtils::~VRRenderUtils()
{
	Shutdown();
}

void VRRenderUtils::Init(ID3D10Device* device)
{
	m_device = device;
	CHECK_D3D10(m_device->CreateVertexShader(g_ShaderFullScreenTriVS, sizeof(g_ShaderFullScreenTriVS), m_fullScreenTriVertexShader.ReleaseAndGetAddressOf()));
	CHECK_D3D10(m_device->CreatePixelShader(g_ShaderDrawTexturePS, sizeof(g_ShaderDrawTexturePS), m_drawTexturePixelShader.ReleaseAndGetAddressOf()));

	D3D10_SAMPLER_DESC sd;
	sd.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
	sd.MipLODBias = 0;
	sd.MaxAnisotropy = 1;
	sd.ComparisonFunc = D3D10_COMPARISON_ALWAYS;
	sd.MinLOD = 0;
	sd.MaxLOD = 0;
	CHECK_D3D10(m_device->CreateSamplerState(&sd, m_sampler.ReleaseAndGetAddressOf()));

	D3D10_RASTERIZER_DESC rd;
	rd.FillMode = D3D10_FILL_SOLID;
	rd.CullMode = D3D10_CULL_NONE;
	rd.FrontCounterClockwise = TRUE;
	rd.DepthBias = 0;
	rd.DepthBiasClamp = 0;
	rd.SlopeScaledDepthBias = 0;
	rd.DepthClipEnable = FALSE;
	rd.ScissorEnable = FALSE;
	rd.MultisampleEnable = FALSE;
	rd.AntialiasedLineEnable = FALSE;
	CHECK_D3D10(m_device->CreateRasterizerState(&rd, m_rasterizerState.ReleaseAndGetAddressOf()));

	D3D10_BLEND_DESC bd {};
	bd.AlphaToCoverageEnable = FALSE;
	bd.BlendEnable[0] = TRUE;
	bd.BlendOp = D3D10_BLEND_OP_ADD;
	bd.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	bd.SrcBlend = D3D10_BLEND_INV_DEST_ALPHA;
	bd.DestBlend = D3D10_BLEND_DEST_ALPHA;
	bd.SrcBlendAlpha = D3D10_BLEND_ZERO;
	bd.DestBlendAlpha = D3D10_BLEND_ONE;
	bd.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_RED|D3D10_COLOR_WRITE_ENABLE_GREEN|D3D10_COLOR_WRITE_ENABLE_BLUE;
	CHECK_D3D10(m_device->CreateBlendState(&bd, m_srcAlphaBlend.ReleaseAndGetAddressOf()));

	D3D10_DEPTH_STENCIL_DESC dsd {};
	dsd.DepthEnable = FALSE;
	dsd.StencilEnable = FALSE;
	CHECK_D3D10(m_device->CreateDepthStencilState(&dsd, m_disableDepth.ReleaseAndGetAddressOf()));
}

void VRRenderUtils::Shutdown()
{
	m_disableDepth.Reset();
	m_srcAlphaBlend.Reset();
	m_rasterizerState.Reset();
	m_sampler.Reset();
	m_drawTexturePixelShader.Reset();
	m_fullScreenTriVertexShader.Reset();
	m_device.Reset();
}

void VRRenderUtils::CopyEyeToScreenMirror(ID3D10ShaderResourceView* eyeTexture)
{
	if (eyeTexture == nullptr)
		return;

	D3D10StateGuard stateGuard(m_device.Get());

	// figure out aspect ratio correction
	Vec2i windowSize = gVRRenderer->GetWindowSize();
	float windowAspect = (float)windowSize.x / windowSize.y;
	Vec2i renderSize = gVR->GetRenderSize();
	float vrAspect = (float)renderSize.x / renderSize.y;
	float scale = vrAspect / windowAspect;
	Vec2 size;
	if (scale < 1.f)
	{
		// mirror view is wider than rendered eye
		size.x = 1.f;
		size.y = 1.f / scale;
	}
	else
	{
		// rendered eye is wider than mirror view
		size.x = scale;
		size.y = 1.f;
	}

	D3D10_VIEWPORT vp;
	vp.TopLeftX = (1.f - size.x) * .5f * renderSize.x;
	vp.TopLeftY = (1.f - size.y) * .5f * renderSize.y;
	vp.Width = size.x * renderSize.x;
	vp.Height = size.y * renderSize.y;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	m_device->RSSetViewports(1, &vp);
	D3D10_RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = renderSize.x;
	scissor.bottom = renderSize.y;
	m_device->RSSetScissorRects(1, &scissor);

	m_device->OMSetBlendState(m_srcAlphaBlend.Get(), nullptr, 0xffffffff);
	m_device->OMSetDepthStencilState(m_disableDepth.Get(), 0);
	m_device->VSSetShader(m_fullScreenTriVertexShader.Get());
	m_device->PSSetShader(m_drawTexturePixelShader.Get());
	m_device->PSSetShaderResources(0, 1, &eyeTexture);
	m_device->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
	m_device->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	m_device->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	m_device->IASetInputLayout(nullptr);
	m_device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_device->RSSetState(m_rasterizerState.Get());

	m_device->Draw(3, 0);
}
