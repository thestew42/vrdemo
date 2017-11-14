/** @file Renderer.h
*
* @brief Defines class that converts scenes into command buffers and runs them
*   on the device
*
* Copyright 2017, Stewart Hall
*
* @author		Stewart Hall (www.stewartghall.com)
* @date			11/12/2017
* @copyright	Copyright 2017, Stewart Hall
*/
#pragma once

#include <vulkan/vulkan.h>

#include "GraphicsDevice.h"
#include "PresentationEngine.h"

class Renderer {
private:
    /**
     * The initialized graphics device to use for rendering
     */
    GraphicsDevice* graphics_device;

    /**
     * Presentation engine being used for the swapchain
     */
    PresentationEngine* presentation_engine;

    /**
     * List of framebuffers to render to. One per swapchain image
     */
    VkFramebuffer* framebuffers;

    /**
    * Fences signaled when submitted command buffer moves from pending to ready
    */
    VkFence* cmd_buffer_fences;

    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_mem;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers;

    VkAllocationCallbacks* p_allocs;

    uint32_t sc_image_count;

    void createCommandPool();
    void createRenderPass();
    void createPipeline();
    void createFramebuffer();
    void createVertexBuffer();

public:
    Renderer(GraphicsDevice* graphics_device, PresentationEngine* presentation_engine,
        VkAllocationCallbacks* p_allocs);

    ~Renderer();

    void createCommandBuffer();
    void drawFrame();
};
