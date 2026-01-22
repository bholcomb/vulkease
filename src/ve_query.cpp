/**
 * @file ve_query.cpp
 * @brief Query Pool Implementation
 * 
 * Implements occlusion queries, timestamp queries, and pipeline statistics.
 */

#include "ve_internal.h"

// =============================================================================
// Query Pool Internal Structure
// =============================================================================

struct VEQueryPoolInternal
{
   VkQueryPool queryPool;
   VEQueryType type;
   uint32_t queryCount;
   VEDeviceInternal *device;
   char debugName[VE_MAX_DEBUG_NAME_LENGTH];
   bool isValid;
};

// =============================================================================
// Query Pool Management
// =============================================================================

VEResult veCreateQueryPool(VEDevice *device, const VEQueryPoolDesc *desc, VEQueryPool **outPool)
{
   if (!device || !desc || !outPool || desc->queryCount == 0)
   {
      veSetError("veCreateQueryPool: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   VEQueryPoolInternal *pool = new (std::nothrow) VEQueryPoolInternal{};
   if (!pool)
   {
      veSetError("veCreateQueryPool: Failed to allocate memory");
      return VE_ERROR_OUT_OF_MEMORY;
   }

   VkQueryPoolCreateInfo createInfo{};
   createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
   createInfo.queryCount = desc->queryCount;

   switch (desc->type)
   {
   case VE_QUERY_TYPE_OCCLUSION:
      createInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
      break;
   case VE_QUERY_TYPE_TIMESTAMP:
      createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
      break;
   case VE_QUERY_TYPE_PIPELINE_STATISTICS:
      createInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
      createInfo.pipelineStatistics = desc->pipelineStatistics;
      break;
   default:
      delete pool;
      veSetError("veCreateQueryPool: Invalid query type");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkResult result = vkCreateQueryPool(deviceInternal->device, &createInfo, nullptr, &pool->queryPool);
   if (result != VK_SUCCESS)
   {
      delete pool;
      veSetError("veCreateQueryPool: vkCreateQueryPool failed (VkResult: %d)", result);
      return VE_ERROR_OUT_OF_MEMORY;
   }

   pool->type = desc->type;
   pool->queryCount = desc->queryCount;
   pool->device = deviceInternal;
   pool->isValid = true;

   if (desc->debugName)
   {
      strncpy(pool->debugName, desc->debugName, VE_MAX_DEBUG_NAME_LENGTH - 1);
      pool->debugName[VE_MAX_DEBUG_NAME_LENGTH - 1] = '\0';
      veSetObjectDebugName(deviceInternal, (uint64_t)pool->queryPool, VK_OBJECT_TYPE_QUERY_POOL, desc->debugName);
   }
   else
   {
      pool->debugName[0] = '\0';
   }

   *outPool = (VEQueryPool *)pool;
   return VE_SUCCESS;
}

VEResult veDestroyQueryPool(VEQueryPool *pool)
{
   if (!pool)
   {
      return VE_SUCCESS; // Nothing to destroy
   }

   VEQueryPoolInternal *poolInternal = (VEQueryPoolInternal *)pool;

   if (poolInternal->queryPool != VK_NULL_HANDLE && poolInternal->device)
   {
      vkDestroyQueryPool(poolInternal->device->device, poolInternal->queryPool, nullptr);
   }

   delete poolInternal;
   return VE_SUCCESS;
}

VEResult veCmdResetQueryPool(VECommandBuffer *cmd, VEQueryPool *pool,
                             uint32_t firstQuery, uint32_t queryCount)
{
   if (!cmd || !pool)
   {
      veSetError("veCmdResetQueryPool: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEQueryPoolInternal *poolInternal = (VEQueryPoolInternal *)pool;

   if (!poolInternal->isValid)
   {
      veSetError("veCmdResetQueryPool: Query pool is not valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (firstQuery + queryCount > poolInternal->queryCount)
   {
      veSetError("veCmdResetQueryPool: Query range out of bounds");
      return VE_ERROR_INVALID_PARAMETER;
   }

   vkCmdResetQueryPool(internal->commandBuffer, poolInternal->queryPool, firstQuery, queryCount);

   return VE_SUCCESS;
}

VEResult veCmdBeginQuery(VECommandBuffer *cmd, VEQueryPool *pool, uint32_t queryIndex)
{
   if (!cmd || !pool)
   {
      veSetError("veCmdBeginQuery: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEQueryPoolInternal *poolInternal = (VEQueryPoolInternal *)pool;

   if (!poolInternal->isValid)
   {
      veSetError("veCmdBeginQuery: Query pool is not valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (queryIndex >= poolInternal->queryCount)
   {
      veSetError("veCmdBeginQuery: Query index out of bounds");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (poolInternal->type == VE_QUERY_TYPE_TIMESTAMP)
   {
      veSetError("veCmdBeginQuery: Cannot use Begin/End with timestamp queries, use veCmdWriteTimestamp");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkQueryControlFlags flags = 0;
   if (poolInternal->type == VE_QUERY_TYPE_OCCLUSION)
   {
      flags = VK_QUERY_CONTROL_PRECISE_BIT;
   }

   vkCmdBeginQuery(internal->commandBuffer, poolInternal->queryPool, queryIndex, flags);

   return VE_SUCCESS;
}

VEResult veCmdEndQuery(VECommandBuffer *cmd, VEQueryPool *pool, uint32_t queryIndex)
{
   if (!cmd || !pool)
   {
      veSetError("veCmdEndQuery: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEQueryPoolInternal *poolInternal = (VEQueryPoolInternal *)pool;

   if (!poolInternal->isValid)
   {
      veSetError("veCmdEndQuery: Query pool is not valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (queryIndex >= poolInternal->queryCount)
   {
      veSetError("veCmdEndQuery: Query index out of bounds");
      return VE_ERROR_INVALID_PARAMETER;
   }

   vkCmdEndQuery(internal->commandBuffer, poolInternal->queryPool, queryIndex);

   return VE_SUCCESS;
}

VEResult veCmdWriteTimestamp(VECommandBuffer *cmd, VEQueryPool *pool, uint32_t queryIndex,
                             VkPipelineStageFlags2 stage)
{
   if (!cmd || !pool)
   {
      veSetError("veCmdWriteTimestamp: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VECommandBufferInternal *internal = (VECommandBufferInternal *)cmd;
   VEQueryPoolInternal *poolInternal = (VEQueryPoolInternal *)pool;

   if (!poolInternal->isValid)
   {
      veSetError("veCmdWriteTimestamp: Query pool is not valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (poolInternal->type != VE_QUERY_TYPE_TIMESTAMP)
   {
      veSetError("veCmdWriteTimestamp: Query pool must be VE_QUERY_TYPE_TIMESTAMP");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (queryIndex >= poolInternal->queryCount)
   {
      veSetError("veCmdWriteTimestamp: Query index out of bounds");
      return VE_ERROR_INVALID_PARAMETER;
   }

   vkCmdWriteTimestamp2(internal->commandBuffer, stage, poolInternal->queryPool, queryIndex);

   return VE_SUCCESS;
}

VEResult veGetQueryResults(VEDevice *device, VEQueryPool *pool, uint32_t firstQuery,
                           uint32_t queryCount, void *data, uint64_t dataSize,
                           uint64_t stride, VkQueryResultFlags flags)
{
   if (!device || !pool || !data || queryCount == 0)
   {
      veSetError("veGetQueryResults: Invalid parameters");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VEQueryPoolInternal *poolInternal = (VEQueryPoolInternal *)pool;

   if (!poolInternal->isValid)
   {
      veSetError("veGetQueryResults: Query pool is not valid");
      return VE_ERROR_INVALID_PARAMETER;
   }

   if (firstQuery + queryCount > poolInternal->queryCount)
   {
      veSetError("veGetQueryResults: Query range out of bounds");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VkDeviceSize vkStride = (stride == 0) ? sizeof(uint64_t) : stride;
   if (flags & VK_QUERY_RESULT_64_BIT)
   {
      if (stride == 0) vkStride = sizeof(uint64_t);
   }
   else
   {
      if (stride == 0) vkStride = sizeof(uint32_t);
   }

   VkResult result = vkGetQueryPoolResults(deviceInternal->device, poolInternal->queryPool,
                                           firstQuery, queryCount, (size_t)dataSize, data,
                                           vkStride, flags);

   if (result == VK_NOT_READY)
   {
      // Results not ready yet - this is not an error if VK_QUERY_RESULT_WAIT_BIT was not set
      return VE_SUCCESS;
   }

   if (result != VK_SUCCESS)
   {
      veSetError("veGetQueryResults: vkGetQueryPoolResults failed (VkResult: %d)", result);
      return VE_ERROR_UNKNOWN;
   }

   return VE_SUCCESS;
}
