/**
 * @file ve_command.c
 * @brief Command Buffer and Rendering Implementation
 */

#include "ve_internal.h"

// =============================================================================
// Command Buffer Management
// =============================================================================

VECommandBufferInternal* veGetCommandBufferInternal(VECommandBuffer* cmd) {
    return (VECommandBufferInternal*)cmd;
}

VEResult veAllocateCommandBuffer(VEDeviceInternal* device, VECommandBufferInternal** outCmd) {
    // Find free command buffer
    for (uint32_t i = 0; i < VE_MAX_COMMAND_BUFFERS; i++) {
        if (!device->commandBufferInUse[i]) {
            if (!device->commandBuffers[i].commandBuffer) {
                // Allocate new command buffer
                VkCommandBufferAllocateInfo allocInfo = {0};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = device->commandPool;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;
                
                VkResult result = vkAllocateCommandBuffers(device->device, &allocInfo, 
                                                         &device->commandBuffers[i].commandBuffer);
                if (result != VK_SUCCESS) {
                    veSetError("Failed to allocate command buffer (VkResult: %d)", result);
                    return VE_ERROR_OUT_OF_MEMORY;
                }
                
                device->commandBuffers[i].commandPool = device->commandPool;
                device->commandBuffers[i].index = i;
            }
            
            device->commandBufferInUse[i] = true;
            device->commandBuffers[i].device = device;  // Store device reference
            device->commandBuffers[i].isRecording = false;
            device->commandBuffers[i].isOneTime = false;
            
            *outCmd = &device->commandBuffers[i];
            return VE_SUCCESS;
        }
    }
    
    veSetError("No free command buffers available");
    return VE_ERROR_OUT_OF_MEMORY;
}

void veFreeCommandBuffer(VEDeviceInternal* device, VECommandBufferInternal* cmd) {
    if (!cmd || cmd->index >= VE_MAX_COMMAND_BUFFERS) return;
    
    device->commandBufferInUse[cmd->index] = false;
    cmd->isRecording = false;
    cmd->isOneTime = false;
}

// =============================================================================
// Command Buffer Operations
// =============================================================================

VECommandBuffer* veBeginCommandBuffer(VEDevice* device) {
    if (!device) {
        veSetError("Device cannot be NULL");
        return NULL;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    VECommandBufferInternal* cmd;
    VEResult result = veAllocateCommandBuffer(deviceInternal, &cmd);
    if (result != VE_SUCCESS) {
        return NULL;
    }
    
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VkResult vkResult = vkBeginCommandBuffer(cmd->commandBuffer, &beginInfo);
    if (vkResult != VK_SUCCESS) {
        veSetError("Failed to begin command buffer (VkResult: %d)", vkResult);
        veFreeCommandBuffer(deviceInternal, cmd);
        return NULL;
    }
    
    cmd->isRecording = true;
    cmd->isOneTime = true;
    
    return (VECommandBuffer*)cmd;
}

VEResult veSubmitCommandBuffer(VECommandBuffer* cmd, bool waitForCompletion) {
    if (!cmd) {
        veSetError("Command buffer cannot be NULL");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    if (!internal->isRecording) {
        veSetError("Command buffer is not recording");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VkResult result = vkEndCommandBuffer(internal->commandBuffer);
    if (result != VK_SUCCESS) {
        veSetError("Failed to end command buffer (VkResult: %d)", result);
        return VE_ERROR_UNKNOWN;
    }
    
    internal->isRecording = false;
    
    // Submit command buffer using stored device reference
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &internal->commandBuffer;
    
    VkFence fence = VK_NULL_HANDLE;
    if (waitForCompletion) {
        // Create fence for synchronization
        VkFenceCreateInfo fenceInfo = {0};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        
        result = vkCreateFence(internal->device->device, &fenceInfo, NULL, &fence);
        if (result != VK_SUCCESS) {
            veSetError("Failed to create fence for command buffer submission (VkResult: %d)", result);
            return VE_ERROR_OUT_OF_MEMORY;
        }
    }
    
    result = vkQueueSubmit(internal->device->graphicsQueue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(internal->device->device, fence, NULL);
        }
        veSetError("Failed to submit command buffer (VkResult: %d)", result);
        return VE_ERROR_OUT_OF_MEMORY;
    }
    
    if (waitForCompletion) {
        // Wait for completion and cleanup fence
        result = vkWaitForFences(internal->device->device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(internal->device->device, fence, NULL);
        
        if (result != VK_SUCCESS) {
            veSetError("Failed to wait for command buffer completion (VkResult: %d)", result);
            return VE_ERROR_OUT_OF_MEMORY;
        }
    }
    
    // Free the command buffer for reuse
    veFreeCommandBuffer(internal->device, internal);
    
    return VE_SUCCESS;
}

// =============================================================================
// Dynamic Rendering
// =============================================================================

void veBeginRendering(VECommandBuffer* cmd, const VERenderingInfo* renderingInfo) {
    if (!cmd || !renderingInfo) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    
    // Convert to Vulkan rendering info
    VkRenderingInfo vkRenderingInfo = {0};
    vkRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    vkRenderingInfo.renderArea.offset.x = renderingInfo->renderAreaX;
    vkRenderingInfo.renderArea.offset.y = renderingInfo->renderAreaY;
    vkRenderingInfo.renderArea.extent.width = renderingInfo->renderAreaWidth;
    vkRenderingInfo.renderArea.extent.height = renderingInfo->renderAreaHeight;
    vkRenderingInfo.layerCount = 1;
    
    // Convert color attachments
    VkRenderingAttachmentInfo colorAttachments[8] = {0};
    for (uint32_t i = 0; i < renderingInfo->colorAttachmentCount && i < 8; i++) {
        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        // TODO: Get image view from texture index
        colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachments[i].loadOp = (VkAttachmentLoadOp)renderingInfo->colorAttachments[i].loadOp;
        colorAttachments[i].storeOp = (VkAttachmentStoreOp)renderingInfo->colorAttachments[i].storeOp;
        colorAttachments[i].clearValue.color.float32[0] = renderingInfo->colorAttachments[i].clearValue.r;
        colorAttachments[i].clearValue.color.float32[1] = renderingInfo->colorAttachments[i].clearValue.g;
        colorAttachments[i].clearValue.color.float32[2] = renderingInfo->colorAttachments[i].clearValue.b;
        colorAttachments[i].clearValue.color.float32[3] = renderingInfo->colorAttachments[i].clearValue.a;
    }
    
    vkRenderingInfo.colorAttachmentCount = renderingInfo->colorAttachmentCount;
    vkRenderingInfo.pColorAttachments = colorAttachments;
    
    // Convert depth attachment
    VkRenderingAttachmentInfo depthAttachment = {0};
    if (renderingInfo->depthAttachment) {
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        // TODO: Get image view from texture index
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = (VkAttachmentLoadOp)renderingInfo->depthAttachment->loadOp;
        depthAttachment.storeOp = (VkAttachmentStoreOp)renderingInfo->depthAttachment->storeOp;
        depthAttachment.clearValue.depthStencil.depth = 1.0f;
        depthAttachment.clearValue.depthStencil.stencil = 0;
        
        vkRenderingInfo.pDepthAttachment = &depthAttachment;
    }
    
    // Convert stencil attachment
    VkRenderingAttachmentInfo stencilAttachment = {0};
    if (renderingInfo->stencilAttachment) {
        stencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        // TODO: Get image view from texture index
        stencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        stencilAttachment.loadOp = (VkAttachmentLoadOp)renderingInfo->stencilAttachment->loadOp;
        stencilAttachment.storeOp = (VkAttachmentStoreOp)renderingInfo->stencilAttachment->storeOp;
        stencilAttachment.clearValue.depthStencil.depth = 1.0f;
        stencilAttachment.clearValue.depthStencil.stencil = 0;
        
        vkRenderingInfo.pStencilAttachment = &stencilAttachment;
    }
    
    vkCmdBeginRendering(internal->commandBuffer, &vkRenderingInfo);
}

void veEndRendering(VECommandBuffer* cmd) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    vkCmdEndRendering(internal->commandBuffer);
}

// =============================================================================
// Render Configuration Application
// =============================================================================

void veApplyRenderConfig(VECommandBuffer* cmd, VERenderConfig* config) {
    if (!cmd || !config) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    VERenderConfigInternal* configInternal = (VERenderConfigInternal*)config;
    
    // Apply rasterization state
    if (configInternal->configTypes & VE_CONFIG_TYPE_RASTERIZATION) {
        vkCmdSetCullMode(internal->commandBuffer, (VkCullModeFlags)configInternal->rasterConfig.cullMode);
        vkCmdSetFrontFace(internal->commandBuffer, (VkFrontFace)configInternal->rasterConfig.frontFace);
        
        // Extended dynamic state 3
        PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT = (PFN_vkCmdSetPolygonModeEXT)
            vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetPolygonModeEXT");
        if (vkCmdSetPolygonModeEXT) {
            vkCmdSetPolygonModeEXT(internal->commandBuffer, (VkPolygonMode)configInternal->rasterConfig.polygonMode);
        }
        
        vkCmdSetLineWidth(internal->commandBuffer, configInternal->rasterConfig.lineWidth);
        
        if (configInternal->rasterConfig.depthBiasEnable) {
            vkCmdSetDepthBias(internal->commandBuffer, 
                            configInternal->rasterConfig.depthBiasConstantFactor,
                            configInternal->rasterConfig.depthBiasClamp,
                            configInternal->rasterConfig.depthBiasSlopeFactor);
        }
    }
    
    // Apply depth/stencil state
    if (configInternal->configTypes & VE_CONFIG_TYPE_DEPTH_STENCIL) {
        vkCmdSetDepthTestEnable(internal->commandBuffer, configInternal->depthConfig.depthTestEnable);
        vkCmdSetDepthWriteEnable(internal->commandBuffer, configInternal->depthConfig.depthWriteEnable);
        vkCmdSetDepthCompareOp(internal->commandBuffer, (VkCompareOp)configInternal->depthConfig.depthCompareOp);
        
        if (configInternal->depthConfig.depthBoundsTestEnable) {
            vkCmdSetDepthBounds(internal->commandBuffer, 
                              configInternal->depthConfig.minDepthBounds,
                              configInternal->depthConfig.maxDepthBounds);
        }
        
        vkCmdSetStencilTestEnable(internal->commandBuffer, configInternal->depthConfig.stencilTestEnable);
        
        if (configInternal->depthConfig.stencilTestEnable) {
            vkCmdSetStencilCompareMask(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, 
                                     configInternal->depthConfig.frontCompareMask);
            vkCmdSetStencilCompareMask(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, 
                                     configInternal->depthConfig.backCompareMask);
            
            vkCmdSetStencilWriteMask(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, 
                                   configInternal->depthConfig.frontWriteMask);
            vkCmdSetStencilWriteMask(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, 
                                   configInternal->depthConfig.backWriteMask);
            
            vkCmdSetStencilReference(internal->commandBuffer, VK_STENCIL_FACE_FRONT_BIT, 
                                   configInternal->depthConfig.frontReference);
            vkCmdSetStencilReference(internal->commandBuffer, VK_STENCIL_FACE_BACK_BIT, 
                                   configInternal->depthConfig.backReference);
        }
    }
    
    // Apply color blend state  
    if (configInternal->configTypes & VE_CONFIG_TYPE_COLOR_BLEND) {
        vkCmdSetBlendConstants(internal->commandBuffer, configInternal->blendConfig.blendConstants);
        
        // Extended dynamic state 3 for per-attachment blending
        PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT = (PFN_vkCmdSetColorBlendEnableEXT)
            vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetColorBlendEnableEXT");
        
        if (vkCmdSetColorBlendEnableEXT && configInternal->blendConfig.attachmentCount > 0) {
            VkBool32 colorBlendEnables[8];
            for (uint32_t i = 0; i < configInternal->blendConfig.attachmentCount && i < 8; i++) {
                colorBlendEnables[i] = configInternal->blendConfig.attachments[i].blendEnable;
            }
            vkCmdSetColorBlendEnableEXT(internal->commandBuffer, 0, 
                                      configInternal->blendConfig.attachmentCount, colorBlendEnables);
        }
    }
    
    // Bind shaders
    if (configInternal->shaderCount > 0) {
        VkShaderStageFlagBits stages[6];
        VkShaderEXT shaders[6];
        uint32_t validShaderCount = 0;
        
        for (uint32_t i = 0; i < configInternal->shaderCount; i++) {
            if (configInternal->shaders[i]) {
                VEShaderInternal* shaderInternal = (VEShaderInternal*)configInternal->shaders[i];
                stages[validShaderCount] = veShaderStageToVk(shaderInternal->stage);
                shaders[validShaderCount] = shaderInternal->shaderObject;
                validShaderCount++;
            }
        }
        
        if (validShaderCount > 0) {
            PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT = (PFN_vkCmdBindShadersEXT)
                vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdBindShadersEXT");
            
            if (vkCmdBindShadersEXT) {
                vkCmdBindShadersEXT(internal->commandBuffer, validShaderCount, stages, shaders);
            }
        }
    }
}

void veApplyVertexConfig(VECommandBuffer* cmd, VEVertexConfig* config) {
    if (!cmd || !config) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    VEVertexConfigInternal* configInternal = (VEVertexConfigInternal*)config;
    
    // Set primitive topology
    vkCmdSetPrimitiveTopology(internal->commandBuffer, (VkPrimitiveTopology)configInternal->topology);
    vkCmdSetPrimitiveRestartEnable(internal->commandBuffer, configInternal->primitiveRestartEnable);
    
    // Set vertex input dynamic state
    PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT = (PFN_vkCmdSetVertexInputEXT)
        vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdSetVertexInputEXT");
    
    if (vkCmdSetVertexInputEXT && configInternal->bindingCount > 0) {
        VkVertexInputBindingDescription2EXT bindings[16];
        VkVertexInputAttributeDescription2EXT attributes[32];
        
        for (uint32_t i = 0; i < configInternal->bindingCount; i++) {
            bindings[i].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
            bindings[i].pNext = NULL;
            bindings[i].binding = configInternal->bindings[i].binding;
            bindings[i].stride = configInternal->bindings[i].stride;
            bindings[i].inputRate = (VkVertexInputRate)configInternal->bindings[i].inputRate;
            bindings[i].divisor = configInternal->bindings[i].divisor;
        }
        
        for (uint32_t i = 0; i < configInternal->attributeCount; i++) {
            attributes[i].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
            attributes[i].pNext = NULL;
            attributes[i].location = configInternal->attributes[i].location;
            attributes[i].binding = configInternal->attributes[i].binding;
            attributes[i].format = veFormatToVk(configInternal->attributes[i].format);
            attributes[i].offset = configInternal->attributes[i].offset;
        }
        
        vkCmdSetVertexInputEXT(internal->commandBuffer, 
                             configInternal->bindingCount, bindings,
                             configInternal->attributeCount, attributes);
    }
}

// =============================================================================
// Shader Binding
// =============================================================================

void veBindShader(VECommandBuffer* cmd, VEShader* shader) {
    if (!cmd || !shader) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    VEShaderInternal* shaderInternal = (VEShaderInternal*)shader;
    
    VkShaderStageFlagBits stage = veShaderStageToVk(shaderInternal->stage);
    
    PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT = (PFN_vkCmdBindShadersEXT)
        vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdBindShadersEXT");
    
    if (vkCmdBindShadersEXT) {
        vkCmdBindShadersEXT(internal->commandBuffer, 1, &stage, &shaderInternal->shaderObject);
    }
}

void veBindShaders(VECommandBuffer* cmd, uint32_t shaderCount, VEShader* const* shaders) {
    if (!cmd || shaderCount == 0 || !shaders) return;
    
    VkShaderStageFlagBits stages[16];
    VkShaderEXT shaderObjects[16];
    uint32_t validCount = 0;
    
    for (uint32_t i = 0; i < shaderCount && i < 16; i++) {
        if (shaders[i]) {
            VEShaderInternal* shaderInternal = (VEShaderInternal*)shaders[i];
            stages[validCount] = veShaderStageToVk(shaderInternal->stage);
            shaderObjects[validCount] = shaderInternal->shaderObject;
            validCount++;
        }
    }
    
    if (validCount > 0) {
        VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
        
        PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT = (PFN_vkCmdBindShadersEXT)
            vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdBindShadersEXT");
        
        if (vkCmdBindShadersEXT) {
            vkCmdBindShadersEXT(internal->commandBuffer, validCount, stages, shaderObjects);
        }
    }
}

void veBindShaderConfig(VECommandBuffer* cmd, VEShaderConfig* config) {
    if (!cmd || !config) return;
    
    VEShaderConfigInternal* configInternal = (VEShaderConfigInternal*)config;
    
    VEShader* shaders[6];
    uint32_t shaderCount = 0;
    
    if (configInternal->vertexShader) shaders[shaderCount++] = configInternal->vertexShader;
    if (configInternal->fragmentShader) shaders[shaderCount++] = configInternal->fragmentShader;
    if (configInternal->geometryShader) shaders[shaderCount++] = configInternal->geometryShader;
    if (configInternal->tessControlShader) shaders[shaderCount++] = configInternal->tessControlShader;
    if (configInternal->tessEvalShader) shaders[shaderCount++] = configInternal->tessEvalShader;
    if (configInternal->computeShader) shaders[shaderCount++] = configInternal->computeShader;
    
    if (shaderCount > 0) {
        veBindShaders(cmd, shaderCount, shaders);
    }
}

void veUnbindShaderStage(VECommandBuffer* cmd, VEShaderStage stage) {
    if (!cmd) return;
    
    VECommandBufferInternal* internal = (VECommandBufferInternal*)cmd;
    VkShaderStageFlagBits vkStage = veShaderStageToVk(stage);
    VkShaderEXT nullShader = VK_NULL_HANDLE;
    
    PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT = (PFN_vkCmdBindShadersEXT)
        vkGetDeviceProcAddr(VK_NULL_HANDLE, "vkCmdBindShadersEXT");
    
    if (vkCmdBindShadersEXT) {
        vkCmdBindShadersEXT(internal->commandBuffer, 1, &vkStage, &nullShader);
    }
}