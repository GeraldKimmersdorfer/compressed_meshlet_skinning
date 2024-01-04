#include "NoCompression.h"

void NoCompression::doCompress(avk::queue* queue)
{
	
	mVertexBuffer = avk::context().create_buffer(avk::memory_usage::device,
		VULKAN_HPP_NAMESPACE::BufferUsageFlagBits::eVertexBuffer,
		avk::storage_buffer_meta::create_from_data(mShared->mVertexData)
	);
	avk::context().record_and_submit_with_fence({ mVertexBuffer->fill(mShared->mVertexData.data(), 0) }, *queue)->wait_until_signalled();
	mAdditionalStaticDescriptorBindings.push_back(avk::descriptor_binding(3, 0, mShared->mVertexBuffer));

	// report to props:
	mShared->mPropertyManager->get("lut_size")->setUint(0);
	mShared->mPropertyManager->get("lut_count")->setUint(0);
	mShared->mPropertyManager->get("vb_size")->setUint(sizeof(vertex_data) * mShared->mVertexData.size());
	mShared->mPropertyManager->get("amb_size")->setUint(0);
}

void NoCompression::doDestroy()
{
	mVertexBuffer = avk::buffer();
}
