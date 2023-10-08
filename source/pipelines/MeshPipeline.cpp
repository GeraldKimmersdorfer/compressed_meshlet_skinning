#include "MeshPipeline.h"

#include "imgui.h"
#include <vk_convenience_functions.hpp>
#include "../shadercompiler/ShaderMetaCompiler.h"
#include "../avk_extensions.hpp"
#include "../meshletbuilder/MeshletbuilderInterface.h"
#include "../vertexcompressor/VertexCompressionInterface.h"

MeshPipeline::MeshPipeline(SharedData* shared)
	:PipelineInterface(shared, "Meshlet")
{
}

void MeshPipeline::doInitialize(avk::queue* queue)
{
	compile(); // ToDo: This is stupid: I now compile two times when changing pipeline, but its necessary when changing vertex compressor. Better solution?
	if (mShadersRecompiled) {
		mMeshletExtension.first = mMeshletExtension.second;
		mMeshletType.first = mMeshletType.second;
		mShadersRecompiled = false;
	}
	auto builder = mShared->getCurrentMeshletBuilder();
	builder->generate();
	if (mMeshletType.first == _NATIVE) {
		auto& meshletsNative = builder->getMeshletsNative();
		mMeshletsBuffer = avk::context().create_buffer(avk::memory_usage::device, {}, avk::storage_buffer_meta::create_from_data(meshletsNative));
		avk::context().record_and_submit_with_fence({ mMeshletsBuffer->fill(meshletsNative.data(), 0) }, *queue)->wait_until_signalled();
		mShared->mConfig.mMeshletsCount = meshletsNative.size();
	}
	else if (mMeshletType.first == _REDIR) {
		auto& [meshletsRedirect, redirectIndexBuffer] = builder->getMeshletsRedirect();
		mMeshletsBuffer = avk::context().create_buffer(avk::memory_usage::device, {}, avk::storage_buffer_meta::create_from_data(meshletsRedirect));
		avk::context().record_and_submit_with_fence({ mMeshletsBuffer->fill(meshletsRedirect.data(), 0) }, *queue)->wait_until_signalled();
		mPackedIndexBuffer = avk::context().create_buffer(avk::memory_usage::device, {}, avk::storage_buffer_meta::create_from_data(redirectIndexBuffer));
		avk::context().record_and_submit_with_fence({ mPackedIndexBuffer->fill(redirectIndexBuffer.data(), 0) }, *queue)->wait_until_signalled();
		mShared->mConfig.mMeshletsCount = meshletsRedirect.size();
	}

	mTaskInvocations = mMeshletExtension.first == _NV ? mShared->mPropsMeshShaderNV.maxTaskWorkGroupInvocations : mShared->mPropsMeshShader.maxPreferredTaskWorkGroupInvocations;
	mMeshInvocations = mMeshletExtension.first == _NV ? mShared->mPropsMeshShaderNV.maxMeshWorkGroupInvocations : mShared->mPropsMeshShader.maxPreferredMeshWorkGroupInvocations;

	mShared->uploadConfig();

	auto sharedPipelineConfig = avk::create_graphics_pipeline_config(
		avk::task_shader(mPathTaskShader, "main", true).set_specialization_constant(0, mTaskInvocations),
		avk::mesh_shader(mPathMeshShader, "main", true).set_specialization_constant(0, mTaskInvocations).set_specialization_constant(1, mMeshInvocations),
		avk::fragment_shader(mPathFragmentShader, "main", true)
	);

	mAdditionalStaticDescriptorBindings.push_back(std::move(avk::descriptor_binding(4, 0, mMeshletsBuffer)));
	if (mMeshletType.first == _REDIR) {
		mAdditionalStaticDescriptorBindings.push_back(std::move(avk::descriptor_binding(4, 2, mPackedIndexBuffer)));
	}

	// Add shared pipeline configuration
	mShared->attachSharedPipelineConfiguration(&sharedPipelineConfig, &mAdditionalStaticDescriptorBindings);

	auto vCompressor = mShared->getCurrentVertexCompressor();
	vCompressor->compress(queue);
	auto vertexBindings = vCompressor->getBindings();
	mAdditionalStaticDescriptorBindings.insert(mAdditionalStaticDescriptorBindings.end(), vertexBindings.begin(), vertexBindings.end());

	// Add static descriptor bindings to pipeline definition
	for (auto& db : mAdditionalStaticDescriptorBindings) {
		sharedPipelineConfig.mResourceBindings.push_back(std::move(db));
	}
	mPipeline = avk::context().create_graphics_pipeline(std::move(sharedPipelineConfig));
}

avk::command::action_type_command MeshPipeline::render(int64_t inFlightIndex)
{
	using namespace avk;
	return command::render_pass(mPipeline->renderpass_reference(), context().main_window()->current_backbuffer_reference(), {
				command::bind_pipeline(mPipeline.as_reference()),
				command::bind_descriptors(mPipeline->layout(), mShared->mDescriptorCache->get_or_create_descriptor_sets(
					avk::mergeVectors(
						std::vector<avk::binding_data>{
							descriptor_binding(0, 0, as_combined_image_samplers(mShared->mImageSamplers, layout::shader_read_only_optimal))
						},
						mAdditionalStaticDescriptorBindings,
						mShared->getDynamicDescriptorBindings(inFlightIndex)
					))),
				command::conditional(
					[this]() { return mMeshletExtension.first == _NV; },
					[this]() { return command::draw_mesh_tasks_nv(div_ceil(mShared->mConfig.mMeshletsCount, mTaskInvocations), 0); },
					[this]() { return command::draw_mesh_tasks_ext(div_ceil(mShared->mConfig.mMeshletsCount, mTaskInvocations), 1, 1); }
				)
		});
}

void MeshPipeline::hud_config(bool& config_has_changed)
{
	config_has_changed |= ImGui::Checkbox("Highlight meshlets", (bool*)(void*)&mShared->mConfig.mOverlayMeshlets);
}

void MeshPipeline::hud_setup(bool& config_has_changes)
{
	ImGui::Combo("Extension", (int*)(void*)&(mMeshletExtension.second), "EXT\0NV\0");
	ImGui::Combo("Meshlet-Type", (int*)(void*)&(mMeshletType.second), "Native\0Redirected\0");
}

void MeshPipeline::compile()
{
	mPathTaskShader = ShaderMetaCompiler::precompile("meshlet.task", {
		{"MESHLET_EXTENSION", MCC_to_string(mMeshletExtension.second)}
		});
	mPathMeshShader = ShaderMetaCompiler::precompile("meshlet.mesh", {
		{"MESHLET_EXTENSION", MCC_to_string(mMeshletExtension.second)},
		{"MESHLET_TYPE", MCC_to_string(mMeshletType.second)},
		{"VERTEX_COMPRESSION", mShared->getCurrentVertexCompressor()->getMccId()}
		});
	mPathFragmentShader = ShaderMetaCompiler::precompile("diffuse_shading_fixed_lightsource.frag", {});
	mShadersRecompiled = true;
}

void MeshPipeline::doDestroy()
{
	mShared->getCurrentVertexCompressor()->destroy();
	mPipeline = avk::graphics_pipeline();
	mMeshletsBuffer = avk::buffer();
	mPackedIndexBuffer = avk::buffer();
	mAdditionalStaticDescriptorBindings.clear();
}
