/** @file GraphicsDevice.h
*
* @brief Defines class that handles creation and management of Vulkan instance
*   and device
*
* Copyright 2017, Stewart Hall
*
* @author		Stewart Hall (www.stewartghall.com)
* @date			11/12/2017
* @copyright	Copyright 2017, Stewart Hall
*/
#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "PresentationEngine.h"

class GraphicsDevice {
private:
    /**
     * Name of Vulkan layer required for debug
     */
    const char* validation_layer = "VK_LAYER_LUNARG_standard_validation";

    /**
     * List of required Vulkan device extension
     */
    const std::vector<const char*> dev_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    /**
     * Presentation engine used for displaying rendered frames
     */
    PresentationEngine* present = nullptr;

    /**
     * Allocation callbacks to use with Vulkan
     */
    VkAllocationCallbacks* p_allocs = nullptr;

    /**
     * Vulkan instance used for rendering
     */
    VkInstance instance;

    /**
     * Vulkan physical device chosen for rendering
     */
    VkPhysicalDevice physical_device;

    /**
     * Vulkan logical device used for rendering
     */
    VkDevice m_device;

    /**
     * Queue family indexes for graphics and presentation
     */
    int gfx_queue_family = -1;
    int present_queue_family = -1;

    /**
     * Device queue handles for graphics and present
     */
    VkQueue gfx_queue = nullptr;
    VkQueue present_queue = nullptr;

    /**
     * Depth/stencil buffer format
     */
    VkFormat ds_format;

    /**
     * Depth/stencil buffer vulkan handle
     */
    VkImage ds_buffer;

    /**
     * Image view for depth/stencil buffer
     */
    VkImageView ds_buffer_view;

    /**
     * Memory backing for depth stencil buffer
     */
    VkDeviceMemory ds_buffer_mem;

    /**
    * Memory type/heap properties for the chosen device
    */
    VkPhysicalDeviceMemoryProperties mem_props;

    /**
    * Debug callback for validation messages
    */
    VkDebugReportCallbackEXT debug_callback;

    void initVulkan();
    void createInstance();
    void selectDevice();
    void createDeviceAndQueues();
    void createDepthBuffer();

    void enableDebugCallback();
    static VKAPI_ATTR VkBool32 VKAPI_CALL GraphicsDevice::debugCallback(VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code,
        const char* layerPrefix, const char* msg, void* userData);

public:
    /**
     * Initialize vulkan device for graphics using the specified presentation engine
     * @param presentation_engine: the presentation image to use for swapchain creation
     * @param p_allocs: pointer to allocation callbacks to use for vulkan calls
     */
    GraphicsDevice(PresentationEngine* presentation_engine, VkAllocationCallbacks* p_allocs);

    /**
     * Destructor
     */
    ~GraphicsDevice();

    /**
     * Submit a graphics command buffer that renders to the swapchain
     * @param command_buffers array of command buffers to submit with one per swapchain image
     * @param fence array of fences corresponding to each command buffer to wait on before submission
     * @return true if the command buffer was submitted, or false if not
     */
    bool submitRenderCommandBuffer(VkCommandBuffer* command_buffers, VkFence* fences);

    /**
     * Return a handle to the logical device
     * @returns the vulkan device
     */
    VkDevice device();

    /**
     * Gets the depth/stencil buffer format
     * @return pixel format of depth/stencil buffer
     */
    VkFormat getDepthStencilFormat();

    /**
     * Get depth stencil buffer image view
     * @return vulkan image view created on the depth/stencil buffer
     */
    VkImageView getDepthStencilView();

    /**
     * Creates a shader module from a SPIR-V file
     * @param filename Name of file containing shader code
     * @return shader module
     */
    VkShaderModule loadShader(const char* filename);

    /**
     * Gets graphics queue family
     * @return index of graphics queue family
     */
    uint32_t getGraphicsQueueFamily();

    /**
    * Gets presentation queue family
    * @return index of presentation queue family
    */
    uint32_t getPresentationQueueFamily();

    /**
     * Finds memory type index matching requirements
     * @params type_bits memory types supported
     * @params props memory properties supported
     * @return index of first matching memory type
     */
    uint32_t findMemType(uint32_t type_bits, VkMemoryPropertyFlagBits props);
};
