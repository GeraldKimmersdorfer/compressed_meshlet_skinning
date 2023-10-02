#include "VertexPulledIndirectPipeline.h"
#include <vk_convenience_functions.hpp>

avk::command::action_type_command draw_indexed_indirect_nobind(const avk::buffer_t& aParametersBuffer, const avk::buffer_t& aIndexBuffer, uint32_t aNumberOfDraws, vk::DeviceSize aParametersOffset, uint32_t aParametersStride)
{
	using namespace avk::command;
	const auto& indexMeta = aIndexBuffer.template meta<avk::index_buffer_meta>();
	vk::IndexType indexType;
	switch (indexMeta.sizeof_one_element()) {
		case sizeof(uint16_t) : indexType = vk::IndexType::eUint16; break;
			case sizeof(uint32_t) : indexType = vk::IndexType::eUint32; break;
			default: AVK_LOG_ERROR("The given size[" + std::to_string(indexMeta.sizeof_one_element()) + "] does not correspond to a valid vk::IndexType"); break;
	}

	return action_type_command{
		avk::sync::sync_hint {
			{{ // DESTINATION dependencies for previous commands:
				vk::PipelineStageFlagBits2KHR::eAllGraphics,
				vk::AccessFlagBits2KHR::eInputAttachmentRead | vk::AccessFlagBits2KHR::eColorAttachmentRead | vk::AccessFlagBits2KHR::eColorAttachmentWrite | vk::AccessFlagBits2KHR::eDepthStencilAttachmentRead | vk::AccessFlagBits2KHR::eDepthStencilAttachmentWrite
			}},
			{{ // SOURCE dependencies for subsequent commands:
				vk::PipelineStageFlagBits2KHR::eAllGraphics,
				vk::AccessFlagBits2KHR::eColorAttachmentWrite | vk::AccessFlagBits2KHR::eDepthStencilAttachmentWrite
			}}
		},
		{}, // no resource-specific sync hints
		[
			indexType,
			lParametersBufferHandle = aParametersBuffer.handle(),
			lIndexBufferHandle = aIndexBuffer.handle(),
			aNumberOfDraws, aParametersOffset, aParametersStride
		](avk::command_buffer_t& cb) {
			cb.handle().bindIndexBuffer(lIndexBufferHandle, 0u, indexType);
			cb.handle().drawIndexedIndirect(lParametersBufferHandle, aParametersOffset, aNumberOfDraws, aParametersStride);
		}
	};
}

VertexPulledIndirectPipeline::VertexPulledIndirectPipeline(SharedData* shared)
	:PipelineInterface(shared, "Vertex Pulled Indirect")
{
}

void VertexPulledIndirectPipeline::doInitialize(avk::queue* queue)
{
	auto gpuDrawCommands = std::vector<VkDrawIndexedIndirectCommand>(mShared->mMeshData.size());
	for (int i = 0; i < gpuDrawCommands.size(); i++) {
		gpuDrawCommands[i] = VkDrawIndexedIndirectCommand{
			.indexCount = mShared->mMeshData[i].mIndexCount,
			.instanceCount = 1,
			.firstIndex = mShared->mMeshData[i].mIndexOffset,
			.vertexOffset = static_cast<int32_t>(mShared->mMeshData[i].mVertexOffset),	// Note: Not strictly necessary, we could also set it to 0 and pull the vertex offset from the mesh buffer
			.firstInstance = static_cast<uint32_t>(i),							// we missuse that such that we know where to access the mesh array in the shader
		};
	}
	mIndirectDrawCommandBuffer = avk::context().create_buffer(avk::memory_usage::device, {},
		avk::indirect_buffer_meta::create_from_data(gpuDrawCommands),
		avk::storage_buffer_meta::create_from_data(gpuDrawCommands)
	);
	avk::context().record_and_submit_with_fence({
			mIndirectDrawCommandBuffer->fill(gpuDrawCommands.data(), 0)
		}, *queue
	)->wait_until_signalled();

	mPipeline = avk::context().create_graphics_pipeline_for(
		avk::vertex_shader("shaders/transform_and_pass_pulled_pos_nrm_uv.vert"),
		avk::fragment_shader("shaders/diffuse_shading_fixed_lightsource.frag"),
		avk::cfg::front_face::define_front_faces_to_be_counter_clockwise(),
		avk::cfg::viewport_depth_scissors_config::from_framebuffer(avk::context().main_window()->backbuffer_reference_at_index(0)),
		avk::context().create_renderpass({
			avk::attachment::declare(avk::format_from_window_color_buffer(avk::context().main_window()), avk::on_load::clear.from_previous_layout(avk::layout::undefined), avk::usage::color(0)     , avk::on_store::store),
			avk::attachment::declare(avk::format_from_window_depth_buffer(avk::context().main_window()), avk::on_load::clear.from_previous_layout(avk::layout::undefined), avk::usage::depth_stencil, avk::on_store::dont_care)
			}, avk::context().main_window()->renderpass_reference().subpass_dependencies()),
		avk::descriptor_binding(0, 0, avk::as_combined_image_samplers(mShared->mImageSamplers, avk::layout::shader_read_only_optimal)),
		avk::descriptor_binding(0, 1, mShared->mViewProjBuffers[0]),
		avk::descriptor_binding(0, 2, mShared->mConfigurationBuffer),
		avk::descriptor_binding(1, 0, mShared->mMaterialsBuffer),
		avk::descriptor_binding(2, 0, mShared->mBoneTransformBuffers[0]),
		avk::descriptor_binding(3, 0, mShared->mVertexBuffer),
		avk::descriptor_binding(4, 1, mShared->mMeshesBuffer)
	);
	mShared->mSharedUpdater->on(avk::shader_files_changed_event(mPipeline.as_reference())).update(mPipeline);
}

avk::command::action_type_command VertexPulledIndirectPipeline::render(int64_t inFlightIndex)
{
	using namespace avk;
	return command::render_pass(mPipeline->renderpass_reference(), context().main_window()->current_backbuffer_reference(), {
				command::bind_pipeline(mPipeline.as_reference()),
				command::bind_descriptors(mPipeline->layout(), mDescriptorCache->get_or_create_descriptor_sets({
					descriptor_binding(0, 0, as_combined_image_samplers(mShared->mImageSamplers, layout::shader_read_only_optimal)),
					descriptor_binding(0, 1, mShared->mViewProjBuffers[inFlightIndex]),
					descriptor_binding(0, 2, mShared->mConfigurationBuffer),
					descriptor_binding(1, 0, mShared->mMaterialsBuffer),
					descriptor_binding(2, 0, mShared->mBoneTransformBuffers[inFlightIndex]),
					descriptor_binding(3, 0, mShared->mVertexBuffer),
					descriptor_binding(4, 1, mShared->mMeshesBuffer)
				})),

				draw_indexed_indirect_nobind(
					mIndirectDrawCommandBuffer.as_reference(),
					mShared->mIndexBuffer.as_reference(),
					mShared->mMeshData.size(),
					0,
					static_cast<uint32_t>(sizeof(VkDrawIndexedIndirectCommand))
				),
		});
}

void VertexPulledIndirectPipeline::hud_config(bool& config_has_changed)
{
	config_has_changed |= ImGui::Checkbox("Highlight meshes", (bool*)(void*)&mShared->mConfig.mOverlayMeshlets);
}

void VertexPulledIndirectPipeline::doDestroy()
{
	mIndirectDrawCommandBuffer = avk::buffer();
}
