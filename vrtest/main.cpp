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

    VkFormat ds_format;
    VkImage ds_buffer;
    VkDeviceMemory ds_buffer_mem;

    VkShaderModule vert_shader;
    VkShaderModule frag_shader;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

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
        if (vkCreateInstance(&inst_ci, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &win_surface) != VK_SUCCESS) {
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

        if (vkCreateDevice(physical_device, &device_ci, nullptr, &device) != VK_SUCCESS) {
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
        if (vkCreateSwapchainKHR(device, &swapchain_ci, nullptr, &swapchain) != VK_SUCCESS) {
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

            if (vkCreateImageView(device, &image_view_ci, nullptr, &(sc_image_views[i])) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image view for swapchain image");
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

        if (vkCreateImage(device, &image_ci, nullptr, &ds_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create depth/stencil buffer image");
        }

        // Query for memory requirements and allocate backing memory
        VkMemoryRequirements mem_req;
        vkGetImageMemoryRequirements(device, ds_buffer, &mem_req);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_req.size;
        alloc_info.memoryTypeIndex = findMemType(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        if (vkAllocateMemory(device, &alloc_info, nullptr, &ds_buffer_mem) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate device memory for depth/stencil buffer");
        }

        if (vkBindImageMemory(device, ds_buffer, ds_buffer_mem, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind memory to depth buffer");
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

        // Render pass specification
        VkRenderPassCreateInfo render_pass_ci = {};
        render_pass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_ci.flags = 0;
        render_pass_ci.attachmentCount = 2;
        render_pass_ci.pAttachments = attachments;
        render_pass_ci.subpassCount = 1;
        render_pass_ci.pSubpasses = &subpass;
        render_pass_ci.dependencyCount = 0;
        render_pass_ci.pDependencies = nullptr;

        if (vkCreateRenderPass(device, &render_pass_ci, nullptr, &render_pass) != VK_SUCCESS) {
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

        if (vkCreatePipelineLayout(device, &playout_ci, nullptr, &pipeline_layout) != VK_SUCCESS) {
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
        VkVertexInputBindingDescription vi_bindings[2] = {};
        vi_bindings[0].binding = 0;
        vi_bindings[0].stride = 24;
        vi_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vi_bindings[1].binding = 1;
        vi_bindings[1].stride = 24;
        vi_bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vi_attributes[2] = {};
        vi_attributes[0].binding = 0;
        vi_attributes[0].location = 0;
        vi_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vi_attributes[0].offset = 0;
        vi_attributes[1].binding = 1;
        vi_attributes[1].location = 1;
        vi_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vi_attributes[1].offset = 12;

        VkPipelineVertexInputStateCreateInfo vi_state_ci = {};
        vi_state_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi_state_ci.flags = 0;
        vi_state_ci.vertexBindingDescriptionCount = 2;
        vi_state_ci.pVertexBindingDescriptions = vi_bindings;
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
        if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipeline_ci, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline.");
        }
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyShaderModule(device, vert_shader, nullptr);
        vkDestroyShaderModule(device, frag_shader, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        vkDestroyRenderPass(device, render_pass, nullptr);
        vkDestroyImage(device, ds_buffer, nullptr);
        vkFreeMemory(device, ds_buffer_mem, nullptr);

        for (unsigned int i = 0; i < sc_image_count; i++) {
            vkDestroyImageView(device, sc_image_views[i], nullptr);
        }

        delete[] sc_image_views;
        delete[] sc_images;
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);

        if (debug)
            DestroyDebugReportCallbackEXT(instance, debug_callback, nullptr);

        vkDestroySurfaceKHR(instance, win_surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void enableDebugCallback() {
        VkDebugReportCallbackCreateInfoEXT debug_ci;
        debug_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        debug_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        debug_ci.pfnCallback = debugCallback;

        if (CreateDebugReportCallbackEXT(instance, &debug_ci, nullptr, &debug_callback) != VK_SUCCESS) {
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
        if (vkCreateShaderModule(device, &shader_ci, nullptr, &shader) != VK_SUCCESS) {
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

        std::cerr << "Vulkan validation: " << msg << std::endl;
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
        return EXIT_FAILURE;
    }

    std::cout << "Press enter to exit.";
    getchar();

    return EXIT_SUCCESS;
}
