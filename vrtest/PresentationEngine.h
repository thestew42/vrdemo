/** @file PresentationEngine.h
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
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <stdint.h>

class PresentationEngine {
private:
    /**
     * Presentation engine resolution
     */
    uint32_t resolution_x;
    uint32_t resolution_y;
    
    /**
     * Name of application
     */
    const char* app_name;

    /**
     * Window handle
     * TODO: move to subclass
     */
    GLFWwindow* window = nullptr;

    /**
     * Vulkan surface handle of the presentation engine
     */
    VkSurfaceKHR win_surface = nullptr;

    /**
     * Swapchain image dimensions
     */
    VkExtent2D sc_extent;

    /**
     * Format of swapchain images
     */
    VkFormat sc_format;
    bool sc_is_srgb;

    /**
     * Color space of swapchain images
     */
    VkColorSpaceKHR sc_color_space;

    /**
     * Number of images in swapchain
     */
    uint32_t sc_image_count;

    /**
     * Vulkan swapchain handle
     */
    VkSwapchainKHR swapchain;

    /**
     * Array of vulkan image handles for the swapchain
     */
    VkImage* sc_images;

    /**
     * Array of vulkan image views for the swapchain images
     */
    VkImageView* sc_image_views;

    /**
    * Semaphores signaled when the swapchain image has been released by the
    * presentation engine
    */
    VkSemaphore* image_ready_semaphores;

    /**
    * Semephores to signal when rendering to a swapchain image is complete
    */
    VkSemaphore* frame_done_semaphores;

    /**
    * Index of next semaphore pair to use for swapchain synchronization
    */
    uint32_t sem_index = 0;

    /**
     * Instance used to create present surface
     */
    VkInstance instance;

    /**
     * Device used to create the swapchain
     */
    VkDevice device;

    /**
     * Callbacks for allocation passed to Vulkan calls
     */
    VkAllocationCallbacks* p_allocs = nullptr;

public:
    /**
     * Constructor initializes the presentation engine. Can be called before any device initialization.
     * @param resolution_x: Requested resolution in x dimension. Actual resolution may vary.
     * @param resolution_y: Requested resolution in y dimension. Actual resolution may vary.
     * @param p_allocs: Allocation callbacks used for Vulkan calls, or nullptr
     * @param app_name: Name of the application to display
     */
    PresentationEngine(uint32_t resolution_x, uint32_t resolution_y, VkAllocationCallbacks* p_allocs,
        const char* app_name);

    /**
     * Destructor
     */
    ~PresentationEngine();

    /**
     * Returns true if the presentation engine has received a signal to shut down
     */
    bool shouldExit();

    /**
     * Polls for events (window signals, etc)
     */
    void pollEvents();

    /**
     * Creates optimal swapchain for device and presentation engine. Also creates synchronization
     *  primitives needed for rendering to the swapchain
     * @param physical_device: The Vulkan physical device being used
     * @param device: The Vulkan logican device to create the swapchain with
     * @param gfx_queue_family: Queue family used for graphics commands
     * @param present_queue_family: Queue family used for present commands
     */
    void createSwapchain(VkPhysicalDevice physical_device, VkDevice device, int gfx_queue_family,
        int present_queue_family);

    /**
     * Gets the index of the next swapchain image to render to
     * @param wait_sem semaphore that will be signaled when the image is free to use
     * @param wait_sem semaphore that presentation engine will wait on before reading image
     * @param index of swapchain image to render to or -1 if none is available
     */
    int getNextSwapchainImage(VkSemaphore** wait_sem, VkSemaphore** signal_sem);

    /**
     * Presents an image back to the engine after rendering
     * @param image_index the swapchain image index to present
     * @param present_queue the queue used for present commands
     */
    void presentSwapchainImage(int image_index, VkQueue present_queue);

    /**
     * Returns the Vulkan surface associated with this presentation engine
     * @param instance: Vulkan instance to use
     * @return surfance handle
     */
    VkSurfaceKHR getPresentSurface(VkInstance instance);

    /**
     * Get length of the swapchain
     * @return number of images in the swapchain
     */
    uint32_t getSwapchainLength();

    /**
     * Get swapchain image view handles
     * @return array of image view handles
     */
    VkImageView* getSwapchainImageViews();

    /**
     * Gets the dimensions of swapchain images
     */
    VkExtent2D getSwapchainExtent();

    /**
     * Gets format of swapchain images
     */
    VkFormat getSwapchainFormat();

    /**
     * Get a list of extension names required to support rendering to this presentation engine
     * @param extension_count: [output] length of extension list
     * @return array of extension name strings
     */
    const char** getRequiredExtensions(uint32_t* extension_count);

    /**
     * Get the application name
     */
    const char* getAppName();

    /**
     * Get the resolution of the presentation surface
     */
    void getResolution(uint32_t* x, uint32_t* y);
};
