/**
 * @file ve_draw.c
 * @brief Drawing and State Management Implementation
 */

#include "ve_internal.h"

// =============================================================================
// Viewport and Scissor Management
// =============================================================================

VEResult veSetViewport(VECommandBuffer *cmd, float x, float y, float width, float height, float minDepth,
                       float maxDepth)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkViewport viewport{};
   viewport.x = x;
   viewport.y = y;
   viewport.width = width;
   viewport.height = height;
   viewport.minDepth = minDepth;
   viewport.maxDepth = maxDepth;

   vkCmdSetViewportWithCount(internal->commandBuffer, 1, &viewport);
   return VE_SUCCESS;
}

VEResult veSetScissor(VECommandBuffer *cmd, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   VkRect2D scissor{};
   scissor.offset.x = x;
   scissor.offset.y = y;
   scissor.extent.width = width;
   scissor.extent.height = height;

   vkCmdSetScissorWithCount(internal->commandBuffer, 1, &scissor);
   return VE_SUCCESS;
}

// =============================================================================
// Graphics State Application
// =============================================================================

VEResult veApplyGraphicsState(VECommandBuffer *cmd, VEGraphicsPipeline *pipeline, VEDrawState *drawState,
                              const VEViewport *viewport, const VERect2D *scissor)
{
   if (!cmd)
   {
      veSetError("veApplyGraphicsState: Invalid command buffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // Validate we're inside a render pass (required for default viewport/scissor)
   if (!internal->inRenderPass && (viewport == NULL || scissor == NULL))
   {
      veSetError("veApplyGraphicsState: must be called inside veBeginRendering when viewport or scissor is NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // Bind graphics pipeline if provided
   if (pipeline)
   {
      VEResult r = veBindGraphicsPipeline(cmd, pipeline);
      if (r != VE_SUCCESS)
         return r;
   }

   // Apply draw state (always call - applies defaults if NULL, which is required for shader objects)
   {
      VEResult r = veApplyDrawState(cmd, drawState);
      if (r != VE_SUCCESS)
         return r;
   }

   // Set viewport (use render area if NULL)
   if (viewport)
   {
      VEResult r = veSetViewport(cmd, viewport->x, viewport->y, viewport->width, viewport->height, viewport->minDepth,
                                 viewport->maxDepth);
      if (r != VE_SUCCESS)
         return r;
   }
   else
   {
      VEResult r = veSetViewport(
          cmd, static_cast<float>(internal->renderAreaX), static_cast<float>(internal->renderAreaY),
          static_cast<float>(internal->renderAreaWidth), static_cast<float>(internal->renderAreaHeight), 0.0f, 1.0f);
      if (r != VE_SUCCESS)
         return r;
   }

   // Set scissor (use render area if NULL)
   if (scissor)
   {
      VEResult r = veSetScissor(cmd, scissor->x, scissor->y, scissor->width, scissor->height);
      if (r != VE_SUCCESS)
         return r;
   }
   else
   {
      VEResult r =
          veSetScissor(cmd, static_cast<int32_t>(internal->renderAreaX), static_cast<int32_t>(internal->renderAreaY),
                       internal->renderAreaWidth, internal->renderAreaHeight);
      if (r != VE_SUCCESS)
         return r;
   }

   return VE_SUCCESS;
}

// =============================================================================
// Graphics Pipeline Binding
// =============================================================================

VEResult veBindGraphicsPipeline(VECommandBuffer *cmd, VEGraphicsPipeline *pipeline)
{
   if (!cmd || !pipeline)
   {
      veSetError("veBindGraphicsPipeline: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEGraphicsPipelineInternal *pipelineInternal = (VEGraphicsPipelineInternal *)pipeline;

   if (!pipelineInternal->isValid)
   {
      veSetError("veBindGraphicsPipeline: Pipeline is not valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (!internal->isRecording)
   {
      veSetError("veBindGraphicsPipeline: Command buffer is not recording");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkCommandBuffer vkCmd = internal->commandBuffer;

   // Determine if this is a mesh shader pipeline
   bool isMeshPipeline = (pipelineInternal->meshShader != nullptr);

   // Track bound shader stages
   internal->boundShaders = 0;

   if (isMeshPipeline)
   {
      // Mesh shader pipeline: bind task (optional) + mesh + fragment shaders
      // Must unbind vertex/geometry/tessellation stages
      VkShaderEXT shaders[7] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
      VkShaderStageFlagBits stages[7] = {VK_SHADER_STAGE_VERTEX_BIT,
                                         VK_SHADER_STAGE_GEOMETRY_BIT,
                                         VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                         VK_SHADER_STAGE_TASK_BIT_EXT,
                                         VK_SHADER_STAGE_MESH_BIT_EXT,
                                         VK_SHADER_STAGE_FRAGMENT_BIT};

      // Unbind traditional vertex pipeline stages (indices 0-3 are null)

      // Bind task shader if present
      if (pipelineInternal->taskShader)
      {
         shaders[4] = ((VEShaderInternal *)pipelineInternal->taskShader)->shaderObject;
         internal->boundShaders |= VK_SHADER_STAGE_TASK_BIT_EXT;
      }

      // Bind mesh shader (required for mesh pipeline)
      shaders[5] = ((VEShaderInternal *)pipelineInternal->meshShader)->shaderObject;
      internal->boundShaders |= VK_SHADER_STAGE_MESH_BIT_EXT;

      // Bind fragment shader
      if (pipelineInternal->fragmentShader)
      {
         shaders[6] = ((VEShaderInternal *)pipelineInternal->fragmentShader)->shaderObject;
         internal->boundShaders |= VK_SHADER_STAGE_FRAGMENT_BIT;
      }

      veFuncs.vkCmdBindShadersEXT(vkCmd, 7, stages, shaders);
   }
   else
   {
      // Traditional vertex pipeline: bind vertex/fragment/geometry/tessellation shaders
      // Must unbind mesh/task stages if previously bound
      VkShaderEXT shaders[7] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                                VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
      VkShaderStageFlagBits stages[7] = {VK_SHADER_STAGE_VERTEX_BIT,
                                         VK_SHADER_STAGE_FRAGMENT_BIT,
                                         VK_SHADER_STAGE_GEOMETRY_BIT,
                                         VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                         VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                         VK_SHADER_STAGE_TASK_BIT_EXT,
                                         VK_SHADER_STAGE_MESH_BIT_EXT};

      if (pipelineInternal->vertexShader)
      {
         shaders[0] = ((VEShaderInternal *)pipelineInternal->vertexShader)->shaderObject;
         internal->boundShaders |= VK_SHADER_STAGE_VERTEX_BIT;
      }
      if (pipelineInternal->fragmentShader)
      {
         shaders[1] = ((VEShaderInternal *)pipelineInternal->fragmentShader)->shaderObject;
         internal->boundShaders |= VK_SHADER_STAGE_FRAGMENT_BIT;
      }
      if (pipelineInternal->geometryShader)
      {
         shaders[2] = ((VEShaderInternal *)pipelineInternal->geometryShader)->shaderObject;
         internal->boundShaders |= VK_SHADER_STAGE_GEOMETRY_BIT;
      }
      if (pipelineInternal->tessControlShader)
      {
         shaders[3] = ((VEShaderInternal *)pipelineInternal->tessControlShader)->shaderObject;
         internal->boundShaders |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
      }
      if (pipelineInternal->tessEvalShader)
      {
         shaders[4] = ((VEShaderInternal *)pipelineInternal->tessEvalShader)->shaderObject;
         internal->boundShaders |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
      }

      // Indices 5-6 are null to unbind any previously bound mesh/task shaders
      veFuncs.vkCmdBindShadersEXT(vkCmd, 7, stages, shaders);
   }

   // Set vertex input (always call, even with zero bindings/attributes for shader objects)
   {
      VkVertexInputBindingDescription2EXT bindings[VE_MAX_VERTEX_BINDINGS];
      VkVertexInputAttributeDescription2EXT attrs[VE_MAX_VERTEX_ATTRIBUTES];

      for (uint32_t i = 0; i < pipelineInternal->vertexBindingCount; i++)
      {
         bindings[i] = {};
         bindings[i].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
         bindings[i].binding = pipelineInternal->vertexBindings[i].binding;
         bindings[i].stride = pipelineInternal->vertexBindings[i].stride;
         bindings[i].inputRate = pipelineInternal->vertexBindings[i].inputRate;
         bindings[i].divisor =
             pipelineInternal->vertexBindings[i].divisor > 0 ? pipelineInternal->vertexBindings[i].divisor : 1;
      }

      for (uint32_t i = 0; i < pipelineInternal->vertexAttributeCount; i++)
      {
         attrs[i] = {};
         attrs[i].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
         attrs[i].location = pipelineInternal->vertexAttributes[i].location;
         attrs[i].binding = pipelineInternal->vertexAttributes[i].binding;
         attrs[i].format = pipelineInternal->vertexAttributes[i].format;
         attrs[i].offset = pipelineInternal->vertexAttributes[i].offset;
      }

      veFuncs.vkCmdSetVertexInputEXT(vkCmd, pipelineInternal->vertexBindingCount, bindings,
                                     pipelineInternal->vertexAttributeCount, attrs);
   }

   // Set depth state
   vkCmdSetDepthTestEnable(vkCmd, pipelineInternal->depthTestEnable ? VK_TRUE : VK_FALSE);
   vkCmdSetDepthWriteEnable(vkCmd, pipelineInternal->depthWriteEnable ? VK_TRUE : VK_FALSE);
   vkCmdSetDepthCompareOp(vkCmd, pipelineInternal->depthCompareOp);
   vkCmdSetDepthBoundsTestEnable(vkCmd, pipelineInternal->depthBoundsTestEnable ? VK_TRUE : VK_FALSE);
   if (pipelineInternal->depthBoundsTestEnable)
   {
      vkCmdSetDepthBounds(vkCmd, pipelineInternal->minDepthBounds, pipelineInternal->maxDepthBounds);
   }

   // Set stencil state
   vkCmdSetStencilTestEnable(vkCmd, pipelineInternal->stencilTestEnable ? VK_TRUE : VK_FALSE);
   if (pipelineInternal->stencilTestEnable)
   {
      vkCmdSetStencilOp(vkCmd, VK_STENCIL_FACE_FRONT_BIT, pipelineInternal->frontStencil.failOp,
                        pipelineInternal->frontStencil.passOp, pipelineInternal->frontStencil.depthFailOp,
                        pipelineInternal->frontStencil.compareOp);
      vkCmdSetStencilCompareMask(vkCmd, VK_STENCIL_FACE_FRONT_BIT, pipelineInternal->frontStencil.compareMask);
      vkCmdSetStencilWriteMask(vkCmd, VK_STENCIL_FACE_FRONT_BIT, pipelineInternal->frontStencil.writeMask);
      vkCmdSetStencilReference(vkCmd, VK_STENCIL_FACE_FRONT_BIT, pipelineInternal->frontStencil.reference);

      vkCmdSetStencilOp(vkCmd, VK_STENCIL_FACE_BACK_BIT, pipelineInternal->backStencil.failOp,
                        pipelineInternal->backStencil.passOp, pipelineInternal->backStencil.depthFailOp,
                        pipelineInternal->backStencil.compareOp);
      vkCmdSetStencilCompareMask(vkCmd, VK_STENCIL_FACE_BACK_BIT, pipelineInternal->backStencil.compareMask);
      vkCmdSetStencilWriteMask(vkCmd, VK_STENCIL_FACE_BACK_BIT, pipelineInternal->backStencil.writeMask);
      vkCmdSetStencilReference(vkCmd, VK_STENCIL_FACE_BACK_BIT, pipelineInternal->backStencil.reference);
   }

   // Set blend state
   veFuncs.vkCmdSetLogicOpEnableEXT(vkCmd, pipelineInternal->logicOpEnable ? VK_TRUE : VK_FALSE);
   if (pipelineInternal->logicOpEnable)
   {
      veFuncs.vkCmdSetLogicOpEXT(vkCmd, pipelineInternal->logicOp);
   }

   if (pipelineInternal->blendAttachmentCount > 0)
   {
      VkBool32 colorBlendEnables[VE_MAX_COLOR_ATTACHMENTS];
      VkColorBlendEquationEXT equations[VE_MAX_COLOR_ATTACHMENTS];
      VkColorComponentFlags writeMasks[VE_MAX_COLOR_ATTACHMENTS];

      for (uint32_t i = 0; i < pipelineInternal->blendAttachmentCount; i++)
      {
         colorBlendEnables[i] = pipelineInternal->blendAttachments[i].blendEnable ? VK_TRUE : VK_FALSE;

         equations[i] = {};
         equations[i].srcColorBlendFactor = pipelineInternal->blendAttachments[i].srcColorBlendFactor;
         equations[i].dstColorBlendFactor = pipelineInternal->blendAttachments[i].dstColorBlendFactor;
         equations[i].colorBlendOp = pipelineInternal->blendAttachments[i].colorBlendOp;
         equations[i].srcAlphaBlendFactor = pipelineInternal->blendAttachments[i].srcAlphaBlendFactor;
         equations[i].dstAlphaBlendFactor = pipelineInternal->blendAttachments[i].dstAlphaBlendFactor;
         equations[i].alphaBlendOp = pipelineInternal->blendAttachments[i].alphaBlendOp;

         writeMasks[i] = pipelineInternal->blendAttachments[i].colorWriteMask;
      }

      veFuncs.vkCmdSetColorBlendEnableEXT(vkCmd, 0, pipelineInternal->blendAttachmentCount, colorBlendEnables);
      veFuncs.vkCmdSetColorBlendEquationEXT(vkCmd, 0, pipelineInternal->blendAttachmentCount, equations);
      veFuncs.vkCmdSetColorWriteMaskEXT(vkCmd, 0, pipelineInternal->blendAttachmentCount, writeMasks);
   }
   vkCmdSetBlendConstants(vkCmd, pipelineInternal->blendConstants);

   // Set rasterization defaults
   vkCmdSetCullMode(vkCmd, pipelineInternal->cullMode);
   vkCmdSetFrontFace(vkCmd, pipelineInternal->frontFace);

   // Set multisampling state (required for shader objects)
   veFuncs.vkCmdSetRasterizationSamplesEXT(vkCmd, VK_SAMPLE_COUNT_1_BIT);
   VkSampleMask sampleMask = 0xFFFFFFFF;
   veFuncs.vkCmdSetSampleMaskEXT(vkCmd, VK_SAMPLE_COUNT_1_BIT, &sampleMask);

   return VE_SUCCESS;
}

VEResult veApplyDrawState(VECommandBuffer *cmd, VEDrawState *drawState)
{
   if (!cmd)
   {
      veSetError("veApplyDrawState: Invalid command buffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkCommandBuffer vkCmd = internal->commandBuffer;

   // If drawState is NULL, apply defaults
   VEDrawStateDesc defaults;
   VEDrawStateInternal *stateInternal;

   if (drawState)
   {
      stateInternal = (VEDrawStateInternal *)drawState;
      if (!stateInternal->isValid)
      {
         veSetError("veApplyDrawState: Draw state is not valid");
         return VE_ERROR_INVALID_PARAMETER;
      }
   }
   else
   {
      // Create a temporary internal with defaults
      defaults = veDefaultDrawStateDesc();
      stateInternal = (VEDrawStateInternal *)&defaults;
   }

   // Set topology
   vkCmdSetPrimitiveTopology(vkCmd, stateInternal->topology);
   internal->currentTopology = stateInternal->topology;

   // Set primitive restart
   vkCmdSetPrimitiveRestartEnable(vkCmd, stateInternal->primitiveRestartEnable ? VK_TRUE : VK_FALSE);

   // Set patch control points (for tessellation)
   if (stateInternal->patchControlPoints > 0)
   {
      veFuncs.vkCmdSetPatchControlPointsEXT(vkCmd, stateInternal->patchControlPoints);
      internal->currentPatchControlPoints = stateInternal->patchControlPoints;
   }

   // Set polygon mode
   veFuncs.vkCmdSetPolygonModeEXT(vkCmd, stateInternal->polygonMode);

   // Set line width
   vkCmdSetLineWidth(vkCmd, stateInternal->lineWidth);

   // Set cull mode (only if not using pipeline default)
   if (stateInternal->cullMode != VE_CULL_MODE_USE_PIPELINE)
   {
      vkCmdSetCullMode(vkCmd, stateInternal->cullMode);
   }

   // Set front face
   vkCmdSetFrontFace(vkCmd, stateInternal->frontFace);

   // Set rasterizer discard
   vkCmdSetRasterizerDiscardEnable(vkCmd, stateInternal->rasterizerDiscardEnable ? VK_TRUE : VK_FALSE);

   // Set depth bias
   vkCmdSetDepthBiasEnable(vkCmd, stateInternal->depthBiasEnable ? VK_TRUE : VK_FALSE);
   if (stateInternal->depthBiasEnable)
   {
      vkCmdSetDepthBias(vkCmd, stateInternal->depthBiasConstantFactor, stateInternal->depthBiasClamp,
                        stateInternal->depthBiasSlopeFactor);
   }

   // Set depth clamp
   veFuncs.vkCmdSetDepthClampEnableEXT(vkCmd, stateInternal->depthClampEnable ? VK_TRUE : VK_FALSE);

   // Set alpha to coverage
   veFuncs.vkCmdSetAlphaToCoverageEnableEXT(vkCmd, stateInternal->alphaToCoverageEnable ? VK_TRUE : VK_FALSE);

   // Set alpha to one
   veFuncs.vkCmdSetAlphaToOneEnableEXT(vkCmd, stateInternal->alphaToOneEnable ? VK_TRUE : VK_FALSE);

   return VE_SUCCESS;
}

// =============================================================================
// Individual Dynamic State Setters
// =============================================================================

VEResult veSetTopology(VECommandBuffer *cmd, VkPrimitiveTopology topology)
{
   if (!cmd)
   {
      veSetError("veSetTopology: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetPrimitiveTopology(internal->commandBuffer, topology);
   internal->currentTopology = topology;
   return VE_SUCCESS;
}

VEResult veSetPrimitiveRestart(VECommandBuffer *cmd, bool enable)
{
   if (!cmd)
   {
      veSetError("veSetPrimitiveRestart: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetPrimitiveRestartEnable(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   return VE_SUCCESS;
}

VEResult veSetPatchControlPoints(VECommandBuffer *cmd, uint32_t controlPoints)
{
   if (!cmd)
   {
      veSetError("veSetPatchControlPoints: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   veFuncs.vkCmdSetPatchControlPointsEXT(internal->commandBuffer, controlPoints);
   internal->currentPatchControlPoints = controlPoints;
   return VE_SUCCESS;
}

VEResult veSetPolygonMode(VECommandBuffer *cmd, VkPolygonMode mode)
{
   if (!cmd)
   {
      veSetError("veSetPolygonMode: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   veFuncs.vkCmdSetPolygonModeEXT(internal->commandBuffer, mode);
   return VE_SUCCESS;
}

VEResult veSetLineWidth(VECommandBuffer *cmd, float width)
{
   if (!cmd)
   {
      veSetError("veSetLineWidth: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetLineWidth(internal->commandBuffer, width);
   return VE_SUCCESS;
}

VEResult veSetCullMode(VECommandBuffer *cmd, VkCullModeFlags cullMode)
{
   if (!cmd)
   {
      veSetError("veSetCullMode: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetCullMode(internal->commandBuffer, cullMode);
   return VE_SUCCESS;
}

VEResult veSetFrontFace(VECommandBuffer *cmd, VkFrontFace frontFace)
{
   if (!cmd)
   {
      veSetError("veSetFrontFace: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetFrontFace(internal->commandBuffer, frontFace);
   return VE_SUCCESS;
}

VEResult veSetRasterizerDiscard(VECommandBuffer *cmd, bool enable)
{
   if (!cmd)
   {
      veSetError("veSetRasterizerDiscard: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetRasterizerDiscardEnable(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   return VE_SUCCESS;
}

VEResult veSetDepthBias(VECommandBuffer *cmd, bool enable, float constantFactor, float clamp, float slopeFactor)
{
   if (!cmd)
   {
      veSetError("veSetDepthBias: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetDepthBiasEnable(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   if (enable)
   {
      vkCmdSetDepthBias(internal->commandBuffer, constantFactor, clamp, slopeFactor);
   }
   return VE_SUCCESS;
}

VEResult veSetDepthClamp(VECommandBuffer *cmd, bool enable)
{
   if (!cmd)
   {
      veSetError("veSetDepthClamp: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   veFuncs.vkCmdSetDepthClampEnableEXT(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   return VE_SUCCESS;
}

VEResult veSetAlphaToCoverage(VECommandBuffer *cmd, bool enable)
{
   if (!cmd)
   {
      veSetError("veSetAlphaToCoverage: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   veFuncs.vkCmdSetAlphaToCoverageEnableEXT(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   return VE_SUCCESS;
}

VEResult veSetAlphaToOne(VECommandBuffer *cmd, bool enable)
{
   if (!cmd)
   {
      veSetError("veSetAlphaToOne: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   veFuncs.vkCmdSetAlphaToOneEnableEXT(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   return VE_SUCCESS;
}

VEResult veSetDepthTest(VECommandBuffer *cmd, bool enable)
{
   if (!cmd)
   {
      veSetError("veSetDepthTest: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetDepthTestEnable(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   return VE_SUCCESS;
}

VEResult veSetDepthWrite(VECommandBuffer *cmd, bool enable)
{
   if (!cmd)
   {
      veSetError("veSetDepthWrite: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetDepthWriteEnable(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   return VE_SUCCESS;
}

VEResult veSetDepthCompareOp(VECommandBuffer *cmd, VkCompareOp op)
{
   if (!cmd)
   {
      veSetError("veSetDepthCompareOp: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetDepthCompareOp(internal->commandBuffer, op);
   return VE_SUCCESS;
}

VEResult veSetStencilTest(VECommandBuffer *cmd, bool enable)
{
   if (!cmd)
   {
      veSetError("veSetStencilTest: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetStencilTestEnable(internal->commandBuffer, enable ? VK_TRUE : VK_FALSE);
   return VE_SUCCESS;
}

VEResult veSetStencilOp(VECommandBuffer *cmd, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp,
                        VkStencilOp depthFailOp, VkCompareOp compareOp)
{
   if (!cmd)
   {
      veSetError("veSetStencilOp: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetStencilOp(internal->commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
   return VE_SUCCESS;
}

VEResult veSetStencilReference(VECommandBuffer *cmd, VkStencilFaceFlags faceMask, uint32_t reference)
{
   if (!cmd)
   {
      veSetError("veSetStencilReference: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   vkCmdSetStencilReference(internal->commandBuffer, faceMask, reference);
   return VE_SUCCESS;
}

// =============================================================================
// Convenience State Toggles
// =============================================================================

VEResult veSetWireframe(VECommandBuffer *cmd, bool enabled)
{
   return veSetPolygonMode(cmd, enabled ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL);
}

VEResult veSetDepthTesting(VECommandBuffer *cmd, bool testEnabled, bool writeEnabled)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   vkCmdSetDepthTestEnable(internal->commandBuffer, testEnabled);
   vkCmdSetDepthWriteEnable(internal->commandBuffer, writeEnabled);
   return VE_SUCCESS;
}

// =============================================================================
// Push Constants
// =============================================================================

// Helper function to determine which shader stages are bound
static VkShaderStageFlags veGetBoundShaderStages(VECommandBufferInternal *cmd)
{
   VkShaderStageFlags stages = 0;

   if (cmd->boundShaders & VK_SHADER_STAGE_COMPUTE_BIT)
      stages = VK_SHADER_STAGE_COMPUTE_BIT;

   // Check which shader objects are currently bound
   // This would be tracked when veBindShaderConfig is called
   if (cmd->boundShaders & VK_SHADER_STAGE_ALL_GRAPHICS)
      stages = VK_SHADER_STAGE_ALL_GRAPHICS;

   return stages;
}

VEResult vePushConstants(VECommandBuffer *cmd, const void *data, uint64_t size, uint64_t offset)
{
   if (!cmd || !data || size == 0)
   {
      veSetError("Invalid parameters for vePushConstants");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   // Alignment and bounds checks required by spec
   if ((offset & 3u) || (size & 3u))
   {
      veSetError("Push constants offset/size must be multiples of 4 (offset=%zu "
                 "size=%zu)",
                 offset, size);
      return VE_ERROR_INVALID_PARAMETER; // 4-byte alignment is required
   }

   // Simple bounds check for VulkEase's standard push constant limit
   if (offset + size > VE_MAX_PUSH_CONSTANT_BYTES)
   { // Standard guaranteed minimum
      veSetError("Push constant size exceeds limit (max 256 bytes, requested %zu+%zu)", offset, size);
      return VE_ERROR_INVALID_PARAMETER;
   }

   // With VK_EXT_shader_object: Push constants are specified per-shader, not
   // per-pipeline We need to track which shader stages are currently bound and
   // determine stage flags
   VkShaderStageFlags stageFlags = veGetBoundShaderStages(internal);

   if (stageFlags == 0)
   {
      veSetError("No shader objects bound - cannot push constants");
      return VE_ERROR_INVALID_PARAMETER;
   }

   // find the device's global push constant layout (created at device init)
   VkPipelineLayout pushConstantLayout;
   if (stageFlags == VK_SHADER_STAGE_ALL_GRAPHICS)
      pushConstantLayout = internal->device->globalGraphicsPipelineLayout;
   else
      pushConstantLayout = internal->device->globalComputePipelineLayout;

   vkCmdPushConstants(internal->commandBuffer, pushConstantLayout, stageFlags, (uint32_t)offset, (uint32_t)size, data);
   return VE_SUCCESS;
}

// =============================================================================
// Drawing Commands
// =============================================================================

static uint64_t calculateTriangleContribution(VkPrimitiveTopology topology, uint32_t elementCount,
                                              uint32_t instanceCount, uint32_t patchControlPoints)
{
   uint64_t baseTriangles = 0;

   switch (topology)
   {
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
      baseTriangles = (elementCount / 3u);
      break;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
      baseTriangles = (elementCount > 2u) ? (elementCount - 2u) : 0u;
      break;
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
   case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
      // Adjacency primitives include extra vertices; treat contribution as unknown for now.
      break;
   case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
      if (patchControlPoints >= 3u)
      {
         baseTriangles = elementCount / patchControlPoints;
      }
      break;
   default:
      break;
   }

   return baseTriangles * instanceCount;
}

VEResult veDraw(VECommandBuffer *cmd, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
                uint32_t firstInstance)
{
   if (!cmd || vertexCount == 0)
   {
      veSetError("Invalid parameters for veDraw");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += 1;
      internal->device->frameStats.verticesRendered += static_cast<uint64_t>(vertexCount) * instanceCount;
      internal->device->frameStats.trianglesRendered += calculateTriangleContribution(
          internal->currentTopology, vertexCount, instanceCount, internal->currentPatchControlPoints);
   }
   vkCmdDraw(internal->commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
   return VE_SUCCESS;
}

VEResult veBindVertexBuffer(VECommandBuffer *cmd, uint32_t binding, VEBufferAddress vertexBuffer, uint64_t offset)
{
   if (!cmd || vertexBuffer == 0)
   {
      veSetError("Invalid parameters for veBindVertexBuffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = internal->device->getVkBufferFromAddress(vertexBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid vertex buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   VkDeviceSize vkOffset = offset;
   vkCmdBindVertexBuffers(internal->commandBuffer, binding, 1, &buffer, &vkOffset);
   return VE_SUCCESS;
}

VEResult veBindIndexBuffer(VECommandBuffer *cmd, VEBufferAddress indexBuffer, uint64_t offset, VkIndexType format)
{
   if (!cmd || indexBuffer == 0)
   {
      veSetError("Invalid parameters for veBindIndexBuffer");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = internal->device->getVkBufferFromAddress(indexBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid index buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdBindIndexBuffer(internal->commandBuffer, buffer, offset, format);
   return VE_SUCCESS;
}

VEResult veDrawIndexed(VECommandBuffer *cmd, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                       int32_t vertexOffset, uint32_t firstInstance)
{
   if (!cmd || indexCount == 0)
   {
      veSetError("Invalid parameters for veDrawIndexed");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += 1;
      internal->device->frameStats.verticesRendered += static_cast<uint64_t>(indexCount) * instanceCount;
      internal->device->frameStats.trianglesRendered += calculateTriangleContribution(
          internal->currentTopology, indexCount, instanceCount, internal->currentPatchControlPoints);
   }
   vkCmdDrawIndexed(internal->commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
   return VE_SUCCESS;
}

VEResult veDrawIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset, uint32_t drawCount,
                        uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0)
   {
      veSetError("Invalid parameters for veDrawIndirect");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = internal->device->getVkBufferFromAddress(indirectBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid indirect buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdDrawIndirect(internal->commandBuffer, buffer, offset, drawCount, stride);
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += drawCount;
   }
   return VE_SUCCESS;
}

VEResult veDrawIndexedIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset,
                               uint32_t drawCount, uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0)
   {
      veSetError("Invalid parameters for veDrawIndexedIndirect");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer buffer = internal->device->getVkBufferFromAddress(indirectBuffer);

   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("Invalid indirect buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdDrawIndexedIndirect(internal->commandBuffer, buffer, offset, drawCount, stride);
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += drawCount;
   }
   return VE_SUCCESS;
}

VEResult veDrawIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                             VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount, uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS)
   {
      veSetError("Invalid parameters for veDrawIndirectCount");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer indirectBuf = internal->device->getVkBufferFromAddress(indirectBuffer);
   VkBuffer countBuf = internal->device->getVkBufferFromAddress(countBuffer);

   if (indirectBuf == VK_NULL_HANDLE || countBuf == VK_NULL_HANDLE)
   {
      veSetError("Invalid buffer address for indirect count draw");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdDrawIndirectCount(internal->commandBuffer, indirectBuf, indirectOffset, countBuf, countOffset, maxDrawCount,
                          stride);
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += maxDrawCount;
   }
   return VE_SUCCESS;
}

VEResult veDrawIndexedIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                                    VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount,
                                    uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS)
   {
      veSetError("Invalid parameters for veDrawIndexedIndirectCount");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VkBuffer indirectBuf = internal->device->getVkBufferFromAddress(indirectBuffer);
   VkBuffer countBuf = internal->device->getVkBufferFromAddress(countBuffer);

   if (indirectBuf == VK_NULL_HANDLE || countBuf == VK_NULL_HANDLE)
   {
      veSetError("Invalid buffer address for indexed indirect count draw");
      return VE_ERROR_NOT_FOUND;
   }

   vkCmdDrawIndexedIndirectCount(internal->commandBuffer, indirectBuf, indirectOffset, countBuf, countOffset,
                                 maxDrawCount, stride);
   if (internal->device)
   {
      internal->device->frameStats.drawCalls += maxDrawCount;
   }
   return VE_SUCCESS;
}

// =============================================================================
// Mesh Shader Drawing Commands
// =============================================================================

VEResult veDrawMeshTasks(VECommandBuffer *cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
   if (!cmd)
   {
      veSetError("veDrawMeshTasks: CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   if (!veFuncs.vkCmdDrawMeshTasksEXT)
   {
      veSetError("veDrawMeshTasks: Mesh shaders not supported on this device");
      return VE_ERROR_UNSUPPORTED;
   }

   veFuncs.vkCmdDrawMeshTasksEXT(internal->commandBuffer, groupCountX, groupCountY, groupCountZ);

   if (internal->device)
   {
      internal->device->frameStats.drawCalls += 1;
   }
   return VE_SUCCESS;
}

VEResult veDrawMeshTasksIndirect(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t offset,
                                 uint32_t drawCount, uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || drawCount == 0)
   {
      veSetError("veDrawMeshTasksIndirect: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   if (!veFuncs.vkCmdDrawMeshTasksIndirectEXT)
   {
      veSetError("veDrawMeshTasksIndirect: Mesh shaders not supported on this device");
      return VE_ERROR_UNSUPPORTED;
   }

   VkBuffer buffer = internal->device->getVkBufferFromAddress(indirectBuffer);
   if (buffer == VK_NULL_HANDLE)
   {
      veSetError("veDrawMeshTasksIndirect: Invalid indirect buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   veFuncs.vkCmdDrawMeshTasksIndirectEXT(internal->commandBuffer, buffer, offset, drawCount, stride);

   if (internal->device)
   {
      internal->device->frameStats.drawCalls += drawCount;
   }
   return VE_SUCCESS;
}

VEResult veDrawMeshTasksIndirectCount(VECommandBuffer *cmd, VEBufferAddress indirectBuffer, uint64_t indirectOffset,
                                      VEBufferAddress countBuffer, uint64_t countOffset, uint32_t maxDrawCount,
                                      uint32_t stride)
{
   if (!cmd || indirectBuffer == VE_INVALID_ADDRESS || countBuffer == VE_INVALID_ADDRESS)
   {
      veSetError("veDrawMeshTasksIndirectCount: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;

   if (!veFuncs.vkCmdDrawMeshTasksIndirectCountEXT)
   {
      veSetError("veDrawMeshTasksIndirectCount: Mesh shaders not supported on this device");
      return VE_ERROR_UNSUPPORTED;
   }

   VkBuffer indirectBuf = internal->device->getVkBufferFromAddress(indirectBuffer);
   VkBuffer countBuf = internal->device->getVkBufferFromAddress(countBuffer);

   if (indirectBuf == VK_NULL_HANDLE || countBuf == VK_NULL_HANDLE)
   {
      veSetError("veDrawMeshTasksIndirectCount: Invalid buffer address");
      return VE_ERROR_NOT_FOUND;
   }

   veFuncs.vkCmdDrawMeshTasksIndirectCountEXT(internal->commandBuffer, indirectBuf, indirectOffset, countBuf,
                                              countOffset, maxDrawCount, stride);

   if (internal->device)
   {
      internal->device->frameStats.drawCalls += maxDrawCount;
   }
   return VE_SUCCESS;
}

// =============================================================================
// Synchronization and Memory Barriers
// =============================================================================

VEResult veBarrier(VECommandBuffer *cmd, const VEBarrierDesc *desc)
{
   if (!cmd || !desc)
   {
      veSetError("Command buffer and barrier desc cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;
   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if ((desc->memoryBarrierCount > 0 && !desc->memoryBarriers) ||
       (desc->bufferBarrierCount > 0 && !desc->bufferBarriers) || (desc->imageBarrierCount > 0 && !desc->imageBarriers))
   {
      veSetError("Barrier arrays cannot be NULL when their counts are > 0");
      return VE_ERROR_INVALID_PARAMETER;
   }

   std::vector<VkMemoryBarrier2> memoryBarriers;
   memoryBarriers.reserve(desc->memoryBarrierCount);
   for (uint32_t i = 0; i < desc->memoryBarrierCount; ++i)
   {
      const VEMemoryBarrier &src = desc->memoryBarriers[i];
      VkMemoryBarrier2 b{};
      b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
      b.srcStageMask = src.srcStageMask;
      b.srcAccessMask = src.srcAccessMask;
      b.dstStageMask = src.dstStageMask;
      b.dstAccessMask = src.dstAccessMask;
      memoryBarriers.push_back(b);
   }

   std::vector<VkBufferMemoryBarrier2> bufferBarriers;
   bufferBarriers.reserve(desc->bufferBarrierCount);
   for (uint32_t i = 0; i < desc->bufferBarrierCount; ++i)
   {
      const VEBufferBarrier &src = desc->bufferBarriers[i];
      if (src.buffer == VE_INVALID_ADDRESS)
      {
         veSetError("Buffer barrier %u has invalid buffer address", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      VkBuffer buf = device->getVkBufferFromAddress(src.buffer);
      if (buf == VK_NULL_HANDLE)
      {
         veSetError("Buffer barrier %u references unknown buffer address", i);
         return VE_ERROR_NOT_FOUND;
      }

      VkBufferMemoryBarrier2 b{};
      b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
      b.srcStageMask = src.srcStageMask;
      b.srcAccessMask = src.srcAccessMask;
      b.dstStageMask = src.dstStageMask;
      b.dstAccessMask = src.dstAccessMask;
      b.srcQueueFamilyIndex = src.srcQueueFamilyIndex;
      b.dstQueueFamilyIndex = src.dstQueueFamilyIndex;
      b.buffer = buf;
      b.offset = src.offset;
      b.size = (src.size == 0) ? VK_WHOLE_SIZE : src.size;
      bufferBarriers.push_back(b);
   }

   std::vector<VkImageMemoryBarrier2> imageBarriers;
   imageBarriers.reserve(desc->imageBarrierCount);
   for (uint32_t i = 0; i < desc->imageBarrierCount; ++i)
   {
      const VEImageBarrier &src = desc->imageBarriers[i];
      if (src.image == VE_INVALID_TEXTURE)
      {
         veSetError("Image barrier %u has invalid texture index", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      VETextureInternal *tex = device->getTexture(src.image);
      if (!tex || !tex->isValid || tex->image == VK_NULL_HANDLE)
      {
         veSetError("Image barrier %u references unknown texture index", i);
         return VE_ERROR_NOT_FOUND;
      }

      if (src.subresourceRange.aspectMask == 0)
      {
         veSetError("Image barrier %u subresourceRange.aspectMask cannot be 0", i);
         return VE_ERROR_INVALID_PARAMETER;
      }

      VkImageLayout oldLayout = src.oldLayout;
      if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && tex->currentLayout != VK_IMAGE_LAYOUT_UNDEFINED)
      {
         oldLayout = tex->currentLayout;
      }

      VkImageMemoryBarrier2 b{};
      b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      b.srcStageMask = src.srcStageMask;
      b.srcAccessMask = src.srcAccessMask;
      b.dstStageMask = src.dstStageMask;
      b.dstAccessMask = src.dstAccessMask;
      b.srcQueueFamilyIndex = src.srcQueueFamilyIndex;
      b.dstQueueFamilyIndex = src.dstQueueFamilyIndex;
      b.oldLayout = oldLayout;
      b.newLayout = src.newLayout;
      b.image = tex->image;
      b.subresourceRange = src.subresourceRange;
      imageBarriers.push_back(b);

      // Update tracked layout if this barrier is a layout transition.
      if (src.newLayout != VK_IMAGE_LAYOUT_UNDEFINED)
      {
         tex->currentLayout = src.newLayout;
      }
   }

   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.memoryBarrierCount = (uint32_t)memoryBarriers.size();
   dependencyInfo.pMemoryBarriers = memoryBarriers.empty() ? nullptr : memoryBarriers.data();
   dependencyInfo.bufferMemoryBarrierCount = (uint32_t)bufferBarriers.size();
   dependencyInfo.pBufferMemoryBarriers = bufferBarriers.empty() ? nullptr : bufferBarriers.data();
   dependencyInfo.imageMemoryBarrierCount = (uint32_t)imageBarriers.size();
   dependencyInfo.pImageMemoryBarriers = imageBarriers.empty() ? nullptr : imageBarriers.data();

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);
   return VE_SUCCESS;
}

VEResult veBarrierVertexToFragment(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEMemoryBarrier b{};
   b.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
   b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
   b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
   VEBarrierDesc desc{};
   desc.memoryBarrierCount = 1;
   desc.memoryBarriers = &b;
   return veBarrier(cmd, &desc);
}

VEResult veBarrierComputeToVertex(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEMemoryBarrier b{};
   b.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   b.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
   b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
   VEBarrierDesc desc{};
   desc.memoryBarrierCount = 1;
   desc.memoryBarriers = &b;
   return veBarrier(cmd, &desc);
}

VEResult veBarrierComputeToCompute(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEMemoryBarrier b{};
   b.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
   b.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
   VEBarrierDesc desc{};
   desc.memoryBarrierCount = 1;
   desc.memoryBarriers = &b;
   return veBarrier(cmd, &desc);
}

VEResult veBarrierGraphicsToPresent(VECommandBuffer *cmd)
{
   if (!cmd)
   {
      veSetError("CommandBuffer cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEMemoryBarrier b{};
   b.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
   b.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
   b.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
   b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
   VEBarrierDesc desc{};
   desc.memoryBarrierCount = 1;
   desc.memoryBarriers = &b;
   return veBarrier(cmd, &desc);
}

VEResult veTransitionTexture(VECommandBuffer *cmd, VETexture texture, VkImageLayout oldLayout, VkImageLayout newLayout)
{
   if (!cmd || texture == VE_INVALID_TEXTURE)
   {
      veSetError("Invalid parameters for veTransitionTexture");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VETextureInternal *textureInternal = device->getTexture(texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return VE_ERROR_NOT_FOUND;
   }

   // Skip if layouts are the same
   if (oldLayout == newLayout)
   {
      return VE_SUCCESS;
   }

   // If oldLayout is VK_IMAGE_LAYOUT_UNDEFINED, use the tracked current layout
   VkImageLayout effectiveOldLayout = (VkImageLayout)oldLayout;
   if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && textureInternal->currentLayout != VK_IMAGE_LAYOUT_UNDEFINED)
   {
      effectiveOldLayout = textureInternal->currentLayout;
   }

   // Auto-detect stage masks and access flags based on common transitions
   VkPipelineStageFlags2 srcStage, dstStage;
   VkAccessFlags2 srcAccess, dstAccess;

   // Source stage and access
   switch (effectiveOldLayout)
   {
   case VK_IMAGE_LAYOUT_UNDEFINED:
      srcStage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
      srcAccess = 0;
      break;
   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      srcAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
      break;
   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      srcAccess = VK_ACCESS_2_SHADER_READ_BIT;
      break;
   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      srcStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      srcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      srcStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
      srcAccess = 0;
      break;
   default:
      srcStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      srcAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
      break;
   }

   // Destination stage and access
   switch (newLayout)
   {
   case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      dstAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
      break;
   case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
      break;
   case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      dstStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      break;
   case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      dstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
      dstAccess = 0;
      break;
   case VK_IMAGE_LAYOUT_GENERAL:
      dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      dstAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
      break;
   default:
      dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
      dstAccess = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
      break;
   }

   // Create image memory barrier
   VkImageMemoryBarrier2 imageBarrier{};
   imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   imageBarrier.srcStageMask = srcStage;
   imageBarrier.srcAccessMask = srcAccess;
   imageBarrier.dstStageMask = dstStage;
   imageBarrier.dstAccessMask = dstAccess;
   imageBarrier.oldLayout = effectiveOldLayout;
   imageBarrier.newLayout = (VkImageLayout)newLayout;
   imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   imageBarrier.image = textureInternal->image;
   imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Default to color aspect
   if (textureInternal->format >= VK_FORMAT_D16_UNORM && textureInternal->format <= VK_FORMAT_D32_SFLOAT_S8_UINT)
   {
      imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      // Add stencil aspect for depth-stencil formats
      if (textureInternal->format == VK_FORMAT_D16_UNORM_S8_UINT ||
          textureInternal->format == VK_FORMAT_D24_UNORM_S8_UINT ||
          textureInternal->format == VK_FORMAT_D32_SFLOAT_S8_UINT)
      {
         imageBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
   }
   imageBarrier.subresourceRange.baseMipLevel = 0;
   imageBarrier.subresourceRange.levelCount = textureInternal->mipLevels;
   imageBarrier.subresourceRange.baseArrayLayer = 0;
   imageBarrier.subresourceRange.layerCount = textureInternal->arrayLayers;

   // Submit the barrier
   VkDependencyInfo dependencyInfo{};
   dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependencyInfo.imageMemoryBarrierCount = 1;
   dependencyInfo.pImageMemoryBarriers = &imageBarrier;

   vkCmdPipelineBarrier2(internal->commandBuffer, &dependencyInfo);

   // Update the tracked layout
   textureInternal->currentLayout = (VkImageLayout)newLayout;
   return VE_SUCCESS;
}

// =============================================================================
// Common Texture Layout Transition Helpers
// =============================================================================

VEResult veTransitionTextureForShaderRead(VECommandBuffer *cmd, VETexture texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

VEResult veTransitionTextureForColorAttachment(VECommandBuffer *cmd, VETexture texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

VEResult veTransitionTextureForDepthAttachment(VECommandBuffer *cmd, VETexture texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VEResult veTransitionTextureForTransferSrc(VECommandBuffer *cmd, VETexture texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

VEResult veTransitionTextureForTransferDst(VECommandBuffer *cmd, VETexture texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

VEResult veTransitionTextureForPresent(VECommandBuffer *cmd, VETexture texture)
{
   return veTransitionTexture(cmd, texture, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

VEResult veTransitionTextureToLayout(VECommandBuffer *cmd, VETexture texture, VkImageLayout newLayout)
{
   if (!cmd || texture == VE_INVALID_TEXTURE)
   {
      veSetError("Invalid parameters for veTransitionTextureToLayout");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEDeviceInternal *device = internal->device;

   if (!device)
   {
      veSetError("Command buffer has no device reference");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VETextureInternal *textureInternal = device->getTexture(texture);
   if (!textureInternal || !textureInternal->isValid)
   {
      veSetError("Invalid texture index: %u", texture);
      return VE_ERROR_NOT_FOUND;
   }

   // Use the tracked current layout, or UNDEFINED if not set
   VkImageLayout currentLayout = textureInternal->currentLayout;
   return veTransitionTexture(cmd, texture, currentLayout, newLayout);
}