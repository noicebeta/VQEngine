//	VQEngine | DirectX11 Renderer
//	Copyright(C) 2018  - Volkan Ilbeyli
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

#include "Engine/SceneResourceView.h"
#include "Engine/Engine.h"
#include "Engine/GameObject.h"
#include "Engine/Light.h"
#include "Engine/SceneView.h"

#include "Renderer/Renderer.h"
#include "Renderer/GeometryGenerator.h"

#include <unordered_map>
#include <set>

constexpr int DRAW_INSTANCED_COUNT_GBUFFER_PASS = 64;

void DeferredRenderingPasses::Initialize(Renderer * pRenderer, bool bAAResolve)
{
	//const ShaderMacro testMacro = { "TEST_DEFINE", "1" };

	// TODO: reduce the code redundancy
	const char* pFSQ_VS = "FullScreenQuad_vs.hlsl";
	const char* pLight_VS = "MVPTransformationWithUVs_vs.hlsl";
	const ShaderDesc geomShaderDesc = { "GBufferPass", {
		ShaderStageDesc{ "Deferred_Geometry_vs.hlsl", {} },
		ShaderStageDesc{ "Deferred_Geometry_ps.hlsl", {} }
	} };
	const std::vector<ShaderMacro> instancedGeomShaderMacros =
	{
		ShaderMacro{ "INSTANCED", "1" },
		ShaderMacro{ "INSTANCE_COUNT", std::to_string(DRAW_INSTANCED_COUNT_GBUFFER_PASS)}
	};
	const ShaderDesc geomShaderInstancedDesc = { "InstancedGBufferPass",
	{
		ShaderStageDesc{ "Deferred_Geometry_vs.hlsl", instancedGeomShaderMacros },
		ShaderStageDesc{ "Deferred_Geometry_ps.hlsl", instancedGeomShaderMacros }
	} };
	const ShaderDesc ambientShaderDesc = { "Deferred_Ambient",
	{
		ShaderStageDesc{ pFSQ_VS, {} },
		ShaderStageDesc{ "deferred_brdf_ambient_ps.hlsl", {} }
	} };
	const ShaderDesc ambientIBLShaderDesc = { "Deferred_AmbientIBL",
	{
		ShaderStageDesc{ pFSQ_VS, {} },
		ShaderStageDesc{ "deferred_brdf_ambientIBL_ps.hlsl", {} }
	} };
	const ShaderDesc BRDFLightingShaderDesc = { "Deferred_BRDF_Lighting",
	{
		ShaderStageDesc{ pFSQ_VS, {} },
		ShaderStageDesc{ "deferred_brdf_lighting_ps.hlsl", {} }
	} };
	const ShaderDesc phongLighintShaderDesc = { "Deferred_Phong_Lighting",
	{
		ShaderStageDesc{ pFSQ_VS, {} },
		ShaderStageDesc{ "deferred_phong_lighting_ps.hlsl", {} }
	} };
	const ShaderDesc BRDF_PointLightShaderDesc = { "Deferred_BRDF_Point",
	{
		ShaderStageDesc{ pLight_VS, {} },
		ShaderStageDesc{ "deferred_brdf_pointLight_ps.hlsl", {} }
	} };
	const ShaderDesc BRDF_SpotLightShaderDesc = { "Deferred_BRDF_Spot",
	{
		ShaderStageDesc{ pLight_VS, {} },
		ShaderStageDesc{ "deferred_brdf_spotLight_ps.hlsl", {} }
	} };

	InitializeGBuffer(pRenderer);

	const EImageFormat format = EImageFormat::RGBA16F; // TODO: get this from somewhere.
	RenderTargetDesc rtDesc = {};
	rtDesc.textureDesc.width  = pRenderer->mAntiAliasing.resolutionY; // TODO: what if aa turned off?
	rtDesc.textureDesc.height = pRenderer->mAntiAliasing.resolutionX; // TODO: what if aa turned off?
	rtDesc.textureDesc.mipCount = 1;
	rtDesc.textureDesc.arraySize = 1;
	rtDesc.textureDesc.format = format;
	rtDesc.textureDesc.usage = ETextureUsage::RENDER_TARGET_RW;
	rtDesc.format = format;
	_shadeTarget = pRenderer->AddRenderTarget(rtDesc);

	_geometryShader = pRenderer->CreateShader(geomShaderDesc);
	_geometryInstancedShader = pRenderer->CreateShader(geomShaderInstancedDesc);
	_ambientShader = pRenderer->CreateShader(ambientShaderDesc);
	_ambientIBLShader = pRenderer->CreateShader(ambientIBLShaderDesc);
	_BRDFLightingShader = pRenderer->CreateShader(BRDFLightingShaderDesc);
	_phongLightingShader = pRenderer->CreateShader(phongLighintShaderDesc);
#if 0
	_spotLightShader = pRenderer->CreateShader(BRDF_PointLightShaderDesc);
	_pointLightShader = pRenderer->CreateShader(BRDF_SpotLightShaderDesc);
#endif
}

void DeferredRenderingPasses::InitializeGBuffer(Renderer* pRenderer)
{
	// initialize RT descriptors for each image type
	EImageFormat imageFormats[3] = { RGBA32F, RGBA16F, R11G11B10F };
	RenderTargetDesc rtDesc[3] = { {}, {}, {} };
	for (int i = 0; i < 3; ++i)
	{
		rtDesc[i].textureDesc.width  = pRenderer->FrameRenderTargetWidth();
		rtDesc[i].textureDesc.height = pRenderer->FrameRenderTargetHeight();
		rtDesc[i].textureDesc.mipCount = 1;
		rtDesc[i].textureDesc.arraySize = 1;
		rtDesc[i].textureDesc.usage = ETextureUsage::RENDER_TARGET_RW;
		rtDesc[i].textureDesc.format = imageFormats[i];
		rtDesc[i].format = imageFormats[i];
	}

	constexpr size_t Float3TypeIndex = 0;		constexpr size_t Float4TypeIndex = 1;
	this->_GBuffer.mRTNormals = pRenderer->AddRenderTarget(rtDesc[this->mNormalRTCompressionLevel]);
	this->_GBuffer.mRTDiffuseRoughness = pRenderer->AddRenderTarget(rtDesc[Float4TypeIndex]);
	this->_GBuffer.mRTSpecularMetallic = pRenderer->AddRenderTarget(rtDesc[Float4TypeIndex]);
	this->_GBuffer.mRTEmissive = pRenderer->AddRenderTarget(rtDesc[Float3TypeIndex]);
	this->_GBuffer.bInitialized = true;

	// http://download.nvidia.com/developer/presentations/2004/6800_Leagues/6800_Leagues_Deferred_Shading.pdf
	// Option: trade storage for computation
	//  - Store pos.z     and compute xy from z + window.xy		(implemented)
	//	- Store normal.xy and compute z = sqrt(1 - x^2 - y^2)
	//
	{	// Geometry depth stencil state descriptor
		D3D11_DEPTH_STENCILOP_DESC dsOpDesc = {};
		dsOpDesc.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsOpDesc.StencilDepthFailOp = D3D11_STENCIL_OP_INCR_SAT;
		dsOpDesc.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		dsOpDesc.StencilFunc = D3D11_COMPARISON_ALWAYS;

		D3D11_DEPTH_STENCIL_DESC desc = {};
		desc.FrontFace = dsOpDesc;
		desc.BackFace = dsOpDesc;

		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

		desc.StencilEnable = true;
		desc.StencilReadMask = 0xFF;
		desc.StencilWriteMask = 0xFF;

		_geometryStencilState = pRenderer->AddDepthStencilState(desc);

		desc.StencilEnable = false;
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_EQUAL;
		_geometryStencilStatePreZ = pRenderer->AddDepthStencilState(desc);
	}

}

void DeferredRenderingPasses::ClearGBuffer(Renderer* pRenderer)
{
	const bool bDoClearColor = true;
	const bool bDoClearDepth = false;
	const bool bDoClearStencil = false;
	ClearCommand clearCmd(
		bDoClearColor, bDoClearDepth, bDoClearStencil,
		{ 0, 0, 0, 0 }, 0, 0
	);
	pRenderer->BindRenderTargets(_GBuffer.mRTDiffuseRoughness, _GBuffer.mRTSpecularMetallic, _GBuffer.mRTNormals);
	pRenderer->BeginRender(clearCmd);
}


//struct InstancedGbufferObjectMatrices { ObjectMatrices objMatrices[DRAW_INSTANCED_COUNT_GBUFFER_PASS]; };
void DeferredRenderingPasses::RenderGBuffer(Renderer* pRenderer, const Scene* pScene, const SceneView& sceneView) const
{
	//--------------------------------------------------------------------------------------------------------------------
	struct InstancedGbufferObjectMaterials { SurfaceMaterial objMaterials[DRAW_INSTANCED_COUNT_GBUFFER_PASS]; };
	auto RenderObject = [&](const GameObject* pObj)
	{
		const Transform& tf = pObj->GetTransform();
		const ModelData& model = pObj->GetModelData();

		const XMMATRIX world = tf.WorldTransformationMatrix();
		const ObjectMatrices mats =
		{
			world * sceneView.view,
			tf.NormalMatrix(world) * sceneView.view,
			world * sceneView.viewProj,
		};


		SurfaceMaterial material;
		for (MeshID id : model.mMeshIDs)
		{
			const auto IABuffer = SceneResourceView::GetVertexAndIndexBufferIDsOfMesh(pScene, id, pObj);

			// SET MATERIAL CONSTANT BUFFER & TEXTURES
			//
			const bool bMeshHasMaterial = model.mMaterialLookupPerMesh.find(id) != model.mMaterialLookupPerMesh.end();
			if (bMeshHasMaterial)
			{
				const MaterialID materialID = model.mMaterialLookupPerMesh.at(id);
				const Material* pMat = SceneResourceView::GetMaterial(pScene, materialID);

				// #TODO: uncomment below when transparency is implemented.
				//if (pMat->IsTransparent())	// avoidable branching - perhaps keeping opaque and transparent meshes on separate vectors is better.
				//	return;

				material = pMat->GetCBufferData();
				pRenderer->SetConstantStruct("surfaceMaterial", &material);
				pRenderer->SetConstantStruct("ObjMatrices", &mats);

				// #TODO: this is duplicate code, see Forward.
				pRenderer->SetSamplerState("sAnisoSampler", EDefaultSamplerState::ANISOTROPIC_4_WRAPPED_SAMPLER);
				if (pMat->diffuseMap >= 0)		pRenderer->SetTexture("texDiffuseMap", pMat->diffuseMap);
				if (pMat->normalMap >= 0)		pRenderer->SetTexture("texNormalMap", pMat->normalMap);
				if (pMat->specularMap >= 0)		pRenderer->SetTexture("texSpecularMap", pMat->specularMap);
				if (pMat->mask >= 0)			pRenderer->SetTexture("texAlphaMask", pMat->mask);
				if (pMat->metallicMap >= 0)		pRenderer->SetTexture("texMetallicMap", pMat->metallicMap);
				if (pMat->roughnessMap >= 0)	pRenderer->SetTexture("texRoughnessMap", pMat->roughnessMap);
#if ENABLE_PARALLAX_MAPPING
				if (pMat->heightMap >= 0)		pRenderer->SetTexture("texHeightMap", pMat->heightMap);
#endif
				if (pMat->emissiveMap >= 0)		pRenderer->SetTexture("texEmissiveMap", pMat->emissiveMap);
				pRenderer->SetConstant1f("BRDFOrPhong", 1.0f);	// assume brdf for now

			}
			else
			{
				// each object should have a material assigned.
				// if not, we just send default
				Material::GetDefaultMaterialCBufferData();
			}

			
			pRenderer->SetRasterizerState(SceneResourceView::GetMeshRenderMode(pScene, pObj, id) == MeshRenderSettings::EMeshRenderMode::WIREFRAME
				? EDefaultRasterizerState::WIREFRAME 
				: EDefaultRasterizerState::CULL_BACK);

			pRenderer->SetVertexBuffer(IABuffer.first);
			pRenderer->SetIndexBuffer(IABuffer.second);
			pRenderer->Apply();
			pRenderer->DrawIndexed();
		}
	};
	auto RenderObject_DepthOnly = [&](const GameObject* pObj)
	{
		const Transform& tf = pObj->GetTransform();
		const ModelData& model = pObj->GetModelData();

		const XMMATRIX wvp = tf.WorldTransformationMatrix() * sceneView.viewProj;
		const DepthOnlyPass_PerObjectMatrices objMats = DepthOnlyPass_PerObjectMatrices({ wvp });

		pRenderer->SetRasterizerState(EDefaultRasterizerState::CULL_BACK);

		SurfaceMaterial material;
		for (MeshID id : model.mMeshIDs)
		{
			const auto IABuffer = SceneResourceView::GetVertexAndIndexBufferIDsOfMesh(pScene, id, pObj);

			// SET MATERIAL CONSTANT BUFFER & TEXTURES
			//
			const bool bMeshHasMaterial = model.mMaterialLookupPerMesh.find(id) != model.mMaterialLookupPerMesh.end();
			if (bMeshHasMaterial)
			{
				const MaterialID materialID = model.mMaterialLookupPerMesh.at(id);
				const Material* pMat = SceneResourceView::GetMaterial(pScene, materialID);

				// #TODO: uncomment below when transparency is implemented.
				//if (pMat->IsTransparent())	// avoidable branching - perhaps keeping opaque and transparent meshes on separate vectors is better.
				//	return;
#if 0
				material = pMat->GetShaderFriendlyStruct();
				pRenderer->SetConstantStruct("surfaceMaterial", &material);
				pRenderer->SetSamplerState("sAnisoSampler", EDefaultSamplerState::LINEAR_FILTER_SAMPLER);
				if (pMat->diffuseMap >= 0)		pRenderer->SetTexture("texDiffuseMap", pMat->diffuseMap);
				if (pMat->mask >= 0)			pRenderer->SetTexture("texAlphaMask", pMat->mask);
#if ENABLE_PARALLAX_MAPPING
				if (pMat->heightMap >= 0)		pRenderer->SetTexture("texHeightMap", pMat->heightMap);
#endif
				if (pMat->emissiveMap >= 0)		pRenderer->SetTexture("texEmissiveMap", pMat->emissiveMap);
				pRenderer->SetConstant1f("BRDFOrPhong", 1.0f);	// assume brdf for now
#endif
			}

			pRenderer->SetConstantStruct("ObjMats", &objMats);

			pRenderer->SetVertexBuffer(IABuffer.first);
			pRenderer->SetIndexBuffer(IABuffer.second);
			pRenderer->Apply();
			pRenderer->DrawIndexed();
		}
	};
	//--------------------------------------------------------------------------------------------------------------------

	bool bDoClearColor = !this->mbUseDepthPrepass; // do not clear color if depth pre-pass
	bool bDoClearDepth = true;
	bool bDoClearStencil = true;
	ClearCommand clearCmd(
		bDoClearColor, bDoClearDepth, bDoClearStencil,
		{ 0, 0, 0, 0 }, 1, 0
	);
	
	if (this->mbUseDepthPrepass)
	{
		pRenderer->BeginEvent("GBuffer::DepthPrePass");
		pRenderer->SetShader(EShaders::SHADOWMAP_DEPTH);
		pRenderer->BindDepthTarget(ENGINE->GetWorldDepthTarget());
		pRenderer->SetDepthStencilState(_geometryStencilState);
		pRenderer->BeginRender(clearCmd);
		pRenderer->Apply();

		// RENDER NON-INSTANCED SCENE OBJECTS
		//
		int numObj = 0;
		for (const auto* obj : sceneView.culledOpaqueList)
		{
			RenderObject_DepthOnly(obj);
			++numObj;
		}


		// RENDER INSTANCED SCENE OBJECTS
		//
		pRenderer->SetShader(EShaders::SHADOWMAP_DEPTH_INSTANCED);

		//InstancedObjectMatrices<DRAW_INSTANCED_COUNT_GBUFFER_PASS> cbufferMatrices;
		//InstancedGbufferObjectMaterials cbufferMaterials;


		DepthOnlyPass_InstancedObjectCBuffer cbuffer;

		for (const RenderListLookupEntry& MeshID_RenderList : sceneView.culluedOpaqueInstancedRenderListLookup)
		{
			const MeshID& meshID = MeshID_RenderList.first;
			const RenderList& renderList = MeshID_RenderList.second;

			const RasterizerStateID rasterizerState = GeometryGenerator::Is2DGeometry(static_cast<EGeometry>(meshID)) ? EDefaultRasterizerState::CULL_NONE : EDefaultRasterizerState::CULL_BACK;
			const auto IABuffer = SceneResourceView::GetVertexAndIndexBufferIDsOfMesh(pScene, meshID, renderList.back());

			pRenderer->SetRasterizerState(rasterizerState);
			pRenderer->SetVertexBuffer(IABuffer.first);
			pRenderer->SetIndexBuffer(IABuffer.second);

			int batchCount = 0;
			do
			{
				int instanceID = 0;
				for (; instanceID < DRAW_INSTANCED_COUNT_GBUFFER_PASS; ++instanceID)
				{
					const int renderListIndex = DRAW_INSTANCED_COUNT_GBUFFER_PASS * batchCount + instanceID;
					if (renderListIndex == renderList.size())
						break;

					const GameObject* pObj = renderList[renderListIndex];
					const Transform& tf = pObj->GetTransform();
					const ModelData& model = pObj->GetModelData();

					const XMMATRIX world = tf.WorldTransformationMatrix();
					cbuffer.objMatrices[instanceID] =
					{
						world * sceneView.viewProj,
					};

					const bool bMeshHasMaterial = model.mMaterialLookupPerMesh.find(meshID) != model.mMaterialLookupPerMesh.end();
					if (bMeshHasMaterial)
					{
						const MaterialID materialID = model.mMaterialLookupPerMesh.at(meshID);
						const Material* pMat = SceneResourceView::GetMaterial(pScene, materialID);

						// TODO: figure out of the object has alpha and use a non-null PS.
						//       opaque objects are drawn without PS.
					}
				}
				//pRenderer->SetConstantStruct("surfaceMaterial", &cbufferMaterials);
				//pRenderer->SetConstant1f("BRDFOrPhong", 1.0f);	// assume brdf for now

				pRenderer->SetConstantStruct("ObjMats", &cbuffer);
				pRenderer->Apply();
				pRenderer->DrawIndexedInstanced(instanceID);
			} while (batchCount++ < renderList.size() / DRAW_INSTANCED_COUNT_GBUFFER_PASS);
		}

		// set the clear command for the main GBuffer pass next
		bDoClearDepth = false;
		bDoClearStencil = false;
		bDoClearColor = true;
		clearCmd = ClearCommand(
			bDoClearColor, bDoClearDepth, bDoClearStencil,
			{ 0, 0, 0, 0 }, 1, 0
		);
		pRenderer->EndEvent();
	}



	pRenderer->SetShader(_geometryShader);
	pRenderer->SetViewport(pRenderer->FrameRenderTargetWidth(), pRenderer->FrameRenderTargetHeight());
	pRenderer->BindRenderTargets(_GBuffer.mRTDiffuseRoughness, _GBuffer.mRTSpecularMetallic, _GBuffer.mRTNormals, _GBuffer.mRTEmissive);
	if(!this->mbUseDepthPrepass)
		pRenderer->BindDepthTarget(ENGINE->GetWorldDepthTarget());
	pRenderer->SetDepthStencilState(this->mbUseDepthPrepass ? _geometryStencilStatePreZ : _geometryStencilState);
	pRenderer->SetSamplerState("sNormalSampler", EDefaultSamplerState::LINEAR_FILTER_SAMPLER_WRAP_UVW);
	pRenderer->BeginRender(clearCmd);
	pRenderer->Apply();


	// RENDER NON-INSTANCED SCENE OBJECTS
	//
	int numObj = 0;
	for (const auto* obj : sceneView.culledOpaqueList)
	{	// TODO: nuke this object-oriented container and render 
		//       on the mesh level.
		RenderObject(obj);
		++numObj;
	}



	// RENDER INSTANCED SCENE OBJECTS
	//
	pRenderer->SetShader(_geometryInstancedShader);
	pRenderer->Apply();

	InstancedObjectMatrices<DRAW_INSTANCED_COUNT_GBUFFER_PASS> cbufferMatrices;
	InstancedGbufferObjectMaterials cbufferMaterials;

	for (const RenderListLookupEntry& MeshID_RenderList : sceneView.culluedOpaqueInstancedRenderListLookup)
	{
		const MeshID& meshID = MeshID_RenderList.first;
		const RenderList& renderList = MeshID_RenderList.second;


		const RasterizerStateID rasterizerState = GeometryGenerator::Is2DGeometry(static_cast<EGeometry>(meshID))
			? EDefaultRasterizerState::CULL_NONE 
			: EDefaultRasterizerState::CULL_BACK;
#if 1
		// note: using renderList.back() as the last argument to the GetVertexAndIndexBufferIDsOfMesh() will enable LOD
		//       levels for GBuffer pass, but they won't be technically correct. we have to separate instanced render list
		//       even further here, based on the active LOD mesh. For now, using back() will provide some nice perf results.
		const auto IABuffer = SceneResourceView::GetVertexAndIndexBufferIDsOfMesh(pScene, meshID, renderList.back());
		pRenderer->SetRasterizerState(rasterizerState);
		pRenderer->SetVertexBuffer(IABuffer.first);
		pRenderer->SetIndexBuffer(IABuffer.second);

		int batchCount = 0;
		do
		{
			int instanceID = 0;
			for (; instanceID < DRAW_INSTANCED_COUNT_GBUFFER_PASS; ++instanceID)
			{
				const int renderListIndex = DRAW_INSTANCED_COUNT_GBUFFER_PASS * batchCount + instanceID;
				if (renderListIndex == renderList.size())
					break;

				const GameObject* pObj = renderList[renderListIndex];
				const Transform& tf = pObj->GetTransform();
				const ModelData& model = pObj->GetModelData();

				const XMMATRIX world = tf.WorldTransformationMatrix();
				cbufferMatrices.objMatrices[instanceID] =
				{
					world * sceneView.view,
					tf.NormalMatrix(world) * sceneView.view,
					world * sceneView.viewProj,
				};

				const bool bMeshHasMaterial = model.mMaterialLookupPerMesh.find(meshID) != model.mMaterialLookupPerMesh.end();
				if (bMeshHasMaterial)
				{
					const MaterialID materialID = model.mMaterialLookupPerMesh.at(meshID);
					const Material* pMat = SceneResourceView::GetMaterial(pScene, materialID);
					cbufferMaterials.objMaterials[instanceID] = pMat->GetCBufferData();
				}

				// if an object doesn't have a material, we send default material info
				else
				{
					cbufferMaterials.objMaterials[instanceID] = Material::GetDefaultMaterialCBufferData();
				}
			}

			pRenderer->SetConstantStruct("ObjMatrices", &cbufferMatrices);
			pRenderer->SetConstantStruct("surfaceMaterial", &cbufferMaterials);
			pRenderer->SetConstant1f("BRDFOrPhong", 1.0f);	// assume brdf for now
			pRenderer->Apply();
			pRenderer->DrawIndexedInstanced(instanceID);
		} while (batchCount++ < renderList.size() / DRAW_INSTANCED_COUNT_GBUFFER_PASS);
#else
		// TODO: iterate over renderList, figure out different LOD levels for a given mesh
		//       and then generate sub RenderLists for each LOD level that exists for that given mesh.
#if 0
		// note: apparently cannot use unorderedmap with std pair as key as it cannot hash (deleted fn).
		using IABuffer_t = std::pair<BufferID, BufferID>;
		using IABufferGameObjectLookup_t = std::unordered_map< IABuffer_t, std::vector<const GameObject*>>;
		IABufferGameObjectLookup_t LOD_IABufferLookup;
		std::unordered_map< std::pair<BufferID, BufferID>, std::vector<const GameObject*>> LOD_IABufferLookup;
#else
		struct LODRenderList
		{
			std::pair<BufferID, BufferID>  LOD_IABuffers;
			std::vector<const GameObject*> LOD_RenderList;
		};
		std::set<LODRenderList> LOD_RenderLists;
#endif

		for (const GameObject* pObj : renderList)
		{
			const auto LOD_IABuffer = SceneResourceView::GetVertexAndIndexBufferIDsOfMesh(pScene, meshID, pObj);
			//if (LOD_IABufferLookup.find(LOD_IABuffer) == LOD_IABufferLookup.end())
			//{
			//	std::vector<const GameObject*> lodObjects(1, pObj);
			//	LOD_IABufferLookup[LOD_IABuffer] = lodObjects;
			//}
			//else
			//{
			//	LOD_IABufferLookup.at(LOD_IABuffer).push_back(pObj);
			//}

		}


		for (const std::pair<std::pair<BufferID, BufferID>, const std::vector<const GameObject*>>& IABufferRenderListPair : LOD_IABufferLookup)
		{
			const auto IABuffer = IABufferRenderListPair.first;
			const RenderList& LOD_renderList = IABufferRenderListPair.second;

			pRenderer->SetRasterizerState(rasterizerState);
			pRenderer->SetVertexBuffer(IABuffer.first);
			pRenderer->SetIndexBuffer(IABuffer.second);

			int batchCount = 0;
			do
			{
				int instanceID = 0;
				for (; instanceID < DRAW_INSTANCED_COUNT_GBUFFER_PASS; ++instanceID)
				{
					const int renderListIndex = DRAW_INSTANCED_COUNT_GBUFFER_PASS * batchCount + instanceID;
					if (renderListIndex == LOD_renderList.size())
						break;

					const GameObject* pObj = LOD_renderList[renderListIndex];
					const Transform& tf = pObj->GetTransform();
					const ModelData& model = pObj->GetModelData();

					const XMMATRIX world = tf.WorldTransformationMatrix();
					cbufferMatrices.objMatrices[instanceID] =
					{
						world * sceneView.view,
						tf.NormalMatrix(world) * sceneView.view,
						world * sceneView.viewProj,
					};

					const bool bMeshHasMaterial = model.mMaterialLookupPerMesh.find(meshID) != model.mMaterialLookupPerMesh.end();
					if (bMeshHasMaterial)
					{
						const MaterialID materialID = model.mMaterialLookupPerMesh.at(meshID);
						const Material* pMat = SceneResourceView::GetMaterial(pScene, materialID);
						cbufferMaterials.objMaterials[instanceID] = pMat->GetCBufferData();
					}

					// if an object doesn't have a material, we send default material info
					else
					{
						cbufferMaterials.objMaterials[instanceID] = Material::GetDefaultMaterialCBufferData();
					}
				}

				pRenderer->SetConstantStruct("ObjMatrices", &cbufferMatrices);
				pRenderer->SetConstantStruct("surfaceMaterial", &cbufferMaterials);
				pRenderer->SetConstant1f("BRDFOrPhong", 1.0f);	// assume brdf for now
				pRenderer->Apply();
				pRenderer->DrawIndexedInstanced(instanceID);
			} while (batchCount++ < renderList.size() / DRAW_INSTANCED_COUNT_GBUFFER_PASS);
		}
#endif
	}
}

void DeferredRenderingPasses::RenderLightingPass(const RenderParams& args) const
{
	ClearCommand cmd = ClearCommand::Color({ 0, 0, 0, 0 });

	const bool bAmbientOcclusionOn = args.tSSAO == -1;
	Renderer* pRenderer = args.pRenderer;

	const vec2 screenSize = pRenderer->GetWindowDimensionsAsFloat2();
	const TextureID texNormal = pRenderer->GetRenderTargetTexture(_GBuffer.mRTNormals);
	const TextureID texDiffuseRoughness = pRenderer->GetRenderTargetTexture(_GBuffer.mRTDiffuseRoughness);
	const TextureID texSpecularMetallic = pRenderer->GetRenderTargetTexture(_GBuffer.mRTSpecularMetallic);
	const TextureID texEmissive = pRenderer->GetRenderTargetTexture(_GBuffer.mRTEmissive);
	const ShaderID lightingShader = args.bUseBRDFLighting ? _BRDFLightingShader : _phongLightingShader;
	const TextureID texIrradianceMap = args.sceneView.environmentMap.irradianceMap;
	const SamplerID smpEnvMap = args.sceneView.environmentMap.envMapSampler;
	const TextureID texSpecularMap = args.sceneView.environmentMap.prefilteredEnvironmentMap;
	const TextureID tBRDFLUT = EnvironmentMap::sBRDFIntegrationLUTTexture;
	const TextureID depthTexture = pRenderer->GetDepthTargetTexture(ENGINE->GetWorldDepthTarget());

	const auto IABuffersQuad = SceneResourceView::GetBuiltinMeshVertexAndIndexBufferID(EGeometry::FULLSCREENQUAD);
	constexpr bool bUnbindRenderTargets = false; // we're switching between lighting shaders w/ same render targets

	pRenderer->UnbindDepthTarget();
	pRenderer->SetRasterizerState(EDefaultRasterizerState::CULL_BACK);
	pRenderer->BindRenderTarget(_shadeTarget);
	pRenderer->BeginRender(cmd);
	pRenderer->SetViewport(pRenderer->FrameRenderTargetWidth(), pRenderer->FrameRenderTargetHeight());
	pRenderer->Apply();

	// AMBIENT LIGHTING
	//-----------------------------------------------------------------------------------------
	const bool bSkylight = args.sceneView.bIsIBLEnabled && texIrradianceMap != -1;
	if (bSkylight)
	{
		pRenderer->BeginEvent("Environment Map Lighting Pass");
#if USE_COMPUTE_PASS_UNIT_TEST || USE_COMPUTE_SSAO
		pRenderer->SetShader(_ambientIBLShader, bUnbindRenderTargets, true);
#else
		pRenderer->SetShader(_ambientIBLShader, bUnbindRenderTargets, false);
#endif
		pRenderer->SetTexture("tDiffuseRoughnessMap", texDiffuseRoughness);
		pRenderer->SetTexture("tSpecularMetalnessMap", texSpecularMetallic);
		pRenderer->SetTexture("tNormalMap", texNormal);
		pRenderer->SetTexture("tDepthMap", depthTexture);
		pRenderer->SetTexture("tAmbientOcclusion", args.tSSAO);
		pRenderer->SetTexture("tIrradianceMap", texIrradianceMap);
		pRenderer->SetTexture("tPreFilteredEnvironmentMap", texSpecularMap);
		pRenderer->SetTexture("tBRDFIntegrationLUT", tBRDFLUT);
		pRenderer->SetSamplerState("sEnvMapSampler", smpEnvMap);
		pRenderer->SetSamplerState("sWrapSampler", EDefaultSamplerState::WRAP_SAMPLER);
		pRenderer->SetConstant4x4f("matViewInverse", args.sceneView.viewInverse);
		pRenderer->SetConstant4x4f("matProjInverse", args.sceneView.projInverse);
	}
	else
	{
		pRenderer->BeginEvent("Ambient Pass");
		pRenderer->SetShader(_ambientShader, bUnbindRenderTargets);
		pRenderer->SetTexture("tDiffuseRoughnessMap", texDiffuseRoughness);
		pRenderer->SetTexture("tAmbientOcclusion", args.tSSAO);
	}
	pRenderer->SetSamplerState("sNearestSampler", EDefaultSamplerState::POINT_SAMPLER);
	pRenderer->SetConstant1f("ambientFactor", args.sceneView.sceneRenderSettings.ssao.ambientFactor);
	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	pRenderer->Apply();
	pRenderer->DrawIndexed();
	pRenderer->EndEvent();


	// DIFFUSE & SPECULAR LIGHTING
	//-----------------------------------------------------------------------------------------
	pRenderer->BeginEvent("Lighting Pass");
	pRenderer->SetBlendState(EDefaultBlendState::ADDITIVE_COLOR);

	// draw fullscreen quad for lighting for now. Will add light volumes
	// as the scene gets more complex or depending on performance needs.
#ifdef USE_LIGHT_VOLUMES
#if 0
	const auto IABuffersSphere = SceneResourceView::GetBuiltinMeshVertexAndIndexBufferID(EGeometry::SPHERE);

	pRenderer->SetConstant3f("CameraWorldPosition", sceneView.pCamera->GetPositionF());
	pRenderer->SetConstant2f("ScreenSize", screenSize);
	pRenderer->SetTexture("texDiffuseRoughnessMap", texDiffuseRoughness);
	pRenderer->SetTexture("texSpecularMetalnessMap", texSpecularMetallic);
	pRenderer->SetTexture("texNormals", texNormal);

	// POINT LIGHTS
	pRenderer->SetShader(SHADERS::DEFERRED_BRDF_POINT);
	pRenderer->SetVertexBuffer(IABuffersSphere.first);
	pRenderer->SetIndexBuffer(IABuffersSphere.second);
	//pRenderer->SetRasterizerState(ERasterizerCullMode::)
	for (const Light& light : lights)
	{
		if (light._type == Light::ELightType::POINT)
		{
			const float& r = light._range;	//bounding sphere radius
			const vec3&  pos = light._transform._position;
			const XMMATRIX world = {
				r, 0, 0, 0,
				0, r, 0, 0,
				0, 0, r, 0,
				pos.x(), pos.y(), pos.z(), 1,
			};

			const XMMATRIX wvp = world * sceneView.viewProj;
			const LightShaderSignature lightData = light.ShaderSignature();
			pRenderer->SetConstant4x4f("worldViewProj", wvp);
			pRenderer->SetConstantStruct("light", &lightData);
			pRenderer->Apply();
			pRenderer->DrawIndexed();
		}
	}
#endif

	// SPOT LIGHTS
#if 0
	pRenderer->SetShader(SHADERS::DEFERRED_BRDF);
	pRenderer->SetConstant3f("cameraPos", m_pCamera->GetPositionF());

	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);

	// for spot lights

	pRenderer->Apply();
	pRenderer->DrawIndexed();
#endif

#else
	pRenderer->SetShader(lightingShader, bUnbindRenderTargets);
	ENGINE->SendLightData();

	pRenderer->SetConstant4x4f("matView", args.sceneView.view);
	pRenderer->SetConstant4x4f("matViewToWorld", args.sceneView.viewInverse);
	pRenderer->SetConstant4x4f("directionalProj", args.sceneView.directionalLightProjection);
	pRenderer->SetConstant4x4f("matProjInverse", args.sceneView.projInverse);
	//pRenderer->SetSamplerState("sNearestSampler", EDefaultSamplerState::POINT_SAMPLER);
	pRenderer->SetSamplerState("sLinearSampler", EDefaultSamplerState::LINEAR_FILTER_SAMPLER);
	pRenderer->SetSamplerState("sShadowSampler", EDefaultSamplerState::POINT_SAMPLER);
	pRenderer->SetTexture("texDiffuseRoughnessMap", texDiffuseRoughness);
	pRenderer->SetTexture("texSpecularMetalnessMap", texSpecularMetallic);
	pRenderer->SetTexture("texNormals", texNormal);
	pRenderer->SetTexture("texEmissiveMap", texEmissive);
	pRenderer->SetTexture("texDepth", depthTexture);
	pRenderer->SetVertexBuffer(IABuffersQuad.first);
	pRenderer->SetIndexBuffer(IABuffersQuad.second);
	pRenderer->Apply();
	pRenderer->DrawIndexed();
#endif	// light volumes
	pRenderer->EndEvent(); // Lighting Pass
	pRenderer->SetBlendState(EDefaultBlendState::DISABLED);



#if ENABLE_TRANSPARENCY
	mpGPUProfiler->BeginEntry("Opaque Pass (ScreenSpace)");
	mpCPUProfiler->BeginEntry("Opaque Pass (ScreenSpace)");



	mpCPUProfiler->EndEntry();
	mpGPUProfiler->EndEntry();



	// Untested. to be refactored when transparency is fully implemented.

	// TRANSPARENT OBJECTS - FORWARD RENDER
	mpGPUProfiler->BeginEntry("Alpha Pass (Forward)");
	mpCPUProfiler->BeginEntry("Alpha Pass (Forward)");
	{
		mpRenderer->BindDepthTarget(GetWorldDepthTarget());
		mpRenderer->SetShader(EShaders::FORWARD_BRDF);
		mpRenderer->SetDepthStencilState(EDefaultDepthStencilState::DEPTH_STENCIL_WRITE);
		//mpRenderer->SetBlendState(EDefaultBlendState::ADDITIVE_COLOR);


		mpRenderer->SetConstant1f("ambientFactor", mSceneView.sceneRenderSettings.ambientFactor);
		mpRenderer->SetConstant3f("cameraPos", mSceneView.cameraPosition);
		mpRenderer->SetConstant2f("screenDimensions", mpRenderer->GetWindowDimensionsAsFloat2());
		const TextureID texIrradianceMap = mSceneView.environmentMap.irradianceMap;
		const SamplerID smpEnvMap = mSceneView.environmentMap.envMapSampler < 0 ? EDefaultSamplerState::POINT_SAMPLER : mSceneView.environmentMap.envMapSampler;
		const TextureID prefilteredEnvMap = mSceneView.environmentMap.prefilteredEnvironmentMap;
		const TextureID tBRDFLUT = mpRenderer->GetRenderTargetTexture(EnvironmentMap::sBRDFIntegrationLUTRT);
		const bool bSkylight = mSceneView.bIsIBLEnabled && texIrradianceMap != -1;
		if (bSkylight)
		{
			mpRenderer->SetTexture("tIrradianceMap", texIrradianceMap);
			mpRenderer->SetTexture("tPreFilteredEnvironmentMap", prefilteredEnvMap);
			mpRenderer->SetTexture("tBRDFIntegrationLUT", tBRDFLUT);
			mpRenderer->SetSamplerState("sEnvMapSampler", smpEnvMap);
		}

		mpRenderer->SetConstant1f("isEnvironmentLightingOn", bSkylight ? 1.0f : 0.0f);
		mpRenderer->SetSamplerState("sWrapSampler", EDefaultSamplerState::WRAP_SAMPLER);
		mpRenderer->SetSamplerState("sNearestSampler", EDefaultSamplerState::POINT_SAMPLER);
		mpRenderer->SetSamplerState("sLinearSampler", EDefaultSamplerState::LINEAR_FILTER_SAMPLER_WRAP_UVW);

		mpRenderer->Apply();
		mFrameStats.numSceneObjects += mpActiveScene->RenderAlpha(mSceneView);

	}
	mpCPUProfiler->EndEntry();
	mpGPUProfiler->EndEntry();
#endif
}



