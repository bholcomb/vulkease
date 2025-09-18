/**
 * @file ve_draw.c
 * @brief Drawing and State Management Implementation  
 */

#include "ve_internal.h"

// =============================================================================
// Viewport and Scissor Management
// =============================================================================

void veSetViewport(VECommandBuffer* cmd, float x, float y, float width, float height,
                  float minDepth, float maxDepth) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    VkViewport viewport = {0};
    viewport.x = x;
    viewport.y = y;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    
    vkCmdSetViewport(internal->commandBuffer, 0, 1, &viewport);
}

void veSetScissor(VECommandBuffer* cmd, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    VkRect2D scissor = {0};
    scissor.offset.x = x;
    scissor.offset.y = y;
    scissor.extent.width = width;
    scissor.extent.height = height;
    
    vkCmdSetScissor(internal->commandBuffer, 0, 1, &scissor);
}

void veSetViewports(VECommandBuffer* cmd, uint32_t firstViewport, uint32_t viewportCount,
                   const VEViewport* viewports) {
    if (!cmd || !viewports || viewportCount == 0) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    VkViewport vkViewports[16];
    uint32_t count = viewportCount < 16 ? viewportCount : 16;
    
    for (uint32_t i = 0; i < count; i++) {
        vkViewports[i].x = viewports[i].x;
        vkViewports[i].y = viewports[i].y;
        vkViewports[i].width = viewports[i].width;
        vkViewports[i].height = viewports[i].height;
        vkViewports[i].minDepth = viewports[i].minDepth;
        vkViewports[i].maxDepth = viewports[i].maxDepth;
    }
    
    vkCmdSetViewport(internal->commandBuffer, firstViewport, count, vkViewports);
}

void veSetScissors(VECommandBuffer* cmd, uint32_t firstScissor, uint32_t scissorCount,
                  const VERect2D* scissors) {
    if (!cmd || !scissors || scissorCount == 0) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    VkRect2D vkScissors[16];
    uint32_t count = scissorCount < 16 ? scissorCount : 16;
    
    for (uint32_t i = 0; i < count; i++) {
        vkScissors[i].offset.x = scissors[i].x;
        vkScissors[i].offset.y = scissors[i].y;
        vkScissors[i].extent.width = scissors[i].width;
        vkScissors[i].extent.height = scissors[i].height;
    }
    
    vkCmdSetScissor(internal->commandBuffer, firstScissor, count, vkScissors);
}

// =============================================================================
// Runtime State Overrides
// =============================================================================

void veOverrideRasterState(VECommandBuffer* cmd, const VERasterConfig* raster) {
    if (!cmd || !raster) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    vkCmdSetCullMode(internal->commandBuffer, (VkCullModeFlags)raster->cullMode);
    vkCmdSetFrontFace(internal->commandBuffer, (VkFrontFace)raster->frontFace);
    vkCmdSetLineWidth(internal->commandBuffer, raster->lineWidth);
    
    if (raster->depthBiasEnable) {
        vkCmdSetDepthBias(internal->commandBuffer, 
                        raster->depthBiasConstantFactor,
                        raster->depthBiasClamp,
                        raster->depthBiasSlopeFactor);
    }
    
    // Extended dynamic state 3
    PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT)
        vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetPolygonModeEXT");
    if (vkCmdSetPolygonModeEXT) {
        vkCmdSetPolygonModeEXT(internal->commandBuffer, (VkPolygonMode)raster->polygonMode);
    }
}

void veOverrideDepthState(VECommandBuffer* cmd, const VEDepthConfig* depth) {
    if (!cmd || !depth) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    vkCmdSetDepthTestEnable(internal->commandBuffer, depth->depthTestEnable);
    vkCmdSetDepthWriteEnable(internal->commandBuffer, depth->depthWriteEnable);
    vkCmdSetDepthCompareOp(internal->commandBuffer, (VkCompareOp)depth->depthCompareOp);
    
    vkCmdSetStencilTestEnable(internal->commandBuffer, depth->stencilTestEnable);
    
    if (depth->stencilTestEnable) {
        vkCmdSetStencilCompareMask(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, depth->frontCompareMask);
        vkCmdSetStencilCompareMask(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, depth->backCompareMask);
        
        vkCmdSetStencilWriteMask(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, depth->frontWriteMask);
        vkCmdSetStencilWriteMask(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, depth->backWriteMask);
        
        vkCmdSetStencilReference(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, depth->frontReference);
        vkCmdSetStencilReference(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, depth->backReference);
    }
    
    if (depth->depthBoundsTestEnable) {
        vkCmdSetDepthBounds(internal->commandBuffer, depth->minDepthBounds, depth->maxDepthBounds);
    }
}

void veOverrideBlendState(VECommandBuffer* cmd, const VEBlendConfig* blend) {
    if (!cmd || !blend) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    vkCmdSetBlendConstants(internal->commandBuffer, blend->blendConstants);
    
    // Extended dynamic state 3
    PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)
        vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetColorBlendEnableEXT");
    
    if (vkCmdSetColorBlendEnableEXT && blend->attachmentCount > 0) {
        VkBool32 colorBlendEnables[8];
        for (uint32_t i = 0; i < blend->attachmentCount && i < 8; i++) {
            colorBlendEnables[i] = blend->attachments[i].blendEnable;
        }
        vkCmdSetColorBlendEnableEXT(internal->commandBuffer, 0, blend->attachmentCount, colorBlendEnables);
    }
}

// =============================================================================
// Quick State Toggles
// =============================================================================

void veSetWireframe(VECommandBuffer* cmd, bool enabled) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT)
        vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetPolygonModeEXT");
    
    if (vkCmdSetPolygonModeEXT) {
        VkPolygonMode mode = enabled ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        vkCmdSetPolygonModeEXT(internal->commandBuffer, mode);
    }
}

void veSetAlphaBlending(VECommandBuffer* cmd, bool enabled) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)
        vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetColorBlendEnableEXT");
    
    if (vkCmdSetColorBlendEnableEXT) {
        VkBool32 blendEnable = enabled ? VK_TRUE : VK_FALSE;
        vkCmdSetColorBlendEnableEXT(internal->commandBuffer, 0, 1, &blendEnable);
    }
}

void veSetDepthTesting(VECommandBuffer* cmd, bool testEnabled, bool writeEnabled) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    vkCmdSetDepthTestEnable(internal->commandBuffer, testEnabled);
    vkCmdSetDepthWriteEnable(internal->commandBuffer, writeEnabled);
}

void veSetCulling(VECommandBuffer* cmd, VECullMode cullMode) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    vkCmdSetCullMode(internal->commandBuffer, (VkCullModeFlags)cullMode);
}

// =============================================================================
// Push Constants
// =============================================================================

void vePushConstants(VECommandBuffer* cmd, const void* data, size_t size, size_t offset) {
    if (!cmd || !data || size == 0) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    // TODO: Need pipeline layout for push constants
    // For now, we can't implement this without knowing the pipeline layout
    // This would need to be provided by the render config or shader config
}

// =============================================================================
// Drawing Commands
// =============================================================================

void veDraw(VECommandBuffer* cmd, uint32_t vertexCount, uint32_t instanceCount,
           uint32_t firstVertex, uint32_t firstInstance) {
    if (!cmd || vertexCount == 0) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    vkCmdDraw(internal->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void veDrawIndexed(VECommandBuffer* cmd, uint32_t indexCount, uint32_t instanceCount,
                  uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    if (!cmd || indexCount == 0) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    vkCmdDrawIndexed(internal->commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void veDrawIndirect(VECommandBuffer* cmd, VEBufferAddress indirectBuffer,
                   size_t offset, uint32_t drawCount, uint32_t stride) {
    if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0) return;
    
    // TODO: Convert buffer address to VkBuffer
    // This requires access to the device to look up the buffer
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    // vkCmdDrawIndirect(internal->commandBuffer, buffer, offset, drawCount, stride);
}

void veDrawIndexedIndirect(VECommandBuffer* cmd, VEBufferAddress indirectBuffer,
                          size_t offset, uint32_t drawCount, uint32_t stride) {
    if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0) return;
    
    // TODO: Convert buffer address to VkBuffer
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    // vkCmdDrawIndexedIndirect(internal->commandBuffer, buffer, offset, drawCount, stride);
}

void veDrawIndirectCount(VECommandBuffer* cmd, VEBufferAddress indirectBuffer,
                        size_t indirectOffset, VEBufferAddress countBuffer,
                        size_t countOffset, uint32_t maxDrawCount, uint32_t stride) {
    if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS) return;
    
    // TODO: Convert buffer addresses to VkBuffer
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    // vkCmdDrawIndirectCount(internal->commandBuffer, indirectBuf, indirectOffset, 
    //                       countBuf, countOffset, maxDrawCount, stride);
}

void veDrawIndexedIndirectCount(VECommandBuffer* cmd, VEBufferAddress indirectBuffer,
                               size_t indirectOffset, VEBufferAddress countBuffer,
                               size_t countOffset, uint32_t maxDrawCount, uint32_t stride) {
    if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS) return;
    
    // TODO: Convert buffer addresses to VkBuffer
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    // vkCmdDrawIndexedIndirectCount(internal->commandBuffer, indirectBuf, indirectOffset,
    //                              countBuf, countOffset, maxDrawCount, stride);
}


// =============================================================================
// Synchronization and Memory Barriers
// =============================================================================

void veBarrierVertexToFragment(VECommandBuffer* cmd) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    VkMemoryBarrier2 memoryBarrier = {0};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    
    VkDependencyInfo dependencyInfo = {0};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;
    
    vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierComputeToVertex(VECommandBuffer* cmd) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    VkMemoryBarrier2 memoryBarrier = {0};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    
    VkDependencyInfo dependencyInfo = {0};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;
    
    vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierComputeToCompute(VECommandBuffer* cmd) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    VkMemoryBarrier2 memoryBarrier = {0};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    
    VkDependencyInfo dependencyInfo = {0};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;
    
    vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veBarrierGraphicsToPresent(VECommandBuffer* cmd) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    VkMemoryBarrier2 memoryBarrier = {0};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    
    VkDependencyInfo dependencyInfo = {0};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;
    
    vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
}

void veTransitionTexture(VECommandBuffer* cmd, VETextureIndex texture,
                        uint32_t oldLayout, uint32_t newLayout) {
    if (!cmd || texture == VE_INVALID_TEXTURE_INDEX) return;
    
    // TODO: Look up texture from index and perform layout transition
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    // This would require:
    // 1. Getting device from command buffer
    // 2. Looking up texture from index
    // 3. Creating image memory barrier
    // 4. Calling vkCmdPipelineBarrier2
}