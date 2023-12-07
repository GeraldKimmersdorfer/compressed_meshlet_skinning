#include <stdlib.h>
#include <configure_and_compose.hpp>
#include <imgui_manager.hpp>
#include <sequential_invoker.hpp>

#include "MeshletsApp.h"
#include "helpers/packing.h"
#include <glm/gtx/string_cast.hpp>

#include "helpers/lut.h"

int main() // <== Starting point ==
{
	int result = EXIT_FAILURE;
	try {
		// Create a window and open it
		auto mainWnd = avk::context().create_window("Skinned Meshlet Playground");

		mainWnd->set_resolution({ 1600, 900 });
		mainWnd->enable_resizing(true);
		mainWnd->set_additional_back_buffer_attachments({
			avk::attachment::declare(vk::Format::eD32Sfloat, avk::on_load::clear.from_previous_layout(avk::layout::undefined), avk::usage::depth_stencil, avk::on_store::dont_care)
			});
		mainWnd->set_presentaton_mode(avk::presentation_mode::fifo);
		mainWnd->set_number_of_concurrent_frames(3u);
		mainWnd->open();

		// Change Icon of the Window
		if (std::filesystem::exists({ "assets/icon_small.png" })) {
			GLFWimage images[1];
			images[0].pixels = stbi_load("assets/icon_small.png", &images[0].width, &images[0].height, 0, 4);
			glfwSetWindowIcon(mainWnd->handle()->mHandle, 1, images);
			stbi_image_free(images[0].pixels);
		}

		auto& singleQueue = avk::context().create_queue({}, avk::queue_selection_preference::versatile_queue, mainWnd);
		mainWnd->set_queue_family_ownership(singleQueue.family_index());
		mainWnd->set_present_queue(singleQueue);

		// Create an instance of our main avk::element which contains all the functionality:
		auto app = MeshletsApp(singleQueue);
		// Create another element for drawing the UI with ImGui
		auto ui = avk::imgui_manager(singleQueue);

		// Compile all the configuration parameters and the invokees into a "composition":
		auto composition = configure_and_compose(
			avk::application_name("Skinned Meshlet Playground"),
			// Gotta enable the mesh shader extension, ...
			avk::required_device_extensions(VK_EXT_MESH_SHADER_EXTENSION_NAME),
			avk::required_device_extensions(VK_EXT_MULTI_DRAW_EXTENSION_NAME),
			avk::required_device_extensions(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME),
			avk::optional_device_extensions(VK_NV_MESH_SHADER_EXTENSION_NAME),
			// ... and enable the mesh shader features that we need:
			[](vk::PhysicalDeviceMeshShaderFeaturesEXT& meshShaderFeatures) {
				meshShaderFeatures.setMeshShader(VK_TRUE);
				meshShaderFeatures.setTaskShader(VK_TRUE);
				meshShaderFeatures.setMeshShaderQueries(VK_TRUE);
			},
			[](vk::PhysicalDeviceFeatures& features) {
				features.setPipelineStatisticsQuery(VK_TRUE);
				features.setMultiDrawIndirect(VK_TRUE);
			},
			[](vk::PhysicalDeviceVulkan12Features& features) {
				features.setUniformAndStorageBuffer8BitAccess(VK_TRUE);
				features.setStorageBuffer8BitAccess(VK_TRUE);
			},
			[](vk::PhysicalDeviceVulkan11Features& features) {
				features.setStorageBuffer16BitAccess(VK_TRUE);
			},
			// Pass windows:
			mainWnd,
			// Pass invokees:
			app, ui
		);

		// Create an invoker object, which defines the way how invokees/elements are invoked
		// (In this case, just sequentially in their execution order):
		avk::sequential_invoker invoker;

		// With everything configured, let us start our render loop:
		composition.start_render_loop(
			// Callback in the case of update:
			[&invoker](const std::vector<avk::invokee*>& aToBeInvoked) {
				// Call all the update() callbacks:
				invoker.invoke_updates(aToBeInvoked);
			},
			// Callback in the case of render:
			[&invoker](const std::vector<avk::invokee*>& aToBeInvoked) {
				// Sync (wait for fences and so) per window BEFORE executing render callbacks
				avk::context().execute_for_each_window([](avk::window* wnd) {
					wnd->sync_before_render();
					});

				// Call all the render() callbacks:
				invoker.invoke_renders(aToBeInvoked);

				// Render per window:
				avk::context().execute_for_each_window([](avk::window* wnd) {
					wnd->render_frame();
					});
			}
		); // This is a blocking call, which loops until avk::current_composition()->stop(); has been called (see update())

		result = EXIT_SUCCESS;
	}
	catch (avk::logic_error&) {}
	catch (avk::runtime_error&) {}
	return result;
}