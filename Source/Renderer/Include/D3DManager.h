//	DX11Renderer - VDemo | DirectX11 Renderer
//	Copyright(C) 2016  - Volkan Ilbeyli
//
//	This program is free software : you can redistribute it and / or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.If not, see <http://www.gnu.org/licenses/>.
//
//	Contact: volkanilbeyli@gmail.com

#pragma once

// DX11 library linking
#pragma comment(lib, "d3d11.lib")		// contains all the Direct3D functionality
#pragma comment(lib, "dxgi.lib")		// tools to interface with the hardware
#pragma comment(lib, "d3dcompiler.lib")	// functionality for compiling shaders

#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

// auto convert for enum classes: https://stackoverflow.com/questions/8357240/how-to-automatically-convert-strongly-typed-enum-into-int/8357462#8357462
template<typename E>
constexpr auto to_underlying(E e) noexcept
{
	return static_cast<typename std::underlying_type_t<E>>(e);
}

enum class RS_CULL_MODE 
{
	FRONT = D3D11_CULL_FRONT,
	NONE = D3D11_CULL_NONE,
	BACK = D3D11_CULL_BACK
};

enum class RS_FILL_MODE
{
	SOLID     = D3D11_FILL_SOLID,
	WIREFRAME = D3D11_FILL_WIREFRAME,
};

enum class TOPOLOGY
{
	POINT_LIST     = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
	TRIANGLE_LIST  = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	LINE_LIST      = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,

	TOPOLOGY_COUNT
};

class D3DManager
{
	friend class Renderer;

public:
	D3DManager();
	~D3DManager();

	bool Init(int width, int height, const bool VSYNC, HWND m_hwnd, const bool FULL_SCREEN);
	void Shutdown();

	void BeginFrame(const float* clearColor);
	void EndFrame();

	void EnableAlphaBlending(bool enable);
	void EnableZBuffer(bool enable);

	ID3D11Device*			GetDevice()			{ return m_device; }
	ID3D11DeviceContext*	GetDeviceContext()	{ return m_deviceContext;}

	void GetVideoCardInfo(char*, int&);
	float AspectRatio() const;
	unsigned WindowWidth() const;
	unsigned WindowHeight() const;

private:
	bool InitSwapChain(HWND hwnd, bool fullscreen, int scrWidth, int scrHeight, unsigned numerator, unsigned denominator);
	bool InitDepthBuffer(int scrWidth, int scrHeight);
	bool InitDepthStencilBuffer();
	bool InitRasterizerState();
	bool InitStencilView();
	void InitViewport(int scrWidth, int scrHeight);
	bool InitAlphaBlending();
	bool InitZBuffer();

private:
	bool						m_vsync_enabled;
	int							m_VRAM;
	char						m_GPUDescription[128];
	IDXGISwapChain*				m_swapChain;
	ID3D11Device*				m_device;
	ID3D11DeviceContext*		m_deviceContext;
	ID3D11RenderTargetView*		m_RTV;
	ID3D11Texture2D*			m_depthStencilBuffer;
	ID3D11DepthStencilView*		m_depthStencilView;
	unsigned					m_wndWidth, m_wndHeight;
	
	//-----------------------------------------------
	// State Management
	ID3D11RasterizerState*		m_rasterState;
	ID3D11DepthStencilState*	m_depthStencilState;
	ID3D11BlendState*			m_alphaEnableBlendState;
	ID3D11BlendState*			m_alphaDisableBlendState;
	ID3D11DepthStencilState*	m_depthDisabledStencilState;
};
