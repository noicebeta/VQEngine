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

#include "RenderPasses.h"

#include "Light.h"

#include "Camera.h"

void ShadowMapPass::Initialize(Renderer* pRenderer, ID3D11Device* device, const Settings::ShadowMap& shadowMapSettings)
{
	this->_shadowMapDimension = static_cast<int>(shadowMapSettings.dimension);

#if _DEBUG
	pRenderer->m_Direct3D->ReportLiveObjects("--------SHADOW_PASS_INIT");
#endif

	// check feature support & error handle:
	// https://msdn.microsoft.com/en-us/library/windows/apps/dn263150

	// create shadow map texture for the pixel shader stage
	const bool bInitializeSRV = false;
	D3D11_TEXTURE2D_DESC shadowMapDesc;
	ZeroMemory(&shadowMapDesc, sizeof(D3D11_TEXTURE2D_DESC));
	shadowMapDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	shadowMapDesc.MipLevels = 1;
	shadowMapDesc.ArraySize = 1;
	shadowMapDesc.SampleDesc.Count = 1;
	shadowMapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL;
	shadowMapDesc.Height = static_cast<UINT>(_shadowMapDimension);
	shadowMapDesc.Width = static_cast<UINT>(_shadowMapDimension);
	this->_shadowMap = pRenderer->CreateTexture2D(shadowMapDesc, bInitializeSRV);

	// careful: removing const qualified from texture. rethink this
	Texture& shadowMap = const_cast<Texture&>(pRenderer->GetTextureObject(_shadowMap));

	// depth stencil view and shader resource view for the shadow map (^ BindFlags)
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	ZeroMemory(&dsvDesc, sizeof(dsvDesc));
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;			// check format
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	this->_dsv = pRenderer->AddDepthStencil(dsvDesc, shadowMap._tex2D);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.Texture2D.MipLevels = 1;

	HRESULT hr = device->CreateShaderResourceView(shadowMap._tex2D, &srvDesc, &shadowMap._srv);
	if (FAILED(hr))
	{
		Log::Error(ERROR_LOG::CANT_CREATE_RESOURCE, "Cannot create SRV in InitilizeDepthPass\n");

	}

	// comparison sampler
	D3D11_SAMPLER_DESC comparisonSamplerDesc;
	ZeroMemory(&comparisonSamplerDesc, sizeof(D3D11_SAMPLER_DESC));
	comparisonSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	comparisonSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	comparisonSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	comparisonSamplerDesc.BorderColor[0] = 1.0f;
	comparisonSamplerDesc.BorderColor[1] = 1.0f;
	comparisonSamplerDesc.BorderColor[2] = 1.0f;
	comparisonSamplerDesc.BorderColor[3] = 1.0f;
	comparisonSamplerDesc.MinLOD = 0.f;
	comparisonSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	comparisonSamplerDesc.MipLODBias = 0.f;
	comparisonSamplerDesc.MaxAnisotropy = 0;
	//comparisonSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;	// todo: set default sampler elsewhere
	comparisonSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	//comparisonSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
	comparisonSamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	this->_shadowSampler = pRenderer->CreateSamplerState(comparisonSamplerDesc);

	// render states for front face culling 
	this->_shadowRenderState = pRenderer->AddRasterizerState(ERasterizerCullMode::FRONT, ERasterizerFillMode::SOLID, true);
	this->_drawRenderState   = pRenderer->AddRasterizerState(ERasterizerCullMode::BACK, ERasterizerFillMode::SOLID, true);

	// shader
	std::vector<InputLayout> layout = {
		{ "POSITION",	FLOAT32_3 },
		{ "NORMAL",		FLOAT32_3 },
		{ "TANGENT",	FLOAT32_3 },
		{ "TEXCOORD",	FLOAT32_2 } };
	this->_shadowShader = SHADERS::SHADOWMAP_DEPTH;

	ZeroMemory(&_shadowViewport, sizeof(D3D11_VIEWPORT));
	_shadowViewport.Height = static_cast<float>(_shadowMapDimension);
	_shadowViewport.Width = static_cast<float>(_shadowMapDimension);
	_shadowViewport.MinDepth = 0.f;
	_shadowViewport.MaxDepth = 1.f;
}

void ShadowMapPass::RenderShadowMaps(Renderer* pRenderer, const std::vector<const Light*> shadowLights, const std::vector<GameObject*> ZPassObjects) const
{
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	pRenderer->BindDepthStencil(_dsv);						// only depth stencil buffer
	pRenderer->SetRasterizerState(_shadowRenderState);		// shadow render state: cull front faces, fill solid, clip dep
	pRenderer->SetViewport(_shadowViewport);				// lights viewport 512x512
	pRenderer->SetShader(_shadowShader);					// shader for rendering z buffer
	pRenderer->SetConstant4x4f("viewProj", shadowLights.front()->GetLightSpaceMatrix());
	pRenderer->SetConstant4x4f("view", shadowLights.front()->GetViewMatrix());
	pRenderer->SetConstant4x4f("proj", shadowLights.front()->GetProjectionMatrix());
	pRenderer->Apply();
	pRenderer->Begin(clearColor, 1.0f);
	for (const GameObject* obj : ZPassObjects)
	{
		obj->RenderZ(pRenderer);
	}
}



void PostProcessPass::Initialize(Renderer* pRenderer, const Settings::PostProcess& postProcessSettings)
{
	_settings = postProcessSettings;
	DXGI_SAMPLE_DESC smpDesc;
	smpDesc.Count = 1;
	smpDesc.Quality = 0;

	constexpr const DXGI_FORMAT HDR_Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr const DXGI_FORMAT LDR_Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	DXGI_FORMAT format = _settings.HDREnabled ? HDR_Format : LDR_Format;

	D3D11_TEXTURE2D_DESC rtDesc;
	ZeroMemory(&rtDesc, sizeof(rtDesc));
	rtDesc.Width = pRenderer->WindowWidth();
	rtDesc.Height = pRenderer->WindowHeight();
	rtDesc.MipLevels = 1;
	rtDesc.ArraySize = 1;
	rtDesc.Format = format;
	rtDesc.Usage = D3D11_USAGE_DEFAULT;
	rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	rtDesc.CPUAccessFlags = 0;
	rtDesc.SampleDesc = smpDesc;
	rtDesc.MiscFlags = 0;

	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
	ZeroMemory(&RTVDesc, sizeof(RTVDesc));
	RTVDesc.Format = format;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;

	// Bloom effect
	this->_bloomPass._colorRT = pRenderer->AddRenderTarget(rtDesc, RTVDesc);
	this->_bloomPass._brightRT = pRenderer->AddRenderTarget(rtDesc, RTVDesc);
	this->_bloomPass._finalRT = pRenderer->AddRenderTarget(rtDesc, RTVDesc);
	this->_bloomPass._blurPingPong[0] = pRenderer->AddRenderTarget(rtDesc, RTVDesc);
	this->_bloomPass._blurPingPong[1] = pRenderer->AddRenderTarget(rtDesc, RTVDesc);

	D3D11_SAMPLER_DESC blurSamplerDesc;
	ZeroMemory(&blurSamplerDesc, sizeof(blurSamplerDesc));
	blurSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	blurSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	blurSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	blurSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	this->_bloomPass._blurSampler = pRenderer->CreateSamplerState(blurSamplerDesc);
	
	// Tonemapping
	this->_tonemappingPass._finalRenderTarget = pRenderer->GetDefaultRenderTarget();

	// World Render Target
	this->_worldRenderTarget = pRenderer->AddRenderTarget(rtDesc, RTVDesc);
}

void PostProcessPass::Render(Renderer * pRenderer) const
{
	const TextureID worldTexture = pRenderer->GetRenderTargetTexture(_worldRenderTarget);

	// ======================================================================================
	// BLOOM  PASS
	// ======================================================================================
	if (_bloomPass._isEnabled)
	{
		const Settings::PostProcess::Bloom& s = _settings.bloom;
		const float brightnessThreshold = SHADERS::FORWARD_BRDF == pRenderer->GetActiveShader() ? s.threshold_brdf : s.threshold_phong;

		// bright filter
		pRenderer->SetShader(SHADERS::BLOOM);
		pRenderer->BindRenderTargets(_bloomPass._colorRT, _bloomPass._brightRT);
		pRenderer->UnbindDepthStencil();
		pRenderer->SetBufferObj(GEOMETRY::QUAD);
		pRenderer->Apply();
		pRenderer->SetTexture("worldRenderTarget", worldTexture);
		pRenderer->SetConstant1f("BrightnessThreshold", brightnessThreshold);
		pRenderer->Apply();
		pRenderer->DrawIndexed();

		// blur
		const TextureID brightTexture = pRenderer->GetRenderTargetTexture(_bloomPass._brightRT);
		pRenderer->SetShader(SHADERS::BLUR);
		for (int i = 0; i < s.blurPassCount; ++i)
		{
			const int isHorizontal = i % 2;
			const TextureID pingPong = pRenderer->GetRenderTargetTexture(_bloomPass._blurPingPong[1 - isHorizontal]);
			const int texWidth = pRenderer->GetTextureObject(pingPong)._width;
			const int texHeight = pRenderer->GetTextureObject(pingPong)._height;

			pRenderer->UnbindRenderTarget();
			pRenderer->Apply();
			pRenderer->BindRenderTarget(_bloomPass._blurPingPong[isHorizontal]);
			pRenderer->SetConstant1i("isHorizontal", 1 - isHorizontal);
			pRenderer->SetConstant1i("textureWidth", texWidth);
			pRenderer->SetConstant1i("textureHeight", texHeight);
			pRenderer->SetTexture("InputTexture", i == 0 ? brightTexture : pingPong);
			pRenderer->SetSamplerState("BlurSampler", _bloomPass._blurSampler);
			pRenderer->Apply();
			pRenderer->DrawIndexed();
		}

		// additive blend combine
		const TextureID colorTex = pRenderer->GetRenderTargetTexture(_bloomPass._colorRT);
		const TextureID bloomTex = pRenderer->GetRenderTargetTexture(_bloomPass._blurPingPong[0]);
		pRenderer->SetShader(SHADERS::BLOOM_COMBINE);
		pRenderer->BindRenderTarget(_bloomPass._finalRT);
		pRenderer->Apply();
		pRenderer->SetTexture("ColorTexture", colorTex);
		pRenderer->SetTexture("BloomTexture", bloomTex);
		pRenderer->SetSamplerState("BlurSampler", _bloomPass._blurSampler);
		pRenderer->Apply();
		pRenderer->DrawIndexed();
	}



	// ======================================================================================
	// TONEMAPPING PASS
	// ======================================================================================
	const TextureID colorTex = pRenderer->GetRenderTargetTexture(_bloomPass._isEnabled ? _bloomPass._finalRT : _worldRenderTarget);
	const float isHDR = _settings.HDREnabled ? 1.0f : 0.0f;
	pRenderer->SetShader(SHADERS::TONEMAPPING);
	pRenderer->SetBufferObj(GEOMETRY::QUAD);
	pRenderer->SetSamplerState("Sampler", _bloomPass._blurSampler);
	pRenderer->SetConstant1f("exposure", _settings.toneMapping.exposure);
	pRenderer->SetConstant1f("isHDR", isHDR);
	pRenderer->BindRenderTarget(_tonemappingPass._finalRenderTarget);
	pRenderer->SetTexture("ColorTexture", colorTex);
	pRenderer->Apply();
	pRenderer->DrawIndexed();

}



void DeferredRenderingPasses::InitializeGBuffer(Renderer* pRenderer)
{
	Log::Info("Initializing GBuffer...");

	DXGI_SAMPLE_DESC smpDesc;
	smpDesc.Count = 1;
	smpDesc.Quality = 0;

	D3D11_TEXTURE2D_DESC RTDescriptor[2] = { {}, {} };
	constexpr const DXGI_FORMAT Format[2] = { /*DXGI_FORMAT_R11G11B10_FLOAT*/ DXGI_FORMAT_R16G16B16A16_FLOAT , DXGI_FORMAT_R16G16B16A16_FLOAT };

	for (int i = 0; i < 2; ++i)
	{
		RTDescriptor[i].Width = pRenderer->WindowWidth();
		RTDescriptor[i].Height = pRenderer->WindowHeight();
		RTDescriptor[i].MipLevels = 1;
		RTDescriptor[i].ArraySize = 1;
		RTDescriptor[i].Format = Format[i];
		RTDescriptor[i].Usage = D3D11_USAGE_DEFAULT;
		RTDescriptor[i].BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		RTDescriptor[i].CPUAccessFlags = 0;
		RTDescriptor[i].SampleDesc = smpDesc;
		RTDescriptor[i].MiscFlags = 0;
	}

	D3D11_RENDER_TARGET_VIEW_DESC RTVDescriptor[2] = { {},{} };
	for (int i = 0; i < 2; ++i)
	{
		RTVDescriptor[i].Format = Format[i];
		RTVDescriptor[i].ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		RTVDescriptor[i].Texture2D.MipSlice = 0;
	}
	
	constexpr size_t Float3TypeIndex = 0;		constexpr size_t Float4TypeIndex = 1;
	this->_GBuffer._positionRT		   = pRenderer->AddRenderTarget(RTDescriptor[Float3TypeIndex], RTVDescriptor[Float3TypeIndex]);
	this->_GBuffer._normalRT		   = pRenderer->AddRenderTarget(RTDescriptor[Float3TypeIndex], RTVDescriptor[Float3TypeIndex]);
	this->_GBuffer._diffuseRoughnessRT = pRenderer->AddRenderTarget(RTDescriptor[Float4TypeIndex], RTVDescriptor[Float4TypeIndex]);
	this->_GBuffer._specularMetallicRT = pRenderer->AddRenderTarget(RTDescriptor[Float4TypeIndex], RTVDescriptor[Float4TypeIndex]);

	Log::Info("Done.");
}

void DeferredRenderingPasses::SetGeometryRenderingStates(Renderer* pRenderer) const
{
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	const float clearDepth = 1.0f;

	pRenderer->SetShader(SHADERS::DEFERRED_GEOMETRY);
	pRenderer->BindRenderTargets(_GBuffer._diffuseRoughnessRT, _GBuffer._specularMetallicRT, _GBuffer._normalRT, _GBuffer._positionRT);
	pRenderer->SetSamplerState("sNormalSampler", 0);
	pRenderer->Begin(clearColor, clearDepth);
	pRenderer->Apply();
}

void DeferredRenderingPasses::RenderLightingPass(Renderer* pRenderer, const RenderTargetID target, const Camera* pCamera, const std::vector<Light>& lights) const
{
	constexpr const float clearColor[4] = { 0,0,0,0 };
	const TextureID texNormal = pRenderer->GetRenderTargetTexture(_GBuffer._normalRT);
	const TextureID texDiffuseRoughness = pRenderer->GetRenderTargetTexture(_GBuffer._diffuseRoughnessRT);
	const TextureID texSpecularMetallic = pRenderer->GetRenderTargetTexture(_GBuffer._specularMetallicRT);
	const TextureID texPosition = pRenderer->GetRenderTargetTexture(_GBuffer._positionRT);

	// pRenderer->UnbindRendertargets();	// ignore this for now
	pRenderer->UnbindDepthStencil();
	pRenderer->BindRenderTarget(target);
	pRenderer->Begin(clearColor, 0);
	pRenderer->Apply();

	// AMBIENT LIGHTING
	const float ambient = 0.0005f;
	// todo: set ambient occlusion texture
	pRenderer->SetShader(SHADERS::DEFERRED_BRDF_AMBIENT);
	pRenderer->SetConstant1f("ambient", ambient);
	pRenderer->SetTexture("texDiffuseRoughnessMap", texDiffuseRoughness);
	pRenderer->SetBufferObj(GEOMETRY::QUAD);
	pRenderer->Apply();
	pRenderer->DrawIndexed();


	// POINT LIGHTS
#if 0
	pRenderer->SetBlendState(EDefaultBlendState::ADDITIVE_COLOR);

	pRenderer->SetShader(SHADERS::DEFERRED_BRDF_POINT);
	pRenderer->SetConstant3f("cameraPos", pCamera->GetPositionF());
	pRenderer->SetTexture("texDiffuseRoughnessMap", texDiffuseRoughness);
	//pRenderer->SetTexture("texSpecularMetalnessMap", texSpecularMetallic);
	//pRenderer->SetTexture("texNormals", texNormal);
	//pRenderer->SetTexture("texPosition", texPosition);
	pRenderer->SetBufferObj(GEOMETRY::QUAD);

	// for point lights
	{

		pRenderer->Apply();
		pRenderer->DrawIndexed();
	}
#endif

	// SPOT LIGHTS
#if 0
	pRenderer->SetShader(SHADERS::DEFERRED_BRDF);
	pRenderer->SetConstant3f("cameraPos", m_pCamera->GetPositionF());

	pRenderer->SetBufferObj(GEOMETRY::QUAD);
	
	// for spot lights
	
	pRenderer->Apply();
	pRenderer->DrawIndexed();
#endif
	pRenderer->SetBlendState(EDefaultBlendState::DISABLED);
}
