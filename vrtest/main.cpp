#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <set>
#include <string>
#include <fstream>

#define MAX(a, b) ((a > b) ? (a) : (b))
#define MIN(a, b) ((a < b) ? (a) : (b))

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

class VRTestApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    const uint32_t WIDTH = 1024;
    const uint32_t HEIGHT = 768;
    const char* validation_layer = "VK_LAYER_LUNARG_standard_validation";

    // List of required device extensions
    const std::vector<const char*> dev_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    GLFWwindow* window = nullptr;

    VkInstance instance;
    VkDebugReportCallbackEXT debug_callback;
    VkSurfaceKHR win_surface = nullptr;
    VkPhysicalDevice physical_device = nullptr;
    VkPhysicalDeviceMemoryProperties mem_props;
    VkDevice device = nullptr;
    VkQueue gfx_queue = nullptr;
    VkQueue present_queue = nullptr;

    VkFormat sc_format;
    VkColorSpaceKHR sc_color_space;
    uint32_t sc_image_count = 0;
    VkExtent2D sc_extent;
    VkImage* sc_images = nullptr;
    VkImageView* sc_image_views = nullptr;
    VkSwapchainKHR swapchain = nullptr;
    VkSemaphore* image_ready_semaphores = nullptr;
    VkSemaphore* frame_done_semaphores = nullptr;
    VkFence* cmd_buffer_fences = nullptr;
    uint32_t sem_index = 0;

    VkFormat ds_format;
    VkImage ds_buffer;
    VkImageView ds_buffer_view;
    VkDeviceMemory ds_buffer_mem;

    VkFramebuffer* framebuffers;

    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_mem;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers;

    VkAllocationCallbacks* p_allocs = nullptr;

    bool sc_is_srgb = false;

    int gfx_queue_family = -1;
    int present_queue_family = -1;

#ifdef DEBUG
    const bool debug = true;
#else
    const bool debug = false;
#endif

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "VR Test", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        if (debug)
            enableDebugCallback();
        createSurface();
        selectDevice();
        createDeviceAndQueues();
        createSwapchain();
        createDepthBuffer();

        createRenderPass();
        createPipeline();
        createFramebuffer();
        createVertexBuffer();

        createCommandPool();
        createCommandBuffer();
    }

    void createInstance() {
        // Description of this application
        VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "VR Test";
        app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        app_info.pEngineName = "Armageddon Engine";
        app_info.engineVersion = VK_MAKE_VERSION(0, 1, 1);
        app_info.apiVersion = VK_API_VERSION_1_0;

        // Vulkan instance create info
        VkInstanceCreateInfo inst_ci = {};
        inst_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        inst_ci.pApplicationInfo = &app_info;

        // Enumerate extensions and layers needed for instance
        unsigned int glfw_extension_count = 0;
        const char** glfw_extensions;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

        std::vector<const char*> requested_extensions;
        for (unsigned int i = 0; i < glfw_extension_count; i++) {
            requested_extensions.push_back(glfw_extensions[i]);
        }

        if (debug) {
            inst_ci.enabledLayerCount = 1;
            inst_ci.ppEnabledLayerNames = &validation_layer;
            
            requested_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }
        else {
            inst_ci.enabledLayerCount = 0;
            inst_ci.ppEnabledLayerNames = nullptr;
        }

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
        for (unsigned int i = 0; i < glfw_extension_count; i++) {
            std::cout << "\t" << glfw_extensions[i] << std::endl;
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

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, p_allocs, &win_surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
    }

    void selectDevice() {
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
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, win_surface, &present_support);
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

    void createDeviceAndQueues() {
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

        if (debug) {
            device_ci.enabledLayerCount = 1;
            device_ci.ppEnabledLayerNames = &validation_layer;
        }
        else {
            device_ci.enabledLayerCount = 0;
            device_ci.ppEnabledLayerNames = nullptr;
        }

        device_ci.enabledExtensionCount = static_cast<uint32_t>(dev_extensions.size());
        device_ci.ppEnabledExtensionNames = dev_extensions.data();
        device_ci.pEnabledFeatures = &dev_features;

        if (vkCreateDevice(physical_device, &device_ci, p_allocs, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        std::cout << "Logical device created" << std::endl;

        // Get queue handles
        vkGetDeviceQueue(device, gfx_queue_family, 0, &gfx_queue);
        vkGetDeviceQueue(device, present_queue_family, 0, &present_queue);
    }

    void createSwapchain() {
        // Query surface for capabilities
        VkSurfaceCapabilitiesKHR surface_caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, win_surface, &surface_caps);
        
        uint32_t min_images;
        if (surface_caps.maxImageCount == 0)
            min_images = surface_caps.minImageCount + 1;
        else
            min_images = MIN(surface_caps.minImageCount + 1, surface_caps.maxImageCount);

        sc_extent.width = MAX(surface_caps.minImageExtent.width, MIN(surface_caps.maxImageExtent.width, WIDTH));
        sc_extent.height = MAX(surface_caps.minImageExtent.height, MIN(surface_caps.maxImageExtent.height, HEIGHT));

        // Query surface for supported formats
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, win_surface, &format_count, nullptr);
        std::vector<VkSurfaceFormatKHR> available_formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, win_surface, &format_count, available_formats.data());

        // Choose image format for presentation, default is first
        sc_format = available_formats[0].format;
        sc_color_space = available_formats[0].colorSpace;
        for (const auto& format : available_formats) {
            // try to choose an sRGB format
            if (format.format == VK_FORMAT_R8G8B8A8_SRGB ||
                format.format == VK_FORMAT_R8G8B8_SRGB ||
                format.format == VK_FORMAT_B8G8R8A8_SRGB ||
                format.format == VK_FORMAT_B8G8R8_SRGB) {

                sc_format = format.format;
                sc_color_space = format.colorSpace;
                sc_is_srgb = true;
            }
        }

        // Query the surface for supported presentation modes
        uint32_t mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, win_surface, &mode_count, nullptr);
        std::vector<VkPresentModeKHR> available_modes(mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, win_surface, &mode_count, available_modes.data());
        
        // Choose presentation mode, default is first
        VkPresentModeKHR sc_present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& mode : available_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                sc_present_mode = mode;
        }

        // Define parameters for swapchain creation
        VkSwapchainCreateInfoKHR swapchain_ci = {};
        swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.flags = 0;
        swapchain_ci.surface = win_surface;
        swapchain_ci.minImageCount = min_images;
        swapchain_ci.imageFormat = sc_format;
        swapchain_ci.imageColorSpace = sc_color_space;
        swapchain_ci.imageExtent = sc_extent;
        swapchain_ci.imageArrayLayers = 1;
        swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_ci.preTransform = surface_caps.currentTransform;
        swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_ci.presentMode = sc_present_mode;
        swapchain_ci.clipped = VK_TRUE;
        swapchain_ci.oldSwapchain = VK_NULL_HANDLE;

        // Swapchain queue ownership properties
        if (gfx_queue_family == present_queue_family) {
            swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchain_ci.queueFamilyIndexCount = 0;
            swapchain_ci.pQueueFamilyIndices = nullptr;
        }
        else {
            uint32_t families[2];
            families[0] = static_cast<uint32_t>(gfx_queue_family);
            families[1] = static_cast<uint32_t>(present_queue_family);
            swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_ci.queueFamilyIndexCount = 2;
            swapchain_ci.pQueueFamilyIndices = families;
        }

        // Create the swapchain
        if (vkCreateSwapchainKHR(device, &swapchain_ci, p_allocs, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain");
        }

        // Get the array of swapchain images
        vkGetSwapchainImagesKHR(device, swapchain, &sc_image_count, nullptr);
        sc_images = new VkImage[sc_image_count];
        vkGetSwapchainImagesKHR(device, swapchain, &sc_image_count, sc_images);

        std::cout << "Swapchain created" << std::endl;
        std::cout << "\tImage count: " << sc_image_count << std::endl;
        std::cout << "\tExtent: " << sc_extent.width << " x " << sc_extent.height << std::endl;
        std::cout << "\tFormat: " << sc_format << std::endl;
        std::cout << "\tColor space: " << sc_color_space << std::endl;

        // Create views for each swapchain image
        sc_image_views = new VkImageView[sc_image_count];
        for (unsigned int i = 0; i < sc_image_count; i++) {
            VkImageViewCreateInfo image_view_ci = {};
            image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_view_ci.flags = 0;
            image_view_ci.image = sc_images[i];
            image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            image_view_ci.format = sc_format;
            image_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_view_ci.subresourceRange.baseMipLevel = 0;
            image_view_ci.subresourceRange.levelCount = 1;
            image_view_ci.subresourceRange.baseArrayLayer = 0;
            image_view_ci.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &image_view_ci, p_allocs, &(sc_image_views[i])) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view for swapchain image");
            }
        }

        // Create semaphores and fences used for swapchain/application synchronization
        VkSemaphoreCreateInfo semaphore_ci = {};
        semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_ci.flags = 0;

        VkFenceCreateInfo fence_ci = {};
        fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        image_ready_semaphores = new VkSemaphore[sc_image_count];
        frame_done_semaphores = new VkSemaphore[sc_image_count];
        cmd_buffer_fences = new VkFence[sc_image_count];
        for (uint32_t i = 0; i < sc_image_count; i++) {
            if (vkCreateSemaphore(device, &semaphore_ci, p_allocs, &(image_ready_semaphores[i])) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create sephamore");
            }
            if (vkCreateSemaphore(device, &semaphore_ci, p_allocs, &(frame_done_semaphores[i])) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create sephamore");
            }
            if (vkCreateFence(device, &fence_ci, p_allocs, &(cmd_buffer_fences[i])) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create fence");
            }
        }
    }

    void createDepthBuffer() {
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

        if (vkCreateImage(device, &image_ci, p_allocs, &ds_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth/stencil buffer image");
        }

        // Query for memory requirements and allocate backing memory
        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(device, ds_buffer, &mem_req);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = findMemType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        if (vkAllocateMemory(device, &alloc_info, p_allocs, &ds_buffer_mem) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate device memory for depth/stencil buffer");
        }

        if (vkBindImageMemory(device, ds_buffer, ds_buffer_mem, 0) != VK_SUCCESS) {
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

        if (vkCreateImageView(device, &ds_view_ci, p_allocs, &ds_buffer_view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create view for depth/stencil buffer");
        }
    }

    void createRenderPass() {
        // Attachment descriptions for color and depth buffer
        VkAttachmentDescription attachments[2] = {};
        attachments[0].flags = 0;
        attachments[0].format = sc_format;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachments[1].flags = 0;
        attachments[1].format = ds_format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_ref = {};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference ds_attachment_ref = {};
        ds_attachment_ref.attachment = 1;
        ds_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Subpass descriptions
        VkSubpassDescription subpass = {};
        subpass.flags = 0;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = &ds_attachment_ref;
        subpass.preserveAttachmentCount = 0;
        subpass.pResolveAttachments = nullptr;

        // Override implicit dependency on swapchain image
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = 0;

        // Render pass specification
        VkRenderPassCreateInfo render_pass_ci = {};
        render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_ci.flags = 0;
        render_pass_ci.attachmentCount = 2;
        render_pass_ci.pAttachments = attachments;
        render_pass_ci.subpassCount = 1;
        render_pass_ci.pSubpasses = &subpass;
        render_pass_ci.dependencyCount = 1;
        render_pass_ci.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &render_pass_ci, p_allocs, &render_pass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass");
        }
    }

    void createPipeline() {
        // Pipeline layout: empty for now
        VkPipelineLayoutCreateInfo playout_ci = {};
        playout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        playout_ci.flags = 0;
        playout_ci.setLayoutCount = 0;
        playout_ci.pSetLayouts = nullptr;
        playout_ci.pushConstantRangeCount = 0;
        playout_ci.pPushConstantRanges = nullptr;

        if (vkCreatePipelineLayout(device, &playout_ci, p_allocs, &pipeline_layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        // Shader stages: vertex and fragment
        vert_shader = loadShader("vert.spv");
        frag_shader = loadShader("frag.spv");

        VkPipelineShaderStageCreateInfo stages_ci[2] = {};
        stages_ci[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages_ci[0].flags = 0;
        stages_ci[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages_ci[0].module = vert_shader;
        stages_ci[0].pName = "main";
        stages_ci[0].pSpecializationInfo = nullptr;

        stages_ci[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages_ci[1].flags = 0;
        stages_ci[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages_ci[1].module = frag_shader;
        stages_ci[1].pName = "main";
        stages_ci[1].pSpecializationInfo = nullptr;

        // Vertex input state: vertex buffer contains position and color data
        VkVertexInputBindingDescription vi_binding = {};
        vi_binding.binding = 0;
        vi_binding.stride = 24;
        vi_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vi_attributes[2] = {};
        vi_attributes[0].binding = 0;
        vi_attributes[0].location = 0;
        vi_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vi_attributes[0].offset = 0;
        vi_attributes[1].binding = 0;
        vi_attributes[1].location = 1;
        vi_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vi_attributes[1].offset = 12;

        VkPipelineVertexInputStateCreateInfo vi_state_ci = {};
        vi_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi_state_ci.flags = 0;
        vi_state_ci.vertexBindingDescriptionCount = 1;
        vi_state_ci.pVertexBindingDescriptions = &vi_binding;
        vi_state_ci.vertexAttributeDescriptionCount = 2;
        vi_state_ci.pVertexAttributeDescriptions = vi_attributes;

        // Input assembly state: triangle list
        VkPipelineInputAssemblyStateCreateInfo ia_state_ci = {};
        ia_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia_state_ci.flags = 0;
        ia_state_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        ia_state_ci.primitiveRestartEnable = VK_FALSE;

        // Viewport state: single viewport and scissor, full screen
        VkViewport viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(sc_extent.width);
        viewport.height = static_cast<float>(sc_extent.height);
        viewport.maxDepth = 1.0f;
        viewport.minDepth = 0.0f;

        VkRect2D scissor;
        scissor.offset = { 0, 0 };
        scissor.extent = sc_extent;

        VkPipelineViewportStateCreateInfo vp_state_ci = {};
        vp_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp_state_ci.flags = 0;
        vp_state_ci.viewportCount = 1;
        vp_state_ci.pViewports = &viewport;
        vp_state_ci.scissorCount = 1;
        vp_state_ci.pScissors = &scissor;

        // Rasterizer state
        VkPipelineRasterizationStateCreateInfo ras_state_ci = {};
        ras_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        ras_state_ci.flags = 0;
        ras_state_ci.depthClampEnable = VK_FALSE;
        ras_state_ci.rasterizerDiscardEnable = VK_FALSE;
        ras_state_ci.polygonMode = VK_POLYGON_MODE_FILL;
        ras_state_ci.cullMode = VK_CULL_MODE_BACK_BIT;
        ras_state_ci.frontFace = VK_FRONT_FACE_CLOCKWISE;
        ras_state_ci.depthBiasEnable = VK_FALSE; // what is this useful for?
        ras_state_ci.depthBiasConstantFactor = 0.0f;
        ras_state_ci.depthBiasClamp = 0.0f;
        ras_state_ci.depthBiasSlopeFactor = 0.0f;
        ras_state_ci.lineWidth = 1.0f;

        // Multisample state
        VkPipelineMultisampleStateCreateInfo ms_state_ci = {};
        ms_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms_state_ci.flags = 0;
        ms_state_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; //TODO: enable msaa once resolve enabled
        ms_state_ci.sampleShadingEnable = VK_FALSE;  //TODO: enable for quality
        ms_state_ci.minSampleShading = 1.0f;
        ms_state_ci.pSampleMask = nullptr;
        ms_state_ci.alphaToCoverageEnable = VK_FALSE; // what is this useful for?
        ms_state_ci.alphaToOneEnable = VK_FALSE; // what is this useful for?

        // Depth stencil state: standard depth buffering, no stencil
        VkPipelineDepthStencilStateCreateInfo ds_state_ci = {};
        ds_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds_state_ci.flags = 0;
        ds_state_ci.depthTestEnable = VK_TRUE;
        ds_state_ci.depthWriteEnable = VK_TRUE;
        ds_state_ci.depthCompareOp = VK_COMPARE_OP_LESS;
        ds_state_ci.depthBoundsTestEnable = VK_FALSE; // what is this useful for?
        ds_state_ci.minDepthBounds = 0.0f;
        ds_state_ci.maxDepthBounds = 0.0f;
        ds_state_ci.stencilTestEnable = VK_FALSE; // TODO: enable later if necessary
        ds_state_ci.front = {};
        ds_state_ci.back = {};

        // Blend state: disabled for now
        VkPipelineColorBlendAttachmentState blend_attachment;
        blend_attachment.blendEnable = VK_FALSE;
        blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | \
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_MAX;

        VkPipelineColorBlendStateCreateInfo blend_state_ci = {};
        blend_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend_state_ci.flags = 0;
        blend_state_ci.logicOpEnable = VK_FALSE; // what is this useful for?
        blend_state_ci.logicOp = VK_LOGIC_OP_COPY;
        blend_state_ci.attachmentCount = 1;
        blend_state_ci.pAttachments = &blend_attachment;
        blend_state_ci.blendConstants[0] = 0.0f;
        blend_state_ci.blendConstants[1] = 0.0f;
        blend_state_ci.blendConstants[2] = 0.0f;
        blend_state_ci.blendConstants[3] = 0.0f;
        
        // Full pipeline description
        VkGraphicsPipelineCreateInfo pipeline_ci = {};
        pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_ci.flags = 0;
        pipeline_ci.stageCount = 2;
        pipeline_ci.pStages = stages_ci;
        pipeline_ci.pVertexInputState = &vi_state_ci;
        pipeline_ci.pInputAssemblyState = &ia_state_ci;
        pipeline_ci.pTessellationState = nullptr;
        pipeline_ci.pViewportState = &vp_state_ci;
        pipeline_ci.pRasterizationState = &ras_state_ci;
        pipeline_ci.pMultisampleState = &ms_state_ci;
        pipeline_ci.pDepthStencilState = &ds_state_ci;
        pipeline_ci.pColorBlendState = &blend_state_ci;
        pipeline_ci.pDynamicState = nullptr;
        pipeline_ci.layout = pipeline_layout;
        pipeline_ci.renderPass = render_pass;
        pipeline_ci.subpass = 0;
        pipeline_ci.basePipelineHandle = nullptr;
        pipeline_ci.basePipelineIndex = 0;
        
        // TODO: pipeline cache
        if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipeline_ci, p_allocs, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline.");
        }
    }

    void createFramebuffer() {
        VkImageView attachments[2];
        attachments[1] = ds_buffer_view;

        framebuffers = new VkFramebuffer[sc_image_count];
        VkFramebufferCreateInfo framebuffer_ci = {};
        for (uint32_t i = 0; i < sc_image_count; i++) {
            attachments[0] = sc_image_views[i];
            
            framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_ci.flags = 0;
            framebuffer_ci.renderPass = render_pass;
            framebuffer_ci.attachmentCount = 2;
            framebuffer_ci.pAttachments = attachments;
            framebuffer_ci.width = sc_extent.width;
            framebuffer_ci.height = sc_extent.height;
            framebuffer_ci.layers = 1;

            if (vkCreateFramebuffer(device, &framebuffer_ci, p_allocs, &(framebuffers[i])) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer");
            }
        }
    }

    void createVertexBuffer() {
        // Create vertex buffer object
        VkBufferCreateInfo buffer_ci = {};
        buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_ci.flags = 0;
        buffer_ci.size = 36 * 3;
        buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &buffer_ci, p_allocs, &vertex_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create vertex buffer");
        }

        // Allocate memory and bind to vertex buffer
        VkMemoryRequirements mem_req;
        vkGetBufferMemoryRequirements(device, vertex_buffer, &mem_req);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = findMemType(mem_req.memoryTypeBits, 
            static_cast<VkMemoryPropertyFlagBits>(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

        if (vkAllocateMemory(device, &alloc_info, p_allocs, &vertex_buffer_mem) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate device memory for vertex buffer");
        }

        if (vkBindBufferMemory(device, vertex_buffer, vertex_buffer_mem, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind memory to vertex buffer");
        }

        // Copy vertex data to vertex buffer
        float vertex_data[] = {
            -0.6f, -0.3f, 0.5f,
            1.0f, 0.0f, 0.0f,
            -0.3f, 0.3f, 0.5f,
            0.0f, 1.0f, 0.0f,
            -0.9f, 0.3f, 0.5f,
            0.0f, 0.0f, 1.0f,

            0.0f, -0.3f, 0.5f,
            1.0f, 0.0f, 0.0f,
            0.3f, 0.3f, 0.5f,
            0.0f, 1.0f, 0.0f,
            -0.3f, 0.3f, 0.5f,
            0.0f, 0.0f, 1.0f,

            0.6f, -0.3f, 0.5f,
            1.0f, 0.0f, 0.0f,
            0.9f, 0.3f, 0.5f,
            0.0f, 1.0f, 0.0f,
            0.3f, 0.3f, 0.5f,
            0.0f, 0.0f, 1.0f
        };
        void* mapped_data;
        if (vkMapMemory(device, vertex_buffer_mem, 0, mem_req.size, 0, &mapped_data) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map vertex buffer memory to host");
        }
        memcpy(mapped_data, vertex_data, mem_req.size);
        vkUnmapMemory(device, vertex_buffer_mem);

        std::cout << "Finished creating vertex buffer" << std::endl;
    }

    void createCommandPool() {
        VkCommandPoolCreateInfo pool_ci = {};
        pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_ci.flags = 0;
        pool_ci.queueFamilyIndex = static_cast<uint32_t>(gfx_queue_family);

        if (vkCreateCommandPool(device, &pool_ci, p_allocs, &command_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }

        std::cout << "Created command pool" << std::endl;
    }

    void createCommandBuffer() {
        command_buffers = new VkCommandBuffer[sc_image_count];

        // Allocate the command buffer from the pool
        VkCommandBufferAllocateInfo buffer_ai = {};
        buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buffer_ai.commandPool = command_pool;
        buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        buffer_ai.commandBufferCount = sc_image_count;

        if (vkAllocateCommandBuffers(device, &buffer_ai, command_buffers) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer");
        }

        VkClearColorValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
        VkClearValue clear_values[2];
        clear_values[0].color = clear_color;
        clear_values[1].depthStencil.depth = 1.0f;

        VkDeviceSize vtx_buffer_offset = 0;

        for (uint32_t i = 0; i < sc_image_count; i++) {
            // Begin recording command buffer
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = 0;
            begin_info.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
                throw std::runtime_error("Failed to begin command buffer recording");
            }

            // Record the render pass and draw triangle
            VkRenderPassBeginInfo rp_begin_info = {};
            rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rp_begin_info.renderPass = render_pass;
            rp_begin_info.framebuffer = framebuffers[i];
            rp_begin_info.renderArea.offset = { 0, 0 };
            rp_begin_info.renderArea.extent = sc_extent;
            rp_begin_info.clearValueCount = 2;
            rp_begin_info.pClearValues = clear_values;

            vkCmdBeginRenderPass(command_buffers[i], &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &vertex_buffer, &vtx_buffer_offset);
            vkCmdDraw(command_buffers[i], 9, 1, 0, 0);

            vkCmdEndRenderPass(command_buffers[i]);

            // Finish recording command buffer
            if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to end command buffer recording");
            }
        }
    }

    void drawFrame() {
        // Get image from swapchain to use in framebuffer
        uint32_t sc_index;
        VkResult sc_result = vkAcquireNextImageKHR(device, swapchain, 0, image_ready_semaphores[sem_index], VK_NULL_HANDLE, &sc_index);
        if (sc_result == VK_NOT_READY) {
            // Image not ready, exit early
            return;
        }
        else if (sc_result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire next image from swapchain");
        }

        // Wait for last submission of the command buffer to complete
        VkResult fence_result = vkWaitForFences(device, 1, &(cmd_buffer_fences[sc_index]), VK_TRUE, 1000000);
        if (fence_result == VK_TIMEOUT) {
            // Can't re-sumbit command buffer yet
            return;
        }
        else if (fence_result != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for command buffer fence");
        }
        if (vkResetFences(device, 1, &(cmd_buffer_fences[sc_index])) != VK_SUCCESS) {
            throw std::runtime_error("Failed to reset command buffer fence");
        }

        // Submit render command buffer to queue
        VkPipelineStageFlags wait_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &(image_ready_semaphores[sem_index]);
        submit_info.pWaitDstStageMask = &wait_flags;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &(command_buffers[sc_index]);
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &(frame_done_semaphores[sem_index]);

        if (vkQueueSubmit(gfx_queue, 1, &submit_info, cmd_buffer_fences[sc_index]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit command buffer to queue");
        }

        // Present image back to swapchain
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &(frame_done_semaphores[sem_index]);
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain;
        present_info.pImageIndices = &sc_index;
        present_info.pResults = nullptr;

        if (vkQueuePresentKHR(present_queue, &present_info) != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swapchain image");
        }

        // Cycle to next semaphore set
        sem_index++;
        if (sem_index == sc_image_count) {
            sem_index = 0;
        }
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device);
    }

    void cleanup() {
        vkDestroyCommandPool(device, command_pool, p_allocs);

        vkDestroyBuffer(device, vertex_buffer, p_allocs);
        vkFreeMemory(device, vertex_buffer_mem, p_allocs);

        vkDestroyPipeline(device, pipeline, p_allocs);
        vkDestroyShaderModule(device, vert_shader, p_allocs);
        vkDestroyShaderModule(device, frag_shader, p_allocs);
        vkDestroyPipelineLayout(device, pipeline_layout, p_allocs);

        vkDestroyRenderPass(device, render_pass, p_allocs);
        vkDestroyImageView(device, ds_buffer_view, p_allocs);
        vkDestroyImage(device, ds_buffer, p_allocs);
        vkFreeMemory(device, ds_buffer_mem, p_allocs);

        for (unsigned int i = 0; i < sc_image_count; i++) {
            vkDestroyImageView(device, sc_image_views[i], p_allocs);
            vkDestroyFramebuffer(device, framebuffers[i], p_allocs);
            vkDestroySemaphore(device, image_ready_semaphores[i], p_allocs);
            vkDestroySemaphore(device, frame_done_semaphores[i], p_allocs);
            vkDestroyFence(device, cmd_buffer_fences[i], p_allocs);
        }

        vkDestroySwapchainKHR(device, swapchain, p_allocs);
        vkDestroyDevice(device, p_allocs);

        if (debug)
            DestroyDebugReportCallbackEXT(instance, debug_callback, p_allocs);

        vkDestroySurfaceKHR(instance, win_surface, p_allocs);
        vkDestroyInstance(instance, p_allocs);

        glfwDestroyWindow(window);
        glfwTerminate();

        delete[] cmd_buffer_fences;
        delete[] image_ready_semaphores;
        delete[] frame_done_semaphores;
        delete[] sc_image_views;
        delete[] sc_images;
        delete[] framebuffers;
        delete[] command_buffers;
    }

    void enableDebugCallback() {
        VkDebugReportCallbackCreateInfoEXT debug_ci;
        debug_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        debug_ci.pfnCallback = debugCallback;

        if (CreateDebugReportCallbackEXT(instance, &debug_ci, p_allocs, &debug_callback) != VK_SUCCESS) {
            throw std::runtime_error("Failed to enable debug callback");
        }
    }

    uint32_t findMemType(uint32_t type_bits, VkMemoryPropertyFlagBits props) {
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
            if ((type_bits & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find memory type");
    }

    VkShaderModule loadShader(const char* filename) {
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
        if (vkCreateShaderModule(device, &shader_ci, p_allocs, &shader) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module");
        }

        // Release buffer
        delete[] code;

        return shader;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
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
};

int main() {
    VRTestApp app;

    try {
        app.run();
    }
    catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        getchar();
        return EXIT_FAILURE;
    }

    std::cout << "Press enter to exit.";
    getchar();

    return EXIT_SUCCESS;
}
