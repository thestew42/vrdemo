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

#include "Renderer.h"

#include <iostream>

Renderer::Renderer(GraphicsDevice* graphics_device, PresentationEngine* presentation_engine,
    VkAllocationCallbacks* p_allocs)
{
    this->graphics_device = graphics_device;
    this->presentation_engine = presentation_engine;
    this->p_allocs = p_allocs;
    this->sc_image_count = presentation_engine->getSwapchainLength();

    createCommandPool();
}

Renderer::~Renderer() {
    VkDevice device = graphics_device->device();
    vkDestroyCommandPool(device, command_pool, p_allocs);

    vkDestroyBuffer(device, vertex_buffer, p_allocs);
    vkFreeMemory(device, vertex_buffer_mem, p_allocs);

    vkDestroyPipeline(device, pipeline, p_allocs);
    vkDestroyShaderModule(device, vert_shader, p_allocs);
    vkDestroyShaderModule(device, frag_shader, p_allocs);
    vkDestroyPipelineLayout(device, pipeline_layout, p_allocs);

    vkDestroyRenderPass(device, render_pass, p_allocs);

    for (unsigned int i = 0; i < sc_image_count; i++) {
        vkDestroyFramebuffer(device, framebuffers[i], p_allocs);
        vkDestroyFence(device, cmd_buffer_fences[i], p_allocs);
    }

    delete[] framebuffers;
    delete[] command_buffers;
    delete[] cmd_buffer_fences;
}

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo pool_ci = {};
    pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.flags = 0;
    pool_ci.queueFamilyIndex = static_cast<uint32_t>(graphics_device->getGraphicsQueueFamily());

    if (vkCreateCommandPool(graphics_device->device(), &pool_ci, p_allocs, &command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    std::cout << "Created command pool" << std::endl;
}

void Renderer::createRenderPass() {
    // Attachment descriptions for color and depth buffer
    VkAttachmentDescription attachments[2] = {};
    attachments[0].flags = 0;
    attachments[0].format = presentation_engine->getSwapchainFormat();
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].flags = 0;
    attachments[1].format = graphics_device->getDepthStencilFormat();
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

    if (vkCreateRenderPass(graphics_device->device(), &render_pass_ci, p_allocs, &render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void Renderer::createPipeline() {
    // Pipeline layout: empty for now
    VkPipelineLayoutCreateInfo playout_ci = {};
    playout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    playout_ci.flags = 0;
    playout_ci.setLayoutCount = 0;
    playout_ci.pSetLayouts = nullptr;
    playout_ci.pushConstantRangeCount = 0;
    playout_ci.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(graphics_device->device(), &playout_ci, p_allocs, &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // Shader stages: vertex and fragment
    vert_shader = graphics_device->loadShader("vert.spv");
    frag_shader = graphics_device->loadShader("frag.spv");

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
    VkExtent2D sc_extent = presentation_engine->getSwapchainExtent();
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
    if (vkCreateGraphicsPipelines(graphics_device->device(), nullptr, 1, &pipeline_ci, p_allocs, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline.");
    }
}

void Renderer::createFramebuffer() {
    VkImageView attachments[2];
    attachments[1] = graphics_device->getDepthStencilView();

    uint32_t sc_image_count = presentation_engine->getSwapchainLength();
    VkImageView* sc_image_views = presentation_engine->getSwapchainImageViews();
    VkExtent2D sc_extent = presentation_engine->getSwapchainExtent();

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

        if (vkCreateFramebuffer(graphics_device->device(), &framebuffer_ci, p_allocs, &(framebuffers[i])) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void Renderer::createVertexBuffer() {
    VkDevice device = graphics_device->device();

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
    alloc_info.memoryTypeIndex = graphics_device->findMemType(mem_req.memoryTypeBits,
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

void Renderer::createCommandBuffer() {
    // Create pipeline related objects
    createRenderPass();
    createPipeline();
    createFramebuffer();
    createVertexBuffer();

    uint32_t sc_image_count = presentation_engine->getSwapchainLength();
    command_buffers = new VkCommandBuffer[sc_image_count];
    cmd_buffer_fences = new VkFence[sc_image_count];

    // Allocate the command buffer from the pool
    VkCommandBufferAllocateInfo buffer_ai = {};
    buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_ai.commandPool = command_pool;
    buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buffer_ai.commandBufferCount = sc_image_count;

    if (vkAllocateCommandBuffers(graphics_device->device(), &buffer_ai, command_buffers) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }

    VkFenceCreateInfo fence_ci = {};
    fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

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
        rp_begin_info.renderArea.extent = presentation_engine->getSwapchainExtent();
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

        // Create fence for the command buffer
        if (vkCreateFence(graphics_device->device(), &fence_ci, p_allocs, &(cmd_buffer_fences[i])) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create fence");
        }
    }
}

void Renderer::drawFrame() {
    graphics_device->submitRenderCommandBuffer(command_buffers, cmd_buffer_fences);
}
