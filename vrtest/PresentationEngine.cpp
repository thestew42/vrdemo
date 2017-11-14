/** @file PresentationEngine.cpp
*
* @brief Defines class that handles creation and management of presentation
*   engine used for displaying images to the user (window, HMD)
*
* Copyright 2017, Stewart Hall
*
* @author		Stewart Hall (www.stewartghall.com)
* @date			11/12/2017
* @copyright	Copyright 2017, Stewart Hall
*/

#include <stdexcept>
#include <vector>
#include <iostream>

#include "PresentationEngine.h"
#include "Common.h"

PresentationEngine::PresentationEngine(uint32_t resolution_x, uint32_t resolution_y, VkAllocationCallbacks* p_allocs,
    const char* app_name) {

    this->resolution_x = resolution_x;
    this->resolution_y = resolution_y;
    this->app_name = app_name;
    this->p_allocs = p_allocs;

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(resolution_x, resolution_y, "VR Test", nullptr, nullptr);
}

PresentationEngine::~PresentationEngine() {
    for (unsigned int i = 0; i < sc_image_count; i++) {
        vkDestroyImageView(device, sc_image_views[i], p_allocs);
        vkDestroySemaphore(device, image_ready_semaphores[i], p_allocs);
        vkDestroySemaphore(device, frame_done_semaphores[i], p_allocs);
    }

    vkDestroySwapchainKHR(device, swapchain, p_allocs);
    vkDestroySurfaceKHR(instance, win_surface, p_allocs);

    glfwDestroyWindow(window);
    glfwTerminate();

    delete[] image_ready_semaphores;
    delete[] frame_done_semaphores;
    delete[] sc_image_views;
    delete[] sc_images;
}

bool PresentationEngine::shouldExit() {
    return glfwWindowShouldClose(window);
}

void PresentationEngine::pollEvents() {
    glfwPollEvents();
}

void PresentationEngine::createSwapchain(VkPhysicalDevice physical_device, VkDevice device, int gfx_queue_family,
    int present_queue_family) {
    if (!win_surface) {
        throw std::runtime_error("Surface must be created before calling createSwapchain");
    }

    this->device = device;

    // Query surface for capabilities
    VkSurfaceCapabilitiesKHR surface_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, win_surface, &surface_caps);

    uint32_t min_images;
    if (surface_caps.maxImageCount == 0)
        min_images = surface_caps.minImageCount + 1;
    else
        min_images = MIN(surface_caps.minImageCount + 1, surface_caps.maxImageCount);

    sc_extent.width = MAX(surface_caps.minImageExtent.width, MIN(surface_caps.maxImageExtent.width, resolution_x));
    sc_extent.height = MAX(surface_caps.minImageExtent.height, MIN(surface_caps.maxImageExtent.height, resolution_y));

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

    image_ready_semaphores = new VkSemaphore[sc_image_count];
    frame_done_semaphores = new VkSemaphore[sc_image_count];
    for (uint32_t i = 0; i < sc_image_count; i++) {
        if (vkCreateSemaphore(device, &semaphore_ci, p_allocs, &(image_ready_semaphores[i])) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sephamore");
        }
        if (vkCreateSemaphore(device, &semaphore_ci, p_allocs, &(frame_done_semaphores[i])) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sephamore");
        }
    }
}

int PresentationEngine::getNextSwapchainImage(VkSemaphore** wait_sem, VkSemaphore** signal_sem) {
    // Get image from swapchain to use in framebuffer
    uint32_t sc_index;
    VkResult sc_result = vkAcquireNextImageKHR(device, swapchain, 0, image_ready_semaphores[sem_index], VK_NULL_HANDLE, &sc_index);
    if (sc_result == VK_NOT_READY) {
        // Image not ready, exit early
        return -1;
    }
    else if (sc_result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire next image from swapchain");
    }

    *wait_sem = &(image_ready_semaphores[sem_index]);
    *signal_sem = &(frame_done_semaphores[sem_index]);
    return static_cast<int>(sc_index);
}

void PresentationEngine::presentSwapchainImage(int image_index, VkQueue present_queue)
{
    uint32_t sc_index = static_cast<uint32_t>(image_index);

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

VkSurfaceKHR PresentationEngine::getPresentSurface(VkInstance instance) {
    if (win_surface)
        return win_surface;

    this->instance = instance;

    if (glfwCreateWindowSurface(instance, window, p_allocs, &win_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    return win_surface;
}

uint32_t PresentationEngine::getSwapchainLength() {
    return sc_image_count;
}

VkImageView* PresentationEngine::getSwapchainImageViews() {
    return sc_image_views;
}

VkExtent2D PresentationEngine::getSwapchainExtent() {
    return sc_extent;
}

VkFormat PresentationEngine::getSwapchainFormat() {
    return sc_format;
}

const char** PresentationEngine::getRequiredExtensions(uint32_t* extension_count) {
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(extension_count);

    return glfw_extensions;
}

const char* PresentationEngine::getAppName() {
    return app_name;
}

void PresentationEngine::getResolution(uint32_t* x, uint32_t* y) {
    *x = resolution_x;
    *y = resolution_y;
}
