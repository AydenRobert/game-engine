#include "renderer/vulkan/vulkan_backend.h"

#include "defines.h"
#include "math/math_types.h"
#include "renderer/vulkan/shaders/vulkan_ui_shader.h"
#include "renderer/vulkan/vulkan_buffer.h"
#include "renderer/vulkan/vulkan_command_buffer.h"
#include "renderer/vulkan/vulkan_device.h"
#include "renderer/vulkan/vulkan_image.h"
#include "renderer/vulkan/vulkan_pipeline.h"
#include "renderer/vulkan/vulkan_platform.h"
#include "renderer/vulkan/vulkan_renderpass.h"
#include "renderer/vulkan/vulkan_shader_types.inl"
#include "renderer/vulkan/vulkan_swapchain.h"
#include "renderer/vulkan/vulkan_types.inl"
#include "renderer/vulkan/vulkan_utils.h"

#include "renderer/vulkan/shaders/vulkan_material_shader.h"

#include "core/application.h"
#include "core/kmemory.h"
#include "core/kstring.h"
#include "core/logger.h"

#include "containers/darray.h"
#include "systems/material_system.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"

#include <limits.h>
#include <vulkan/vulkan_core.h>

static vulkan_context context;
static u32 cached_framebuffer_width = 0;
static u32 cached_framebuffer_height = 0;

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);

i32 find_memory_index(u32 type_filter, u32 property_flags);
b8 create_buffers(vulkan_context *context);

void create_command_buffers(renderer_backend *backend);
void regenerate_framebuffers();
b8 recreate_swapchain(renderer_backend *backend);

b8 create_module(vulkan_shader *shader, vulkan_shader_stage_config config,
                 vulkan_shader_stage *shader_stage);

b8 upload_data_range(vulkan_context *context, VkCommandPool pool, VkFence fence,
                     VkQueue queue, vulkan_buffer *buffer, u64 *out_offset,
                     u64 size, const void *data) {
    // Allocate space in the buffer
    if (!vulkan_buffer_allocate(buffer, size, out_offset)) {
        KERROR("upload_data_range - failed to allocate space in buffer.");
    }
    // Create a host visible staging buffer to upload to. Mark it as the source
    // of the transfer.
    VkBufferUsageFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    vulkan_buffer staging;
    vulkan_buffer_create(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, flags,
                         true, &staging);

    // Load data into the staging buffer
    vulkan_buffer_load_data(context, &staging, 0, size, 0, data);

    // Perform the copy
    vulkan_buffer_copy_to(context, pool, fence, queue, staging.handle, 0,
                          buffer->handle, *out_offset, size);

    // Clean up the staging buffer
    vulkan_buffer_destroy(context, &staging);

    return true;
}

void free_data_range(vulkan_buffer *buffer, u64 offset, u64 size) {
    if (!buffer) {
        return;
    }

    vulkan_buffer_free(buffer, size, offset);
}

b8 vulkan_renderer_backend_initialize(renderer_backend *backend,
                                      const char *application_name,
                                      struct platform_state *plat_state) {
    // Function pointers
    context.find_memory_index = find_memory_index;

    // Sets to initial w and h
    application_get_framebuffer_size(&cached_framebuffer_width,
                                     &cached_framebuffer_height);
    context.framebuffer_width =
        (cached_framebuffer_width != 0) ? cached_framebuffer_width : 800;
    context.framebuffer_height =
        (cached_framebuffer_height != 0) ? cached_framebuffer_height : 600;
    cached_framebuffer_width = 0;
    cached_framebuffer_height = 0;

    // TODO: custom allocator
    context.allocator = 0;

    // Setup vulkan instance
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pNext = NULL;
    app_info.apiVersion = VK_API_VERSION_1_2;
    app_info.pApplicationName = application_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Kohi Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pNext = NULL;
    create_info.pApplicationInfo = &app_info;

    // Obtain a list of required extensions
    const char **required_extensions = darray_create(const char *);
    darray_push(required_extensions, &VK_KHR_SURFACE_EXTENSION_NAME);
    platform_get_required_extension_names(&required_extensions);
#if defined(_DEBUG)
    darray_push(required_extensions, &VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    KDEBUG("Required Extensions:");
    u32 length = darray_length(required_extensions);
    for (u32 i = 0; i < length; i++) {
        KDEBUG(required_extensions[i]);
    }
#endif // defined(_DEBUG)

    create_info.enabledExtensionCount = darray_length(required_extensions);
    create_info.ppEnabledExtensionNames = required_extensions;

    // Validation layers
    const char **required_validation_layer_names = 0;
    u32 required_validation_layer_count = 0;

#if defined(_DEBUG)
    KINFO("Validation layers enabled. Enumerating...");

    // The list of validation layers required
    required_validation_layer_names = darray_create(const char *);
    darray_push(required_validation_layer_names,
                &"VK_LAYER_KHRONOS_validation");
    // NOTE: enable this for debugging purposes if needed
    // darray_push(required_validation_layer_names,
    //             &"VK_LAYER_LUNARG_api_dump");
    required_validation_layer_count =
        darray_length(required_validation_layer_names);

    // Obtain list of available validation layers
    u32 available_layer_count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
    VkLayerProperties *available_layers =
        darray_reserve(VkLayerProperties, available_layer_count);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count,
                                                available_layers));

    // Verify all required layers are available
    for (u32 i = 0; i < required_validation_layer_count; i++) {
        KINFO("Searching for layer: %s.", required_validation_layer_names[i]);
        b8 found = false;
        for (u32 j = 0; j < available_layer_count; j++) {
            if (strings_equal(required_validation_layer_names[i],
                              available_layers[j].layerName)) {
                found = true;
                KINFO("Found.");
                break;
            }
        }

        if (!found) {
            KFATAL("Required validation layer is missing: %s.",
                   required_validation_layer_names[i]);
            return false;
        }
    }
    KINFO("All required validation layers are present.");
#endif // defined(_DEBUG)

    create_info.enabledLayerCount = required_validation_layer_count;
    create_info.ppEnabledLayerNames = required_validation_layer_names;

    VK_CHECK(
        vkCreateInstance(&create_info, context.allocator, &context.instance));
    KINFO("Vulkan instance created.");

    // Debugger
#if defined(_DEBUG)
    KDEBUG("Creating Vulkan debugger...");
    u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = log_severity;
    debug_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = vk_debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            context.instance, "vkCreateDebugUtilsMessengerEXT");
    KASSERT_MSG(func, "Failed to create debug messenger!");
    VK_CHECK(func(context.instance, &debug_create_info, context.allocator,
                  &context.debug_messenger));
    KDEBUG("Vulkan debugger created.");
#endif // defined(_DEBUG)

    // Surface creation
    KDEBUG("Creating Vulkan surface...");
    if (!platform_create_vulkan_surface(plat_state, &context)) {
        KERROR("Failed to create platform surface!");
        return false;
    }
    KDEBUG("Vulkan surface created.");

    // Device creation
    if (!vulkan_device_create(&context)) {
        KERROR("Failed to create device!");
        return false;
    }

    // Swapchain
    vulkan_swapchain_create(&context, context.framebuffer_width,
                            context.framebuffer_height, &context.swapchain);

    // World renderpass
    vulkan_renderpass_create(
        &context, &context.main_renderpass,
        (vec4){{0, 0, context.framebuffer_width, context.framebuffer_height}},
        (vec4){{0.0f, 0.0f, 0.2f, 1.0f}}, 1.0f, 0,
        RENDERPASS_CLEAR_FLAG_COLOUR_BUFFER |
            RENDERPASS_CLEAR_FLAG_DEPTH_BUFFER |
            RENDERPASS_CLEAR_FLAG_STENCIL_BUFFER,
        false, true);

    // UI renderpass
    vulkan_renderpass_create(
        &context, &context.ui_renderpass,
        (vec4){{0, 0, context.framebuffer_width, context.framebuffer_height}},
        (vec4){{0.0f, 0.0f, 0.0f, 0.0f}}, 1.0f, 0, RENDERPASS_CLEAR_FLAG_NONE,
        true, false);

    // Swapchain framebuffers
    regenerate_framebuffers();

    create_command_buffers(backend);

    context.image_available_semaphores =
        darray_reserve(VkSemaphore, context.swapchain.max_frames_in_flight);
    context.queue_complete_semaphores =
        darray_reserve(VkSemaphore, context.swapchain.max_frames_in_flight);

    for (u8 i = 0; i < context.swapchain.max_frames_in_flight; i++) {
        VkSemaphoreCreateInfo semaphore_create_info = {
            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        vkCreateSemaphore(context.device.logical_device, &semaphore_create_info,
                          context.allocator,
                          &context.image_available_semaphores[i]);
        vkCreateSemaphore(context.device.logical_device, &semaphore_create_info,
                          context.allocator,
                          &context.queue_complete_semaphores[i]);

        // Create the fence in a signaled state, indicating that the first frame
        // has already been 'rendered'. This will prevent the application from
        // waiting indefinitely for the first frame to render since it cannot be
        // rendered until a frame is 'rendered' before it.
        VkFenceCreateInfo fence_create_info = {
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(context.device.logical_device,
                               &fence_create_info, context.allocator,
                               &context.in_flight_fences[i]));
    }

    // In flight fences should not yet exist at this point; so clear the list.
    // These are stored in pointers because the initial state will be 0, and
    // will be 0 when not in use. Actual fences are not owned by the list.
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        context.images_in_flight[i] = 0; // NULL
    }

    create_buffers(&context);

    // Mark all geometries invalid
    for (u32 i = 0; i < VULKAN_MAX_GEOMETRY_COUNT; i++) {
        context.geometries[i].id = INVALID_ID;
    }

    KINFO("Vulkan renderer initialized successfully!");
    return true;
}

void vulkan_renderer_backend_shutdown(renderer_backend *backend) {
    vkDeviceWaitIdle(context.device.logical_device);

    // Destroy buffers
    vulkan_buffer_destroy(&context, &context.object_vertex_buffer);
    vulkan_buffer_destroy(&context, &context.object_index_buffer);

    // Sync objects
    for (u8 i = 0; i < context.swapchain.max_frames_in_flight; i++) {
        if (context.image_available_semaphores[i]) {
            vkDestroySemaphore(context.device.logical_device,
                               context.image_available_semaphores[i],
                               context.allocator);
            context.image_available_semaphores[i] = 0;
        }
        if (context.queue_complete_semaphores[i]) {
            vkDestroySemaphore(context.device.logical_device,
                               context.queue_complete_semaphores[i],
                               context.allocator);
            context.queue_complete_semaphores[i] = 0;
        }
        vkDestroyFence(context.device.logical_device,
                       context.in_flight_fences[i], context.allocator);
    }
    darray_destroy(context.image_available_semaphores);
    context.image_available_semaphores = 0;

    darray_destroy(context.queue_complete_semaphores);
    context.queue_complete_semaphores = 0;

    // Command buffers
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        if (context.graphics_command_buffers[i].handle) {
            vulkan_command_buffer_free(&context,
                                       context.device.graphics_command_pool,
                                       &context.graphics_command_buffers[i]);
            context.graphics_command_buffers[i].handle = 0;
        }
    }
    darray_destroy(context.graphics_command_buffers);
    context.graphics_command_buffers = 0;

    // Framebuffers
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        vkDestroyFramebuffer(context.device.logical_device,
                             context.world_framebuffers[i], context.allocator);
        vkDestroyFramebuffer(context.device.logical_device,
                             context.swapchain.framebuffers[i],
                             context.allocator);
    }

    // Render
    vulkan_renderpass_destroy(&context, &context.ui_renderpass);
    vulkan_renderpass_destroy(&context, &context.main_renderpass);

    // Swapchain
    vulkan_swapchain_destroy(&context, &context.swapchain);

    KDEBUG("Destroying Vulkan device...");
    vulkan_device_destroy(&context);

    KDEBUG("Destroying Vulkan surface...");
    if (context.surface) {
        vkDestroySurfaceKHR(context.instance, context.surface,
                            context.allocator);
        context.surface = 0;
    }

#if defined(_DEBUG)
    KDEBUG("Destroying Vulkan debugger...");
    if (context.debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT func =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                context.instance, "vkDestroyDebugUtilsMessengerEXT");
        func(context.instance, context.debug_messenger, context.allocator);
    }
#endif // defined(_DEBUG)

    KDEBUG("Destroying Vulkan instance...");
    vkDestroyInstance(context.instance, context.allocator);
}

void vulkan_renderer_backend_on_resized(renderer_backend *backend, u16 width,
                                        u16 height) {
    // Update the 'framebuffer size generation', a counter with indicates when
    // the framebuffer size has been updated.
    cached_framebuffer_width = width;
    cached_framebuffer_height = height;
    context.framebuffer_size_generation++;

    KINFO("Vulkan renderer backend->resize: w/h/gen: %i/%i/%llu", width, height,
          context.framebuffer_size_generation);
}

b8 vulkan_renderer_backend_begin_frame(renderer_backend *backend,
                                       f32 delta_time) {
    context.frame_delta_time = delta_time;
    vulkan_device *device = &context.device;

    if (context.recreating_swapchain) {
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if (!vulkan_result_is_success(result)) {
            KERROR("vulkan_renderer_backend_begin_frame vkDeviceWaitIdle (1) "
                   "failed: '&s'",
                   vulkan_result_string(result, true));
            return false;
        }
        KINFO("Recreating swapchain, booting.");
        return false;
    }

    // Check if the framebuffer has been resized, if so, a new swapchain must be
    // created.
    if (context.framebuffer_size_generation !=
        context.framebuffer_size_last_generation) {
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if (!vulkan_result_is_success(result)) {
            KERROR("vulkan_renderer_backend_begin_frame vkDeviceWaitIdle (2) "
                   "failed: '&s'",
                   vulkan_result_string(result, true));
            return false;
        }

        // If the swapchain recreation failed (e.g. minimizing window), boot out
        // before unsetting the flag.
        if (!recreate_swapchain(backend)) {
            return false;
        }

        KINFO("Resized, booting.");
        return false;
    }

    // Wait for the execution of the current frame to complete. The fence being
    // free will allow this once to move on.
    VkResult result = vkWaitForFences(
        context.device.logical_device, 1,
        &context.in_flight_fences[context.current_frame], true, UINT64_MAX);
    if (!vulkan_result_is_success(result)) {
        KERROR("In flight fence wait failure! error: '%s'.",
               vulkan_result_string(result, true));
        return false;
    }

    // Acquire the next chain image from the swapchain. Pass on the semaphore
    // that should be signaled with this completes. This same semaphore will
    // later be waited on by the queue submission to ensure this image is
    // available.
    if (!vulkan_swapchain_acquire_next_image_index(
            &context, &context.swapchain, UINT64_MAX,
            context.image_available_semaphores[context.current_frame], 0,
            &context.image_index)) {
        KERROR("Failed to acquire next image index, booting.");
        return false; // Shouldn't happen...
    }

    // Begin recording commands.
    vulkan_command_buffer *command_buffer =
        &context.graphics_command_buffers[context.image_index];
    vulkan_command_buffer_reset(command_buffer);
    vulkan_command_buffer_begin(command_buffer, false, false, false);

    // Dynamic state
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (f32)context.framebuffer_height;
    viewport.width = (f32)context.framebuffer_width;
    viewport.height = -(f32)context.framebuffer_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = context.framebuffer_width;
    scissor.extent.height = context.framebuffer_height;

    vkCmdSetViewport(command_buffer->handle, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer->handle, 0, 1, &scissor);

    context.main_renderpass.render_area.z = context.framebuffer_width;
    context.main_renderpass.render_area.w = context.framebuffer_height;

    context.ui_renderpass.render_area.z = context.framebuffer_width;
    context.ui_renderpass.render_area.w = context.framebuffer_height;

    return true;
}

b8 vulkan_renderer_backend_end_frame(renderer_backend *backend,
                                     f32 delta_time) {
    vulkan_command_buffer *command_buffer =
        &context.graphics_command_buffers[context.image_index];

    vulkan_command_buffer_end(command_buffer);

    // Make sure the previous frame is not using this image
    if (context.images_in_flight[context.image_index] != VK_NULL_HANDLE) {
        VkResult result = vkWaitForFences(
            context.device.logical_device, 1,
            &context.in_flight_fences[context.current_frame], true, UINT64_MAX);
        if (!vulkan_result_is_success(result)) {
            KFATAL("In flight fence wait failure! error: '%s'.",
                   vulkan_result_string(result, true));
            return false;
        }
    }

    // Mark the image fence as in-use by this frame
    context.images_in_flight[context.image_index] =
        &context.in_flight_fences[context.current_frame];

    // Reset the fence for use on the next frame
    VK_CHECK(vkResetFences(context.device.logical_device, 1,
                           &context.in_flight_fences[context.current_frame]));

    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

    // Command buffer(s) to be executed
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer->handle;

    // The semaphore(s) to be signaled when the queue is complete
    // After you have acquired: u32 imageIndex = ...
    VkSemaphore signal_semaphores[] = {
        context.swapchain.render_finished_per_image[context.image_index]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    // Wait semaphore ensures that the operation cannot begin until the image is
    // available.
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores =
        &context.image_available_semaphores[context.current_frame];

    // Each semaphore waits on the corresponding pipeline stage to complete. 1:1
    // ratio. VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT precents subsequent
    // colour attachment writes from executing until the semaphore signals (one
    // frame is presented at a time).
    VkPipelineStageFlags flags[1] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.pWaitDstStageMask = flags;

    VkResult result =
        vkQueueSubmit(context.device.graphics_queue, 1, &submit_info,
                      context.in_flight_fences[context.current_frame]);

    if (result != VK_SUCCESS) {
        KERROR("vkQueueSubmit failed with result: '%s'",
               vulkan_result_string(result, true));
        return false;
    }

    vulkan_command_buffer_update_submitted(command_buffer);
    // End queue submission

    // Give the image back to the swapchain
    vulkan_swapchain_present(
        &context, &context.swapchain, context.device.graphics_queue,
        context.device.present_queue,
        context.queue_complete_semaphores[context.current_frame],
        context.image_index);

    return true;
}

b8 vulkan_renderer_begin_renderpass(renderer_backend *backend,
                                    u8 renderpass_id) {
    vulkan_renderpass *renderpass = 0;
    VkFramebuffer framebuffer = 0;
    vulkan_command_buffer *command_buffer =
        &context.graphics_command_buffers[context.image_index];

    // TODO: make custom renderpasses
    switch (renderpass_id) {
    case BUILTIN_RENDERPASS_WORLD:
        renderpass = &context.main_renderpass;
        framebuffer = context.world_framebuffers[context.image_index];
        break;
    case BUILTIN_RENDERPASS_UI:
        renderpass = &context.ui_renderpass;
        framebuffer = context.swapchain.framebuffers[context.image_index];
        break;
    default:
        KERROR("vulkan_renderer_begin_renderpass - unrecognised renderpass is "
               "'%#02x'.",
               renderpass_id);
        return false;
    }

    // Begin the renderpass
    vulkan_renderpass_begin(command_buffer, renderpass, framebuffer);

    return true;
}

b8 vulkan_renderer_end_renderpass(renderer_backend *backend, u8 renderpass_id) {
    vulkan_renderpass *renderpass = 0;
    vulkan_command_buffer *command_buffer =
        &context.graphics_command_buffers[context.image_index];

    switch (renderpass_id) {
    case BUILTIN_RENDERPASS_WORLD:
        renderpass = &context.main_renderpass;
        break;
    case BUILTIN_RENDERPASS_UI:
        renderpass = &context.ui_renderpass;
        break;
    default:
        KERROR("vulkan_renderer_end_renderpass - unrecognised renderpass is "
               "'%#02x'.",
               renderpass_id);
        return false;
    }

    vulkan_renderpass_end(command_buffer, renderpass);
    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                  VkDebugUtilsMessageTypeFlagsEXT message_types,
                  const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                  void *user_data) {
    switch (message_severity) {
    default:
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        KERROR(callback_data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        KWARN(callback_data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        KINFO(callback_data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        KTRACE(callback_data->pMessage);
        break;
    }
    return VK_FALSE;
}

i32 find_memory_index(u32 type_filter, u32 property_flags) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(context.device.physical_device,
                                        &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; i++) {
        // Check each memory type
        if (type_filter & (1 << i) &&
            (memory_properties.memoryTypes[i].propertyFlags & property_flags) ==
                property_flags) {
            return i;
        }
    }

    KWARN("Unable to find suitable memory type!");
    return -1;
}

void create_command_buffers(renderer_backend *backend) {
    if (!context.graphics_command_buffers) {
        context.graphics_command_buffers = darray_reserve(
            vulkan_command_buffer, context.swapchain.image_count);
        for (u32 i = 0; i < context.swapchain.image_count; i++) {
            kzero_memory(&context.graphics_command_buffers[i],
                         sizeof(vulkan_command_buffer));
        }
    }

    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        if (context.graphics_command_buffers[i].handle) {
            vulkan_command_buffer_free(&context,
                                       context.device.graphics_command_pool,
                                       &context.graphics_command_buffers[i]);
        }
        kzero_memory(&context.graphics_command_buffers[i],
                     sizeof(vulkan_command_buffer));
        vulkan_command_buffer_allocate(
            &context, context.device.graphics_command_pool, true,
            &context.graphics_command_buffers[i]);
    }
    KINFO("Vulkan command buffers created.");
}

void regenerate_framebuffers() {
    u32 image_count = context.swapchain.image_count;
    for (u32 i = 0; i < image_count; i++) {
        VkImageView world_attachments[2] = {
            context.swapchain.views[i],
            context.swapchain.depth_attachment.view};
        VkFramebufferCreateInfo framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebuffer_create_info.renderPass = context.main_renderpass.handle;
        framebuffer_create_info.attachmentCount = 2;
        framebuffer_create_info.pAttachments = world_attachments;
        framebuffer_create_info.width = context.framebuffer_width;
        framebuffer_create_info.height = context.framebuffer_height;
        framebuffer_create_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(
            context.device.logical_device, &framebuffer_create_info,
            context.allocator, &context.world_framebuffers[i]));

        VkImageView ui_attachments[1] = {context.swapchain.views[i]};
        VkFramebufferCreateInfo sc_framebuffer_create_info = {
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        sc_framebuffer_create_info.renderPass = context.ui_renderpass.handle;
        sc_framebuffer_create_info.attachmentCount = 1;
        sc_framebuffer_create_info.pAttachments = ui_attachments;
        sc_framebuffer_create_info.width = context.framebuffer_width;
        sc_framebuffer_create_info.height = context.framebuffer_height;
        sc_framebuffer_create_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(
            context.device.logical_device, &sc_framebuffer_create_info,
            context.allocator, &context.swapchain.framebuffers[i]));
    }
}

b8 recreate_swapchain(renderer_backend *backend) {
    // If already being recreated, do not try again
    if (context.recreating_swapchain) {
        KDEBUG(
            "recreate_swapchain called when already being recreated. Booting");
        return false;
    }

    // Detect if window is too small to be drawn to
    if (context.framebuffer_width == 0 || context.framebuffer_height == 0) {
        KDEBUG("recreate_swapchain called when window is < 1 in a dimension. "
               "Booting.");
        return false;
    }

    // Mark as recreating swapchain
    context.recreating_swapchain = true;

    // Wait for any operations to complete
    vkDeviceWaitIdle(context.device.logical_device);

    // Clear out just in case
    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        context.images_in_flight[i] = 0;
    }

    // Requery support
    vulkan_device_query_swapchain_support(context.device.physical_device,
                                          context.surface,
                                          &context.device.swapchain_support);
    vulkan_device_detect_depth_format(&context.device);

    vulkan_swapchain_recreate(&context, cached_framebuffer_width,
                              cached_framebuffer_height, &context.swapchain);

    // Sync the framebuffer size with the cached sizes
    context.framebuffer_width = cached_framebuffer_width;
    context.framebuffer_height = cached_framebuffer_height;

    context.main_renderpass.render_area.z = context.framebuffer_width;
    context.main_renderpass.render_area.w = context.framebuffer_height;

    cached_framebuffer_width = 0;
    cached_framebuffer_height = 0;

    // Update framebuffer size generation
    context.framebuffer_size_last_generation =
        context.framebuffer_size_generation;

    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        vulkan_command_buffer_free(&context,
                                   context.device.graphics_command_pool,
                                   &context.graphics_command_buffers[i]);
    }

    for (u32 i = 0; i < context.swapchain.image_count; i++) {
        vkDestroyFramebuffer(context.device.logical_device,
                             context.world_framebuffers[i], context.allocator);
        vkDestroyFramebuffer(context.device.logical_device,
                             context.swapchain.framebuffers[i],
                             context.allocator);
    }

    context.main_renderpass.render_area.x = 0;
    context.main_renderpass.render_area.y = 0;
    context.main_renderpass.render_area.z = context.framebuffer_width;
    context.main_renderpass.render_area.w = context.framebuffer_height;

    context.ui_renderpass.render_area.x = 0;
    context.ui_renderpass.render_area.y = 0;
    context.ui_renderpass.render_area.z = context.framebuffer_width;
    context.ui_renderpass.render_area.w = context.framebuffer_height;

    // Regenerate swapchain and world frame buffers
    regenerate_framebuffers();

    create_command_buffers(backend);

    // Clear the recreating flag
    context.recreating_swapchain = false;

    return true;
}

b8 create_buffers(vulkan_context *context) {
    VkMemoryPropertyFlagBits memory_property_flag =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Geometry vertex buffer
    const u64 vertex_buffer_size = sizeof(vertex_3d) * 1024 * 1024;
    if (!vulkan_buffer_create(context, vertex_buffer_size,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              memory_property_flag, true,
                              &context->object_vertex_buffer)) {
        KERROR("Error creating vertex buffer.");
        return false;
    }

    const u64 index_buffer_size = sizeof(u32) * 1024 * 1024;
    if (!vulkan_buffer_create(context, index_buffer_size,
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              memory_property_flag, true,
                              &context->object_index_buffer)) {
        KERROR("Error creating index buffer.");
        return false;
    }

    return true;
}

void vulkan_renderer_create_texture(const u8 *pixels, struct texture *texture) {
    // Internal data creation
    // TODO: use an allocator for this
    texture->internal_data = (vulkan_texture_data *)kallocate(
        sizeof(vulkan_texture_data), MEMORY_TAG_TEXTURE);
    vulkan_texture_data *data = (vulkan_texture_data *)texture->internal_data;
    VkDeviceSize image_size =
        texture->width * texture->height * texture->channel_count;

    // NOTE: Assuming channel count
    VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;

    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags memory_prop_flags =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    vulkan_buffer staging;
    vulkan_buffer_create(&context, image_size, usage, memory_prop_flags, true,
                         &staging);
    vulkan_buffer_load_data(&context, &staging, 0, image_size, 0, pixels);

    // NOTE: loads of assumptions
    vulkan_image_create(
        &context, VK_IMAGE_TYPE_2D, texture->width, texture->height,
        image_format, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, VK_IMAGE_ASPECT_COLOR_BIT,
        &data->image);

    vulkan_command_buffer temp_buffer;
    VkCommandPool pool = context.device.graphics_command_pool;
    VkQueue queue = context.device.graphics_queue;
    vulkan_command_buffer_allocate_and_begin_single_use(&context, pool,
                                                        &temp_buffer);

    // Transition the layout
    vulkan_image_transition_layout(&context, &temp_buffer, &data->image,
                                   image_format, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy the data from the buffer
    vulkan_image_copy_from_buffer(&context, &data->image, staging.handle,
                                  &temp_buffer);

    vulkan_image_transition_layout(&context, &temp_buffer, &data->image,
                                   image_format,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vulkan_command_buffer_end_single_use(&context, pool, &temp_buffer, queue);

    vulkan_buffer_destroy(&context, &staging);

    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    // TODO: these should be configurable
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    VkResult result =
        vkCreateSampler(context.device.logical_device, &sampler_info,
                        context.allocator, &data->sampler);
    if (!vulkan_result_is_success(result)) {
        KERROR("Error creating texture sampler: %s",
               vulkan_result_string(result, true));
        return;
    }

    texture->generation++;
}

void vulkan_renderer_destroy_texture(texture *texture) {
    vkDeviceWaitIdle(context.device.logical_device);

    vulkan_texture_data *data = (vulkan_texture_data *)texture->internal_data;

    if (data) {
        vulkan_image_destroy(&context, &data->image);
        kzero_memory(&data->image, sizeof(vulkan_image));
        vkDestroySampler(context.device.logical_device, data->sampler,
                         context.allocator);
        data->sampler = 0;

        kfree(texture->internal_data, sizeof(vulkan_texture_data),
              MEMORY_TAG_TEXTURE);
    }

    kzero_memory(texture, sizeof(struct texture));
}

b8 vulkan_renderer_create_geometry(geometry *geometry, u32 vertex_size,
                                   u32 vertex_count, const void *vertices,
                                   u32 index_size, u32 index_count,
                                   const void *indices) {
    if (!vertex_count || !vertices) {
        KERROR("vulkan_renderer_create_geometry - requires proper vertex data "
               "which hasn't been supplied. vertex_count='%d', vertices='%p'.",
               vertex_count, vertices);
        return false;
    }

    // Check if is a reupload
    b8 is_reupload = geometry->internal_id != INVALID_ID;
    vulkan_geometry_data old_range;

    vulkan_geometry_data *internal_data = 0;
    if (is_reupload) {
        internal_data = &context.geometries[geometry->internal_id];

        // Take a copy of the old range
        old_range.index_buffer_offset = internal_data->index_buffer_offset;
        old_range.index_count = internal_data->index_count;
        old_range.index_element_size = internal_data->index_element_size;
        old_range.vertex_buffer_offset = internal_data->vertex_buffer_offset;
        old_range.vertex_buffer_offset = internal_data->vertex_buffer_offset;
        old_range.vertex_buffer_offset = internal_data->vertex_buffer_offset;
    } else {
        for (u32 i = 0; i < VULKAN_MAX_GEOMETRY_COUNT; i++) {
            if (context.geometries[i].id == INVALID_ID) {
                geometry->internal_id = i;
                context.geometries[i].id = i;
                internal_data = &context.geometries[i];
                break;
            }
        }
    }

    if (!internal_data) {
        KFATAL("vulkan_renderer_create_geometry - could not find a free index. "
               "Adjust config to allow more.");
        return false;
    }

    VkCommandPool pool = context.device.graphics_command_pool;
    VkQueue queue = context.device.graphics_queue;

    // Vertex data
    internal_data->vertex_count = vertex_count;
    internal_data->vertex_element_size = sizeof(vertex_3d);
    u32 vertex_total_size = vertex_count * vertex_size;
    if (!upload_data_range(&context, pool, 0, queue,
                           &context.object_vertex_buffer,
                           &internal_data->vertex_buffer_offset,
                           vertex_total_size, vertices)) {
        KERROR("vulkan_renderer_create_geometry - failed to upload to the "
               "vertex buffer.");
        return false;
    }

    // Index data, if applicable
    if (index_count && indices) {
        internal_data->index_count = index_count;
        internal_data->index_element_size = sizeof(u32);
        u32 index_total_size = index_count * index_size;
        KTRACE("vulkan_renderer_create_geometry - Upload data range 2");
        if (!upload_data_range(&context, pool, 0, queue,
                               &context.object_index_buffer,
                               &internal_data->index_buffer_offset,
                               index_total_size, indices)) {
            KERROR("vulkan_renderer_create_geometry - failed to upload to the "
                   "index buffer.");
            return false;
        }
    }

    if (internal_data->generation == INVALID_ID) {
        internal_data->generation = 0;
    } else {
        internal_data->generation++;
    }

    if (is_reupload) {
        u32 old_range_total_vertex_size =
            old_range.vertex_element_size * old_range.vertex_count;
        free_data_range(&context.object_vertex_buffer,
                        old_range.vertex_buffer_offset,
                        old_range_total_vertex_size);

        // Free index data if applicable
        if (old_range.index_element_size > 0) {
            u32 old_range_total_index_size =
                old_range.index_element_size * old_range.index_count;
            free_data_range(&context.object_index_buffer,
                            old_range.index_buffer_offset,
                            old_range_total_index_size);
        }
    }

    return true;
}

void vulkan_renderer_destroy_geometry(geometry *geometry) {
    if (!geometry) {
        return;
    }

    if (geometry->internal_id == INVALID_ID) {
        return;
    }

    vkDeviceWaitIdle(context.device.logical_device);
    vulkan_geometry_data *internal_data =
        &context.geometries[geometry->internal_id];

    // Free vertex data
    u32 total_vertex_size =
        internal_data->vertex_element_size * internal_data->vertex_count;
    free_data_range(&context.object_vertex_buffer,
                    internal_data->vertex_buffer_offset, total_vertex_size);

    // Free index data, if applicable
    if (internal_data->index_element_size > 0) {
        u32 total_index_size =
            internal_data->index_element_size * internal_data->index_count;
        free_data_range(&context.object_vertex_buffer,
                        internal_data->vertex_buffer_offset, total_index_size);
    }

    kzero_memory(internal_data, sizeof(vulkan_geometry_data));
    internal_data->id = INVALID_ID;
    internal_data->generation = INVALID_ID;
}

void vulkan_renderer_draw_geometry(renderer_backend *backend,
                                   geometry_render_data data) {
    // ignore non-uploaded geometries
    if (data.geometry && data.geometry->internal_id == INVALID_ID) {
        return;
    }

    vulkan_geometry_data *buffer_data =
        &context.geometries[data.geometry->internal_id];
    vulkan_command_buffer *command_buffer =
        &context.graphics_command_buffers[context.image_index];

    VkDeviceSize offsets[1] = {buffer_data->vertex_buffer_offset};
    vkCmdBindVertexBuffers(command_buffer->handle, 0, 1,
                           &context.object_vertex_buffer.handle,
                           (VkDeviceSize *)offsets);

    if (buffer_data->index_count > 0) {
        // Bind index buffer at offset
        vkCmdBindIndexBuffer(
            command_buffer->handle, context.object_index_buffer.handle,
            buffer_data->index_buffer_offset, VK_INDEX_TYPE_UINT32);
        // Issue the draw
        vkCmdDrawIndexed(command_buffer->handle, buffer_data->index_count, 1, 0,
                         0, 0);
    } else {
        vkCmdDraw(command_buffer->handle, buffer_data->vertex_count, 1, 0, 0);
    }
}

// The index of the global descriptor set
const u32 DESC_SET_INDEX_GLOBAL = 0;
// The index of the instance descriptor set
const u32 DESC_SET_INDEX_INSTANCE = 1;
// The index of the UBO binding
const u32 BINDING_INDEX_UBO = 0;
// The index of the image sampler binding
const u32 BINDING_INDEX_SAMPLER = 1;

b8 vulkan_renderer_shader_create(struct shader *shader, u8 renderpass_id,
                                 u8 stage_count, const char **stage_filenames,
                                 shader_stage *stages) {
    shader->internal_data =
        kallocate(sizeof(vulkan_shader), MEMORY_TAG_RENDERER);

    // TODO: dynamic renderpass
    vulkan_renderpass *renderpass =
        renderpass_id == 1 ? &context.main_renderpass : &context.ui_renderpass;

    // Translate stages
    VkShaderStageFlags vk_stages[VULKAN_SHADER_MAX_STAGES];
    for (u8 i = 0; i < stage_count; i++) {
        switch (stages[i]) {
        case SHADER_STAGE_FRAGMENT:
            vk_stages[i] = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case SHADER_STAGE_VERTEX:
            vk_stages[i] = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case SHADER_STAGE_GEOMETRY:
            KWARN("vulkan_renderer_shader_create - Geometry flag set but not "
                  "supported yet.");
            vk_stages[i] = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case SHADER_STAGE_COMPUTE:
            KWARN("vulkan_renderer_shader_create - Compute flag set but not "
                  "supported yet.");
            vk_stages[i] = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
        default:
            KERROR("vulkan_renderer_shader_create - Unsupported stage type %d.",
                   stages[i]);
            break;
        }
    }

    // TODO: make configurable
    u32 max_descriptor_allocate_count = 1024;

    vulkan_shader *out_shader = (vulkan_shader *)shader->internal_data;
    out_shader->renderpass = renderpass;
    out_shader->config.max_descriptor_set_count = max_descriptor_allocate_count;

    kzero_memory(out_shader->config.stages,
                 sizeof(vulkan_shader_stage_config) * VULKAN_SHADER_MAX_STAGES);
    out_shader->config.stage_count = 0;
    for (u8 i = 0; i < stage_count; i++) {
        if (out_shader->config.stage_count + 1 > VULKAN_SHADER_MAX_STAGES) {
            KERROR("vulkan_renderer_shader_create - shader surpass maximum of "
                   "'%d'.",
                   VULKAN_SHADER_MAX_STAGES);
            return false;
        }

        VkShaderStageFlagBits stage_flag;
        switch (stages[i]) {
        case SHADER_STAGE_FRAGMENT:
            stage_flag = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case SHADER_STAGE_VERTEX:
            stage_flag = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        default:
            KERROR("vulkan_renderer_shader_create - Unsupported stage flag %d. "
                   "Stage is being ignored.",
                   stages[i]);
            continue;
        }

        out_shader->config.stages[out_shader->config.stage_count].stage =
            stage_flag;
        string_ncopy(out_shader->config.stages[out_shader->config.stage_count++]
                         .file_name,
                     stage_filenames[i], 255);
    }

    kzero_memory(out_shader->config.descriptor_sets,
                 sizeof(vulkan_descriptor_set_config) * 2);
    kzero_memory(out_shader->config.attributes,
                 sizeof(VkVertexInputAttributeDescription) *
                     darray_length(out_shader->config.attributes));

    // For now, shaders will only have two types of descriptor pools
    out_shader->config.pool_sizes[0] =
        (VkDescriptorPoolSize){VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024};
    out_shader->config.pool_sizes[1] =
        (VkDescriptorPoolSize){VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096};

    vulkan_descriptor_set_config global_descriptor_set_config = {};
    global_descriptor_set_config.bindings[BINDING_INDEX_UBO].binding =
        BINDING_INDEX_UBO;
    global_descriptor_set_config.bindings[BINDING_INDEX_UBO].descriptorCount =
        1;
    global_descriptor_set_config.bindings[BINDING_INDEX_UBO].descriptorType =
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    global_descriptor_set_config.bindings[BINDING_INDEX_UBO].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    global_descriptor_set_config.binding_count++;

    out_shader->config.descriptor_sets[DESC_SET_INDEX_GLOBAL] =
        global_descriptor_set_config;
    out_shader->config.descriptor_set_count++;

    if (shader->use_instances) {
        vulkan_descriptor_set_config instance_descriptor_set_config = {};
        instance_descriptor_set_config.bindings[BINDING_INDEX_UBO].binding =
            BINDING_INDEX_UBO;
        instance_descriptor_set_config.bindings[BINDING_INDEX_UBO]
            .descriptorCount = 1;
        instance_descriptor_set_config.bindings[BINDING_INDEX_UBO]
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        instance_descriptor_set_config.bindings[BINDING_INDEX_UBO].stageFlags =
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_VERTEX_BIT;
        instance_descriptor_set_config.binding_count++;

        out_shader->config.descriptor_sets[DESC_SET_INDEX_INSTANCE] =
            instance_descriptor_set_config;
        out_shader->config.descriptor_set_count++;
    }

    // Invalidate instance states
    // TODO: make dynamic
    for (u32 i = 0; i < 1024; i++) {
        out_shader->instance_states[i].id = INVALID_ID;
    }

    return true;
}

void vulkan_renderer_shader_destroy(struct shader *s) {
    if (!s || !s->internal_data) {
        return;
    }

    vulkan_shader *shader = s->internal_data;
    if (!shader) {
        KERROR("vulkan_renderer_shader_destroy - requires a valid pointer to a "
               "shader.");
    }

    VkDevice logical_device = context.device.logical_device;
    VkAllocationCallbacks *vk_allocator = context.allocator;

    // Descriptor set layouts
    for (u32 i = 0; i < shader->config.max_descriptor_set_count; i++) {
        if (shader->descriptor_set_layouts[i]) {
            vkDestroyDescriptorSetLayout(logical_device,
                                         shader->descriptor_set_layouts[i],
                                         vk_allocator);
            shader->descriptor_set_layouts[i] = 0;
        }
    }

    // Descriptor pool
    if (shader->descriptor_pool) {
        vkDestroyDescriptorPool(logical_device, shader->descriptor_pool,
                                vk_allocator);
    }

    // Uniform buffer
    vulkan_buffer_unlock_memory(&context, &shader->uniform_buffer);
    shader->mapped_uniform_buffer_block = 0;
    vulkan_buffer_destroy(&context, &shader->uniform_buffer);

    // Pipeline
    vulkan_pipeline_destroy(&context, &shader->pipeline);

    // Shader modules
    for (u32 i = 0; i < shader->config.stage_count; i++) {
        vkDestroyShaderModule(logical_device, shader->stages[i].handle,
                              vk_allocator);
    }

    // Destroy the config
    kzero_memory(&shader->config, sizeof(shader_config));

    kfree(s->internal_data, sizeof(vulkan_shader), MEMORY_TAG_RENDERER);
    s->internal_data = 0;
}

b8 vulkan_renderer_shader_initialize(struct shader *shader) {
    VkDevice logical_device = context.device.logical_device;
    VkAllocationCallbacks *vk_allocator = context.allocator;
    vulkan_shader *s = (vulkan_shader *)shader->internal_data;

    // Create a module for each stage
    kzero_memory(s->stages,
                 sizeof(vulkan_shader_stage) * VULKAN_SHADER_MAX_STAGES);
    for (u32 i = 0; i < s->config.stage_count; i++) {
        if (!create_module(s, s->config.stages[i], &s->stages[i])) {
            KERROR("Unable to create %s shader module for shader '%s'. Shader "
                   "will be destroyed.",
                   s->config.stages[i].file_name, shader->name);
            return false;
        }
    }

    // Static lookup for types->Vulkan
    // TODO: put this somewhere else, it's ugly here
    static VkFormat *types = 0;
    static VkFormat t[29];
    if (!types) {
        t[SHADER_ATTRIB_TYPE_FLOAT32] = VK_FORMAT_R32_SFLOAT;
        t[SHADER_ATTRIB_TYPE_FLOAT32_2] = VK_FORMAT_R32G32_SFLOAT;
        t[SHADER_ATTRIB_TYPE_FLOAT32_3] = VK_FORMAT_R32G32B32_SFLOAT;
        t[SHADER_ATTRIB_TYPE_FLOAT32_4] = VK_FORMAT_R32G32B32A32_SFLOAT;

        // NOTE: A mat4 requires 4 attribute slots of Vec4
        t[SHADER_ATTRIB_TYPE_MATRIX4] = VK_FORMAT_R32G32B32A32_SFLOAT;

        // 8-bit Integers
        t[SHADER_ATTRIB_TYPE_INT8] = VK_FORMAT_R8_SINT;
        t[SHADER_ATTRIB_TYPE_INT8_2] = VK_FORMAT_R8G8_SINT;
        t[SHADER_ATTRIB_TYPE_INT8_3] = VK_FORMAT_R8G8B8_SINT;
        t[SHADER_ATTRIB_TYPE_INT8_4] = VK_FORMAT_R8G8B8A8_SINT;

        t[SHADER_ATTRIB_TYPE_UINT8] = VK_FORMAT_R8_UINT;
        t[SHADER_ATTRIB_TYPE_UINT8_2] = VK_FORMAT_R8G8_UINT;
        t[SHADER_ATTRIB_TYPE_UINT8_3] = VK_FORMAT_R8G8B8_UINT;
        t[SHADER_ATTRIB_TYPE_UINT8_4] = VK_FORMAT_R8G8B8A8_UINT;

        // 16-bit Integers
        t[SHADER_ATTRIB_TYPE_INT16] = VK_FORMAT_R16_SINT;
        t[SHADER_ATTRIB_TYPE_INT16_2] = VK_FORMAT_R16G16_SINT;
        t[SHADER_ATTRIB_TYPE_INT16_3] = VK_FORMAT_R16G16B16_SINT;
        t[SHADER_ATTRIB_TYPE_INT16_4] = VK_FORMAT_R16G16B16A16_SINT;

        t[SHADER_ATTRIB_TYPE_UINT16] = VK_FORMAT_R16_UINT;
        t[SHADER_ATTRIB_TYPE_UINT16_2] = VK_FORMAT_R16G16_UINT;
        t[SHADER_ATTRIB_TYPE_UINT16_3] = VK_FORMAT_R16G16B16_UINT;
        t[SHADER_ATTRIB_TYPE_UINT16_4] = VK_FORMAT_R16G16B16A16_UINT;

        // 32-bit Integers
        t[SHADER_ATTRIB_TYPE_INT32] = VK_FORMAT_R32_SINT;
        t[SHADER_ATTRIB_TYPE_INT32_2] = VK_FORMAT_R32G32_SINT;
        t[SHADER_ATTRIB_TYPE_INT32_3] = VK_FORMAT_R32G32B32_SINT;
        t[SHADER_ATTRIB_TYPE_INT32_4] = VK_FORMAT_R32G32B32A32_SINT;

        t[SHADER_ATTRIB_TYPE_UINT32] = VK_FORMAT_R32_UINT;
        t[SHADER_ATTRIB_TYPE_UINT32_2] = VK_FORMAT_R32G32_UINT;
        t[SHADER_ATTRIB_TYPE_UINT32_3] = VK_FORMAT_R32G32B32_UINT;
        t[SHADER_ATTRIB_TYPE_UINT32_4] = VK_FORMAT_R32G32B32A32_UINT;
        types = t;
    }

    // Process attributes
    u32 attribute_count = darray_length(shader->attributes);
    u32 offset = 0;
    u32 location = 0;
    for (u32 i = 0; i < attribute_count; i++) {
        shader_attribute_type type = shader->attributes[i].type;

        if (type != SHADER_ATTRIB_TYPE_MATRIX4) {
            // Setup new attribute
            VkVertexInputAttributeDescription attribute;
            attribute.location = location;
            attribute.binding = 0;
            attribute.offset = offset;
            attribute.format = types[type];

            // Push into the config's attribute collection and add to the stride
            darray_push(s->config.attributes, attribute);

            offset += shader->attributes[i].size;
            location++;
            continue;
        }

        // special case for matrix
        for (u32 j = 0; j < 4; j++) {
            VkVertexInputAttributeDescription attribute;
            attribute.location = location;
            attribute.binding = 0;
            attribute.offset = offset;
            attribute.format = types[type];

            darray_push(s->config.attributes, attribute);

            offset += shader->attributes[i].size;
            location++;
        }
    }

    // Process uniforms
    u32 uniform_count = darray_length(shader->uniforms);
    for (u32 i = 0; i < uniform_count; i++) {
        // For samplers, the descriptor bindings need to be updated.
        if (shader->uniforms[i].type == SHADER_UNIFORM_TYPE_SAMPLER) {
            const u32 set_index =
                (shader->uniforms[i].scope == SHADER_SCOPE_GLOBAL
                     ? DESC_SET_INDEX_GLOBAL
                     : DESC_SET_INDEX_INSTANCE);
            vulkan_descriptor_set_config *set_config =
                &s->config.descriptor_sets[set_index];
            if (set_config->binding_count < 2) {
                set_config->bindings[BINDING_INDEX_SAMPLER].binding =
                    BINDING_INDEX_SAMPLER;
                set_config->bindings[BINDING_INDEX_SAMPLER].descriptorCount = 1;
                set_config->bindings[BINDING_INDEX_SAMPLER].descriptorType =
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                set_config->bindings[BINDING_INDEX_SAMPLER].stageFlags =
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                set_config->binding_count++;
            } else {
                set_config->bindings[BINDING_INDEX_SAMPLER].descriptorCount++;
            }
        }
    }

    // Descriptor pool
    VkDescriptorPoolCreateInfo pool_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = s->config.pool_sizes;
    pool_info.maxSets = s->config.max_descriptor_set_count;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    // Create descriptor pool
    VkResult result = vkCreateDescriptorPool(logical_device, &pool_info,
                                             vk_allocator, &s->descriptor_pool);
    if (!vulkan_result_is_success(result)) {
        KERROR("vulkan_renderer_shader_initialize - failed creating descriptor "
               "pool: '%s'.",
               vulkan_result_string(result, true));
        return false;
    }

    // Create descriptor set layouts
    kzero_memory(s->descriptor_set_layouts, s->config.descriptor_set_count);
    for (u32 i = 0; i < s->config.descriptor_set_count; i++) {
        VkDescriptorSetLayoutCreateInfo layout_info = {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout_info.bindingCount = s->config.descriptor_sets[i].binding_count;
        layout_info.pBindings = s->config.descriptor_sets[i].bindings;
        result = vkCreateDescriptorSetLayout(logical_device, &layout_info,
                                             vk_allocator,
                                             &s->descriptor_set_layouts[i]);
        if (!vulkan_result_is_success(result)) {
            KERROR("vulkan_renderer_shader_initialize - failed creating "
                   "descriptor set layouts: '%s'.",
                   vulkan_result_string(result, true));
            return false;
        }
    }

    // TODO: this shouldn't be here
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (f32)context.framebuffer_height;
    viewport.width = (f32)context.framebuffer_width;
    viewport.height = -(f32)context.framebuffer_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor;
    scissor.offset.x = scissor.offset.x = 0;
    scissor.extent.width = context.framebuffer_width;
    scissor.extent.height = context.framebuffer_height;

    VkPipelineShaderStageCreateInfo
        stage_create_infos[VULKAN_SHADER_MAX_STAGES];
    kzero_memory(stage_create_infos, sizeof(VkPipelineShaderStageCreateInfo) *
                                         VULKAN_SHADER_MAX_STAGES);
    for (u32 i = 0; i < s->config.stage_count; i++) {
        stage_create_infos[i] = s->stages[i].shader_stage_create_info;
    }

    b8 pipeline_result = vulkan_graphics_pipeline_create(
        &context, s->renderpass, shader->attribute_stride,
        darray_length(s->config.attributes), s->config.attributes,
        s->config.descriptor_set_count, s->descriptor_set_layouts,
        s->config.stage_count, stage_create_infos, viewport, scissor, false,
        true, &s->pipeline);

    if (!pipeline_result) {
        KERROR("vulkan_renderer_shader_initialize - Failed to load graphics "
               "pipeline for shader object.");
        return false;
    }

    // Get the UBO alignment
    shader->required_ubo_alignment =
        context.device.properties.limits.minUniformBufferOffsetAlignment;

    // Make sure the UBO is aligned
    shader->global_ubo_stride =
        get_aligned(shader->global_ubo_size, shader->required_ubo_alignment);
    shader->ubo_stride =
        get_aligned(shader->ubo_size, shader->required_ubo_alignment);

    // Uniform buffer
    u32 device_local_bits = context.device.supports_device_local_host_visible
                                ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                : 0;
    // TODO: max count should be configurable
    u32 total_buffer_size = shader->global_ubo_stride +
                            (shader->ubo_stride * VULKAN_MAX_MATERIAL_COUNT);
    if (!vulkan_buffer_create(&context, total_buffer_size,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                  device_local_bits,
                              true, &s->uniform_buffer)) {
        KERROR("vulkan_renderer_shader_initialize - buffer creation failed for "
               "object shader.");
        return false;
    }

    // Allocate space for the global UBO
    if (!vulkan_buffer_allocate(&s->uniform_buffer, shader->global_ubo_stride,
                                &shader->global_ubo_offset)) {
        KERROR("vulkan_renderer_shader_initialize - Failed to allocate space "
               "to the uniform buffer.");
        return false;
    }

    // Map the entire buffer's memory
    s->mapped_uniform_buffer_block = vulkan_buffer_lock_memory(
        &context, &s->uniform_buffer, 0, VK_WHOLE_SIZE, 0);

    // Allocate global descriptor sets. One per frame
    VkDescriptorSetLayout global_layouts[3] = {
        s->descriptor_set_layouts[DESC_SET_INDEX_GLOBAL],
        s->descriptor_set_layouts[DESC_SET_INDEX_GLOBAL],
        s->descriptor_set_layouts[DESC_SET_INDEX_GLOBAL]};

    VkDescriptorSetAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = s->descriptor_pool;
    alloc_info.descriptorSetCount = 3;
    alloc_info.pSetLayouts = global_layouts;
    VK_CHECK(vkAllocateDescriptorSets(logical_device, &alloc_info,
                                      s->global_descriptor_sets));

    return true;
}

#if defined(_DEBUG)
#define SHADER_VERIFY_SHADER_ID(shader_id)                                     \
    if (shader_id == INVALID_ID ||                                             \
        context.shaders[shader_id].id == INVALID_ID) {                         \
        return false                                                           \
    }
#else
#define SHADER_VERIFY_SHADER_ID(shader_id)
#endif

b8 vulkan_renderer_shader_use(struct shader *shader) {
    vulkan_shader *s = shader->internal_data;
    vulkan_pipeline_bind(&context.graphics_command_buffers[context.image_index],
                         VK_PIPELINE_BIND_POINT_GRAPHICS, &s->pipeline);
    return true;
}

b8 vulkan_renderer_shader_bind_globals(struct shader *s) {
    if (!s) {
        return false;
    }

    s->bound_ubo_offset = s->global_ubo_offset;
    return true;
}

b8 vulkan_renderer_shader_bind_instance(struct shader *s, u32 instance_id) {
    if (!s) {
        KERROR("vulkan_renderer_shader_bind_instance - requires valid shader "
               "pointer.");
        return false;
    }

    vulkan_shader *internal = s->internal_data;

    s->bound_instance_id = instance_id;
    vulkan_shader_instance_state *object_state =
        &internal->instance_states[instance_id];
    s->bound_ubo_offset = object_state->offset;
    return true;
}

b8 vulkan_renderer_shader_apply_globals(struct shader *s) {
    u32 image_index = context.image_index;
    vulkan_shader *internal = s->internal_data;
    VkCommandBuffer command_buffer =
        context.graphics_command_buffers[image_index].handle;
    VkDescriptorSet global_descriptor =
        internal->global_descriptor_sets[image_index];

    // Apply first UBO
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = internal->uniform_buffer.handle;
    bufferInfo.offset = s->global_ubo_offset;
    bufferInfo.range = s->global_ubo_stride;

    // Update descriptor set
    VkWriteDescriptorSet ubo_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    ubo_write.dstSet = internal->global_descriptor_sets[image_index];
    ubo_write.dstBinding = 0;
    ubo_write.dstArrayElement = 0;
    ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_write.descriptorCount = 1;
    ubo_write.pBufferInfo = &bufferInfo;

    VkWriteDescriptorSet descriptor_writes[2];
    descriptor_writes[0] = ubo_write;

    u32 global_set_binding_count =
        internal->config.descriptor_sets[DESC_SET_INDEX_GLOBAL].binding_count;
    if (global_set_binding_count > 1) {
        // TODO: There are samplers to be written.
        global_set_binding_count = 1;
        KERROR("Global image samplers are not supported yet.");
    }

    vkUpdateDescriptorSets(context.device.logical_device,
                           global_set_binding_count, descriptor_writes, 0, 0);

    // Bind the global descriptor set to be updated
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            internal->pipeline.pipeline_layout, 0, 1,
                            &global_descriptor, 0, 0);
    return true;
}

b8 vulkan_renderer_shader_apply_instance(struct shader *s) {
    if (!s->use_instances) {
        KERROR("vulkan_renderer_shader_apply_instance - shader does not use "
               "instances.");
        return false;
    }

    u32 image_index = context.image_index;
    vulkan_shader *internal = s->internal_data;
    VkCommandBuffer command_buffer =
        context.graphics_command_buffers[image_index].handle;

    // Obtains instance data
    vulkan_shader_instance_state *object_state =
        &internal->instance_states[s->bound_instance_id];
    VkDescriptorSet object_descriptor_set =
        object_state->descriptor_set_state.descriptor_sets[image_index];

    // TODO: if needs updating
    VkWriteDescriptorSet descriptor_writes[2];
    kzero_memory(descriptor_writes, sizeof(VkWriteDescriptorSet) * 2);
    u32 descriptor_count = 0;
    u32 descriptor_index = 0;

    // Descriptor 0 - Uniform buffer
    // Only do this if the descriptor has not been updated
    u8 *instance_ubo_generation =
        &(object_state->descriptor_set_state.descriptor_states[descriptor_index]
              .generations[image_index]);
    // TODO: determine if update is necessary
    if (*instance_ubo_generation == INVALID_ID_U8) {
        VkDescriptorBufferInfo buffer_info;
        buffer_info.buffer = internal->uniform_buffer.handle;
        buffer_info.offset = object_state->offset;
        buffer_info.range = s->ubo_stride;

        VkWriteDescriptorSet ubo_descriptor = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        ubo_descriptor.dstSet = object_descriptor_set;
        ubo_descriptor.dstBinding = descriptor_index;
        ubo_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_descriptor.descriptorCount = 1;
        ubo_descriptor.pBufferInfo = &buffer_info;

        descriptor_writes[descriptor_count] = ubo_descriptor;
        descriptor_count++;

        *instance_ubo_generation = 1;
    }
    descriptor_index++;

    // Samplers will always be in the binding. If the binding count is less than
    // two, then there are no samplers.
    if (internal->config.descriptor_sets[DESC_SET_INDEX_INSTANCE]
            .binding_count > 1) {
        // Iterate samplers
        u32 total_sampler_count =
            internal->config.descriptor_sets[DESC_SET_INDEX_INSTANCE]
                .bindings[BINDING_INDEX_SAMPLER]
                .descriptorCount;
        u32 update_sampler_count = 0;
        VkDescriptorImageInfo image_infos[VULKAN_SHADER_MAX_INSTANCE_TEXTURES];
        for (u32 i = 0; i < total_sampler_count; i++) {
            // TODO: only update if it needs to be updated
            texture *t = internal->instance_states[s->bound_instance_id]
                             .instance_textures[i];
            vulkan_texture_data *internal_data =
                (vulkan_texture_data *)t->internal_data;
            image_infos[i].imageLayout =
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[i].imageView = internal_data->image.view;
            image_infos[i].sampler = internal_data->sampler;

            // TODO: change up descriptor state to handle this properly.
            // Sync frame generation if not using a default texture.
            // if (t->generation != INVALID_ID) {
            //     *descriptor_generation = t->generation;
            //     *descriptor_id = t->id;
            // }}

            update_sampler_count++;
        }

        VkWriteDescriptorSet sampler_descriptor = {
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        sampler_descriptor.dstSet = object_descriptor_set;
        sampler_descriptor.dstBinding = descriptor_index;
        sampler_descriptor.descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sampler_descriptor.descriptorCount = update_sampler_count;
        sampler_descriptor.pImageInfo = image_infos;

        descriptor_writes[descriptor_count] = sampler_descriptor;
        descriptor_count++;
    }

    if (descriptor_count > 0) {
        vkUpdateDescriptorSets(context.device.logical_device, descriptor_count,
                               descriptor_writes, 0, 0);
    }

    // Bind the descriptor set to be updated
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            internal->pipeline.pipeline_layout, 1, 1,
                            &object_descriptor_set, 0, 0);

    return true;
}

b8 vulkan_renderer_shader_acquire_instance_resources(struct shader *s,
                                                     u32 *out_instance_id) {
    vulkan_shader *internal = s->internal_data;

    // TODO: dynamic
    *out_instance_id = INVALID_ID;
    for (u32 i = 0; i < VULKAN_MAX_MATERIAL_COUNT; i++) {
        if (internal->instance_states[i].id == INVALID_ID) {
            internal->instance_states[i].id = i;
            *out_instance_id = i;
            break;
        }
    }
    if (*out_instance_id == INVALID_ID) {
        KERROR("vulkan_renderer_shader_acquire_instance_resources - failed to "
               "acquire new instance resources.");
        return false;
    }

    vulkan_shader_instance_state *instance_state =
        &internal->instance_states[*out_instance_id];
    u32 instance_texture_count =
        internal->config.descriptor_sets[DESC_SET_INDEX_INSTANCE]
            .bindings[BINDING_INDEX_SAMPLER]
            .descriptorCount;
    // Wipe memory for whole array
    instance_state->instance_textures = kallocate(
        sizeof(texture *) * s->instance_texture_count, MEMORY_TAG_ARRAY);
    texture *default_texture = texture_system_get_default_texture();
    // Set all texture points to default until assigned
    for (u32 i = 0; i < instance_texture_count; i++) {
        instance_state->instance_textures[i] = default_texture;
    }

    // Allocate some space in the UBO
    u64 size = s->ubo_stride;
    if (!vulkan_buffer_allocate(&internal->uniform_buffer, size,
                                &instance_state->offset)) {
        KERROR("vulkan_renderer_shader_acquire_instance_resources - failed to "
               "acquire UBO space.");
        return false;
    }

    vulkan_shader_descriptor_set_state *set_state =
        &instance_state->descriptor_set_state;

    // Each descriptor binding
    u32 binding_count =
        internal->config.descriptor_sets[DESC_SET_INDEX_INSTANCE].binding_count;
    kzero_memory(set_state->descriptor_states,
                 sizeof(vulkan_descriptor_state) * VULKAN_SHADER_MAX_BINDINGS);
    for (u32 i = 0; i < binding_count; i++) {
        // PERF: check if compiler has unrolled in release build
        for (u32 j = 0; j < 3; j++) {
            set_state->descriptor_states[i].generations[j] = INVALID_ID_U8;
            set_state->descriptor_states[i].ids[j] = INVALID_ID;
        }
    }

    // Allocate 3 descriptor sets
    VkDescriptorSetLayout layouts[3] = {
        internal->descriptor_set_layouts[DESC_SET_INDEX_INSTANCE],
        internal->descriptor_set_layouts[DESC_SET_INDEX_INSTANCE],
        internal->descriptor_set_layouts[DESC_SET_INDEX_INSTANCE]};

    VkDescriptorSetAllocateInfo alloc_info = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = internal->descriptor_pool;
    alloc_info.descriptorSetCount = 3;
    alloc_info.pSetLayouts = layouts;

    VkResult result = vkAllocateDescriptorSets(
        context.device.logical_device, &alloc_info,
        instance_state->descriptor_set_state.descriptor_sets);
    if (!vulkan_result_is_success(result)) {
        KERROR("vulkan_renderer_shader_acquire_instance_resources - error "
               "allocating descriptor sets in shader '%s'.",
               vulkan_result_string(result, true));
        return false;
    }

    return true;
}

b8 vulkan_renderer_shader_release_instance_resources(struct shader *s,
                                                     u32 instance_id) {
    vulkan_shader *internal = s->internal_data;
    vulkan_shader_instance_state *instance_state =
        &internal->instance_states[instance_id];

    // Wait for any pending operations
    vkDeviceWaitIdle(context.device.logical_device);

    // Free 3 descriptor sets, one per FRAME
    VkResult result = vkFreeDescriptorSets(
        context.device.logical_device, internal->descriptor_pool, 3,
        instance_state->descriptor_set_state.descriptor_sets);
    if (!vulkan_result_is_success(result)) {
        KERROR("vulkan_renderer_shader_release_instance_resources - Error "
               "freeing object shader descriptor sets.");
    }

    // Destroy the descriptor states
    kzero_memory(instance_state->descriptor_set_state.descriptor_states,
                 sizeof(vulkan_descriptor_state) * VULKAN_SHADER_MAX_BINDINGS);

    if (instance_state->instance_textures) {
        kfree(instance_state->instance_textures,
              sizeof(texture *) * s->instance_texture_count, MEMORY_TAG_ARRAY);
        instance_state->instance_textures = 0;
    }

    vulkan_buffer_free(&internal->uniform_buffer, s->ubo_stride,
                       instance_state->offset);
    instance_state->offset = INVALID_ID;
    instance_state->id = INVALID_ID;

    return true;
}

b8 vulkan_renderer_set_uniform(struct shader *s, struct shader_uniform *uniform,
                               const void *value) {
    vulkan_shader *internal = s->internal_data;
    if (uniform->type == SHADER_UNIFORM_TYPE_SAMPLER) {
        if (uniform->scope == SHADER_SCOPE_GLOBAL) {
            s->global_textures[uniform->location] = (texture *)value;
        } else {
            internal->instance_states[s->bound_instance_id]
                .instance_textures[uniform->location] = (texture *)value;
        }
        return true;
    }

    if (uniform->scope == SHADER_SCOPE_LOCAL) {
        VkCommandBuffer command_buffer =
            context.graphics_command_buffers[context.image_index].handle;
        vkCmdPushConstants(command_buffer, internal->pipeline.pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                           uniform->offset, uniform->size, value);
        return true;
    }

    // Map the appropriate memory location, copy the data over
    u64 addr = (u64)internal->mapped_uniform_buffer_block;
    addr += s->bound_ubo_offset + uniform->offset;
    // TODO: check if this is what was meant by Travis Vroman
    // That's were this shader code is yoinked from
    if (addr) {
        kcopy_memory((void *)addr, value, uniform->size);
    }

    return true;
}

b8 create_module(vulkan_shader *shader, vulkan_shader_stage_config config,
                 vulkan_shader_stage *shader_stage) {
    // Read the resource
    resource binary_resource;
    if (!resource_system_load(config.file_name, RESOURCE_TYPE_BINARY,
                              &binary_resource)) {
        KERROR("Unable to read shader module '%s'.", config.file_name);
        return false;
    }

    kzero_memory(&shader_stage->create_info, sizeof(VkShaderModuleCreateInfo));
    shader_stage->create_info.sType =
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    // Use the resource size and data directly
    shader_stage->create_info.codeSize = binary_resource.data_size;
    shader_stage->create_info.pCode = (u32 *)binary_resource.data;

    VK_CHECK(vkCreateShaderModule(context.device.logical_device,
                                  &shader_stage->create_info, context.allocator,
                                  &shader_stage->handle));

    // Release the resource
    resource_system_unload(&binary_resource);

    // Shader stage info
    kzero_memory(&shader_stage->shader_stage_create_info,
                 sizeof(VkPipelineShaderStageCreateInfo));
    shader_stage->shader_stage_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage->shader_stage_create_info.stage = config.stage;
    shader_stage->shader_stage_create_info.module = shader_stage->handle;
    shader_stage->shader_stage_create_info.pName = "main";

    return true;
}
