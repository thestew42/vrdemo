/** @file GraphicsDevice.cpp
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

#include <vector>
#include <set>
#include <iostream>
#include <stdexcept>
#include <fstream>

#include "GraphicsDevice.h"

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {

    auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback,
    const VkAllocationCallbacks* pAllocator) {

    auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr) {
        func(instance, callback, pAllocator);
    }
}

GraphicsDevice::GraphicsDevice(PresentationEngine* presentation_engine, VkAllocationCallbacks* p_allocs) {
    this->present = presentation_engine;
    this->p_allocs = p_allocs;
    initVulkan();
}

GraphicsDevice::~GraphicsDevice() {
    vkDestroyImageView(m_device, ds_buffer_view, p_allocs);
    vkDestroyImage(m_device, ds_buffer, p_allocs);
    vkFreeMemory(m_device, ds_buffer_mem, p_allocs);
    vkDestroyDevice(m_device, p_allocs);
#ifdef DEBUG
    DestroyDebugReportCallbackEXT(instance, debug_callback, p_allocs);
#endif
    vkDestroyInstance(instance, p_allocs);
}

void GraphicsDevice::initVulkan() {
    createInstance();
#ifdef DEBUG
    enableDebugCallback();
#endif
    selectDevice();
    createDeviceAndQueues();
    createDepthBuffer();
}

void GraphicsDevice::createInstance() {
    // Description of this application
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = present->getAppName();
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "Armageddon Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 1);
    app_info.apiVersion = VK_API_VERSION_1_0;

    // Vulkan instance create info
    VkInstanceCreateInfo inst_ci = {};
    inst_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_ci.pApplicationInfo = &app_info;

    // Enumerate extensions and layers needed for instance
    unsigned int pe_extension_count = 0;
    const char** pe_extensions;
    pe_extensions = present->getRequiredExtensions(&pe_extension_count);

    std::vector<const char*> requested_extensions;
    for (unsigned int i = 0; i < pe_extension_count; i++) {
        requested_extensions.push_back(pe_extensions[i]);
    }

#ifdef DEBUG
    inst_ci.enabledLayerCount = 1;
    inst_ci.ppEnabledLayerNames = &validation_layer;

    requested_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#else
    inst_ci.enabledLayerCount = 0;
    inst_ci.ppEnabledLayerNames = nullptr;
#endif

    inst_ci.enabledExtensionCount = static_cast<uint32_t>(requested_extensions.size());
    inst_ci.ppEnabledExtensionNames = requested_extensions.data();

    // Enumerate available extensions for information purposes
    uint32_t extension_count;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> extensions(extension_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
    std::cout << "available Vulkan extensions:" << std::endl;
    for (const auto& extension : extensions) {
        std::cout << "\t" << extension.extensionName << std::endl;
    }

    std::cout << "required extensions for VR test:" << std::endl;
    for (unsigned int i = 0; i < requested_extensions.size(); i++) {
        std::cout << "\t" << requested_extensions[i] << std::endl;
    }

    // Enumerate available layers for information purposes
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
    std::cout << "Available Vulkan instance layers:" << std::endl;
    for (const auto& layer : layers) {
        std::cout << "\t" << layer.layerName << std::endl;
    }

    std::cout << "required layers for VR test:" << std::endl;
    for (unsigned int i = 0; i < inst_ci.enabledLayerCount; i++) {
        std::cout << "\t" << inst_ci.ppEnabledLayerNames[i] << std::endl;
    }

    // Create vulkan instance
    if (vkCreateInstance(&inst_ci, p_allocs, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void GraphicsDevice::selectDevice() {
    VkPhysicalDeviceProperties device_props;

    // Enumerate available devices
    uint32_t device_count;
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());

    // Print requested extensions for information purposes
    std::cout << "Requested device extensions: " << std::endl;
    for (const auto& extension : dev_extensions)
    {
        std::cout << "\t" << extension << std::endl;
    }

    // Find a graphics device
    bool found_device = false;
    std::cout << "Vulkan physical devices:" << std::endl;
    for (unsigned int i = 0; i < device_count; i++) {
        vkGetPhysicalDeviceProperties(physical_devices[i], &device_props);
        std::cout << "\t" << device_props.deviceName << std::endl;

        if (device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
            device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            // Check for all required device extensions
            uint32_t extension_count;
            vkEnumerateDeviceExtensionProperties(physical_devices[i], nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> supported_extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(physical_devices[i], nullptr, &extension_count, supported_extensions.data());

            std::set<std::string> req_extensions(dev_extensions.begin(), dev_extensions.end());
            std::cout << "Device supported extensions: " << std::endl;
            for (const auto& extension : supported_extensions) {
                std::cout << "\t" << extension.extensionName << std::endl;
                req_extensions.erase(extension.extensionName);
            }

            if (req_extensions.empty()) {
                found_device = true;
                physical_device = physical_devices[i];
            }
        }
    }

    if (!found_device) {
        throw std::runtime_error("Failed to find a suitable Vulkan device");
    }

    // Get presentation engine surface handle
    VkSurfaceKHR pe_surface = present->getPresentSurface(instance);

    // Find graphics and present queue families
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

    for (unsigned int i = 0; i < queue_family_count; i++) {
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
            queue_families[i].queueCount > 0 &&
            gfx_queue_family < 0) {

            gfx_queue_family = i;
        }

        // check queue for surface presentation support
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, pe_surface, &present_support);
        if (present_support && present_queue_family < 0) {
            present_queue_family = i;
        }
    }

    if (gfx_queue_family < 0) {
        throw std::runtime_error("Device has no graphics queue family");
    }

    if (present_queue_family < 0) {
        throw std::runtime_error("Device has no queue family which can present to window");
    }

    vkGetPhysicalDeviceProperties(physical_device, &device_props);
    std::cout << "Using device: " << device_props.deviceName << std::endl;

    // Get memory properties
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
}

void GraphicsDevice::createDeviceAndQueues() {
    // Define queue creation params
    float queue_pri = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_cis;

    VkDeviceQueueCreateInfo gfx_queue_ci = {};
    gfx_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    gfx_queue_ci.queueFamilyIndex = gfx_queue_family;
    gfx_queue_ci.queueCount = 1;
    gfx_queue_ci.pQueuePriorities = &queue_pri;
    queue_cis.push_back(gfx_queue_ci);

    if (gfx_queue_family != present_queue_family) {
        VkDeviceQueueCreateInfo present_queue_ci = {};
        present_queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        present_queue_ci.queueFamilyIndex = present_queue_family;
        present_queue_ci.queueCount = 1;
        present_queue_ci.pQueuePriorities = &queue_pri;
        queue_cis.push_back(present_queue_ci);
    }

    // Device features: TODO
    VkPhysicalDeviceFeatures dev_features = {};

    // Create logical device
    VkDeviceCreateInfo device_ci = {};
    device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount = static_cast<uint32_t>(queue_cis.size());
    device_ci.pQueueCreateInfos = queue_cis.data();

#ifdef DEBUG
    device_ci.enabledLayerCount = 1;
    device_ci.ppEnabledLayerNames = &validation_layer;
#else
    device_ci.enabledLayerCount = 0;
    device_ci.ppEnabledLayerNames = nullptr;
#endif

    device_ci.enabledExtensionCount = static_cast<uint32_t>(dev_extensions.size());
    device_ci.ppEnabledExtensionNames = dev_extensions.data();
    device_ci.pEnabledFeatures = &dev_features;

    if (vkCreateDevice(physical_device, &device_ci, p_allocs, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    std::cout << "Logical device created" << std::endl;

    // Get queue handles
    vkGetDeviceQueue(m_device, gfx_queue_family, 0, &gfx_queue);
    vkGetDeviceQueue(m_device, present_queue_family, 0, &present_queue);

    // Create swapchain
    present->createSwapchain(physical_device, m_device, gfx_queue_family, present_queue_family);
}

void GraphicsDevice::createDepthBuffer() {
    // Determine the format to use
    VkFormatProperties format_props;
    ds_format = VK_FORMAT_D24_UNORM_S8_UINT;
    vkGetPhysicalDeviceFormatProperties(physical_device, ds_format, &format_props);

    if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        std::cout << "Using depth buffer format D24_UNORM_S8_UINT" << std::endl;
    }
    else {
        ds_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        vkGetPhysicalDeviceFormatProperties(physical_device, ds_format, &format_props);
        if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            std::cout << "Using depth buffer format D32_SFLOAT_S8_UINT";
        }
        else {
            throw std::runtime_error("Failed to find suitable depth/stencil format.");
        }
    }

    // Create depth buffer
    VkExtent2D sc_extent = present->getSwapchainExtent();
    VkExtent3D ds_extent;
    ds_extent.depth = 1;
    ds_extent.width = sc_extent.width;
    ds_extent.height = sc_extent.height;

    VkImageCreateInfo image_ci = {};
    image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.flags = 0;
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.format = ds_format;
    image_ci.extent = ds_extent;
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(m_device, &image_ci, p_allocs, &ds_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth/stencil buffer image");
    }

    // Query for memory requirements and allocate backing memory
    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(m_device, ds_buffer, &mem_req);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = findMemType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &alloc_info, p_allocs, &ds_buffer_mem) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate device memory for depth/stencil buffer");
    }

    if (vkBindImageMemory(m_device, ds_buffer, ds_buffer_mem, 0) != VK_SUCCESS) {
        throw std::runtime_error("Failed to bind memory to depth/stencil buffer");
    }

    // Create depth stencil buffer view
    VkImageViewCreateInfo ds_view_ci = {};
    ds_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ds_view_ci.flags = 0;
    ds_view_ci.image = ds_buffer;
    ds_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ds_view_ci.format = ds_format;
    ds_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ds_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ds_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ds_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ds_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    ds_view_ci.subresourceRange.baseMipLevel = 0;
    ds_view_ci.subresourceRange.levelCount = 1;
    ds_view_ci.subresourceRange.baseArrayLayer = 0;
    ds_view_ci.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device, &ds_view_ci, p_allocs, &ds_buffer_view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create view for depth/stencil buffer");
    }
}

bool GraphicsDevice::submitRenderCommandBuffer(VkCommandBuffer* command_buffers, VkFence* fences) {
    VkSemaphore* wait_sem;
    VkSemaphore* signal_sem;
    int sc_index = present->getNextSwapchainImage(&wait_sem, &signal_sem);
    if (sc_index < 0) {
        // swapchain not ready
        return false;
    }

    // Wait for last submission of the command buffer to complete
    VkResult fence_result = vkWaitForFences(m_device, 1, &(fences[sc_index]), VK_TRUE, 1000000);
    if (fence_result == VK_TIMEOUT) {
        // Can't re-sumbit command buffer yet
        return false;
    }
    else if (fence_result != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for command buffer fence");
    }
    if (vkResetFences(m_device, 1, &(fences[sc_index])) != VK_SUCCESS) {
        throw std::runtime_error("Failed to reset command buffer fence");
    }

    // Submit render command buffer to queue
    VkPipelineStageFlags wait_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_sem;
    submit_info.pWaitDstStageMask = &wait_flags;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &(command_buffers[sc_index]);
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_sem;

    if (vkQueueSubmit(gfx_queue, 1, &submit_info, fences[sc_index]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer to queue");
    }

    present->presentSwapchainImage(sc_index, present_queue);

    return true;
}

VkShaderModule GraphicsDevice::loadShader(const char* filename) {
    // Open source file
    std::ifstream shader_file(filename, std::ios::ate | std::ios::binary);

    if (!shader_file.is_open()) {
        throw std::runtime_error("Failed to open shader source file");
    }

    // Load code into buffer
    size_t code_size = static_cast<size_t>(shader_file.tellg());
    char* code = new char[code_size];
    shader_file.seekg(0);
    shader_file.read(code, code_size);
    shader_file.close();

    // Create shader module
    VkShaderModuleCreateInfo shader_ci = {};
    shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_ci.flags = 0;
    shader_ci.codeSize = code_size;
    shader_ci.pCode = reinterpret_cast<const uint32_t*>(code);

    VkShaderModule shader;
    if (vkCreateShaderModule(m_device, &shader_ci, p_allocs, &shader) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    // Release buffer
    delete[] code;

    return shader;
}

VkDevice GraphicsDevice::device() {
    return m_device;
}

VkFormat GraphicsDevice::getDepthStencilFormat() {
    return ds_format;
}

VkImageView GraphicsDevice::getDepthStencilView() {
    return ds_buffer_view;
}

uint32_t GraphicsDevice::getGraphicsQueueFamily() {
    return gfx_queue_family;
}

uint32_t GraphicsDevice::getPresentationQueueFamily() {
    return present_queue_family;
}

uint32_t GraphicsDevice::findMemType(uint32_t type_bits, VkMemoryPropertyFlagBits props) {
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_bits & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find memory type");
}

void GraphicsDevice::enableDebugCallback() {
    VkDebugReportCallbackCreateInfoEXT debug_ci;
    debug_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
    debug_ci.pfnCallback = debugCallback;

    if (CreateDebugReportCallbackEXT(instance, &debug_ci, p_allocs, &debug_callback) != VK_SUCCESS) {
        throw std::runtime_error("Failed to enable debug callback");
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL GraphicsDevice::debugCallback(VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* msg,
    void* userData) {

    std::cerr << "[VULKAN VALIDATION]: " << msg << std::endl;
    return VK_FALSE;
}
