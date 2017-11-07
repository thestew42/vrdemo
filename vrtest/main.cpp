#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <vector>
#include <set>
#include <string>

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
    VkDevice device = nullptr;
    VkQueue gfx_queue = nullptr;
    VkQueue present_queue = nullptr;
    VkSwapchainKHR swapchain = nullptr;

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

        VkExtent2D extent;
        extent.width = MAX(surface_caps.minImageExtent.width, MIN(surface_caps.maxImageExtent.width, WIDTH));
        extent.height = MAX(surface_caps.minImageExtent.height, MIN(surface_caps.maxImageExtent.height, HEIGHT));

        // Query surface for supported formats
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, win_surface, &format_count, nullptr);
        std::vector<VkSurfaceFormatKHR> available_formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, win_surface, &format_count, available_formats.data());

        // Choose image format for presentation, default is first
        VkFormat sc_format = available_formats[0].format;
        VkColorSpaceKHR sc_color_space = available_formats[0].colorSpace;
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
        swapchain_ci.imageExtent = extent;
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

        std::cout << "Swapchain created" << std::endl;
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
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
