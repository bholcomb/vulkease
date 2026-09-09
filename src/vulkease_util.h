/**
 * @file vulkease_util.h
 * @brief Optional convenience helpers built on the VulkEase core API.
 */

#ifndef VULKEASE_UTIL_H
#define VULKEASE_UTIL_H

#include "vulkease.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum VEDefaultSampler
{
   VE_DEFAULT_SAMPLER_NEAREST,
   VE_DEFAULT_SAMPLER_LINEAR,
   VE_DEFAULT_SAMPLER_ANISOTROPIC_4X,
   VE_DEFAULT_SAMPLER_ANISOTROPIC_16X,
   VE_DEFAULT_SAMPLER_SHADOW,
   VE_DEFAULT_SAMPLER_COUNT
} VEDefaultSampler;

/* Buffer helpers. */
VULKEASE_API VEResult veCreateVertexBuffer(VEDevice *device, const void *vertices, uint64_t size,
                                            const char *debugName, VEBufferAddress *outAddress);
VULKEASE_API VEResult veCreateIndexBuffer(VEDevice *device, const void *indices, uint64_t size,
                                           const char *debugName, VEBufferAddress *outAddress);
VULKEASE_API VEResult veCreateUniformBuffer(VEDevice *device, uint64_t size, bool persistentlyMapped,
                                             const char *debugName, VEBufferAddress *outAddress);
VULKEASE_API VEResult veCreateStorageBuffer(VEDevice *device, uint64_t size, const char *debugName,
                                             VEBufferAddress *outAddress);
VULKEASE_API VEResult veCreateIndirectBuffer(VEDevice *device, uint64_t size, const char *debugName,
                                              VEBufferAddress *outAddress);
VULKEASE_API VEResult veCopyBuffer(VEDevice *device, VEBufferAddress src, VEBufferAddress dst,
                                    uint64_t srcOffset, uint64_t dstOffset, uint64_t size);

/* Texture construction, file I/O, and synchronous transfer helpers. */
VULKEASE_API VEResult veCreateTexture1D(VEDevice *device, uint32_t width, VkFormat format,
                                         VkImageUsageFlags usage, const char *debugName,
                                         VETexture *outIndex);
VULKEASE_API VEResult veCreateTexture2D(VEDevice *device, uint32_t width, uint32_t height, VkFormat format,
                                         VkImageUsageFlags usage, const char *debugName,
                                         VETexture *outIndex);
VULKEASE_API VEResult veCreateTexture3D(VEDevice *device, uint32_t width, uint32_t height, uint32_t depth,
                                         VkFormat format, VkImageUsageFlags usage, const char *debugName,
                                         VETexture *outIndex);
VULKEASE_API VEResult veCreateTexture2DArray(VEDevice *device, uint32_t width, uint32_t height, uint32_t layers,
                                              VkFormat format, VkImageUsageFlags usage, const char *debugName,
                                              VETexture *outIndex);
VULKEASE_API VEResult veCreateTextureCube(VEDevice *device, uint32_t size, VkFormat format,
                                           VkImageUsageFlags usage, const char *debugName,
                                           VETexture *outIndex);
VULKEASE_API VEResult veCreateTexture2DMultisample(VEDevice *device, uint32_t width, uint32_t height,
                                                    VkFormat format, VkImageUsageFlags usage,
                                                    VkSampleCountFlagBits samples, const char *debugName,
                                                    VETexture *outIndex);
VULKEASE_API VEResult veLoadTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage,
                                     bool generateMips, VETexture *outIndex);
VULKEASE_API VEResult veLoadHDRTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage,
                                        bool generateMips, VETexture *outIndex);
VULKEASE_API VEResult veLoadCubeTexture(VEDevice *device, const char *filenames[6], VkImageUsageFlags usage,
                                         bool generateMips, VETexture *outIndex);
VULKEASE_API VEResult veGenerateMipmaps(VEDevice *device, VETexture texture);
VULKEASE_API VEResult veSaveTexture(VEDevice *device, VETexture texture, const char *filename);
VULKEASE_API VEResult veCopyTexture(VEDevice *device, VETexture src, VETexture dst,
                                     const VETextureCopyRegion *regions, uint32_t regionCount);
VULKEASE_API VEResult veBlitTexture(VEDevice *device, VETexture src, VETexture dst,
                                     const VETextureBlitRegion *regions, uint32_t regionCount, VkFilter filter);
VULKEASE_API VEResult veCopyBufferToTexture(VEDevice *device, VEBufferAddress src, VETexture dst,
                                             const VEBufferTextureCopyRegion *regions, uint32_t regionCount);
VULKEASE_API VEResult veCopyTextureToBuffer(VEDevice *device, VETexture src, VEBufferAddress dst,
                                             const VEBufferTextureCopyRegion *regions, uint32_t regionCount);

/* Sampler, shader, graphics-state, and render-target conveniences. */
VULKEASE_API VEResult veCreateLinearSampler(VEDevice *device, VESamplerIndex *outIndex);
VULKEASE_API VEResult veCreateNearestSampler(VEDevice *device, VESamplerIndex *outIndex);
VULKEASE_API VEResult veCreateAnisotropicSampler(VEDevice *device, float maxAnisotropy,
                                                  VESamplerIndex *outIndex);
VULKEASE_API VEResult veCreateShadowSampler(VEDevice *device, VESamplerIndex *outIndex);
VULKEASE_API VESamplerIndex veGetDefaultSampler(VEDevice *device, VEDefaultSampler sampler);
VULKEASE_API VEResult veLoadShaderFromFile(VEDevice *device, const char *filename, VkShaderStageFlags stage,
                                            const char *entryPoint, const char *debugName, VEShader **outShader);
VULKEASE_API VEResult veSetShaderHotReloadEnabled(VEDevice *device, bool enable);
VULKEASE_API bool veShaderNeedsReload(VEShader *shader);
VULKEASE_API VEResult veReloadShader(VEShader *shader);
VULKEASE_API VEGraphicsPipelineDesc veDefaultGraphicsPipelineDesc(void);
VULKEASE_API VEDrawStateDesc veDefaultDrawStateDesc(void);
VULKEASE_API VEResult veCreateDefaultDrawState(VEDevice *device, VEDrawState **outDrawState);
VULKEASE_API VEResult veCreateWireframeDrawState(VEDevice *device, VEDrawState **outDrawState);
VULKEASE_API VEResult veCreateShadowDrawState(VEDevice *device, VEDrawState **outDrawState);
VULKEASE_API VERenderTarget veCreateRenderTarget(uint32_t width, uint32_t height);
VULKEASE_API VERenderTarget veCreateRenderTargetWithOffset(int32_t x, int32_t y, uint32_t width, uint32_t height);
VULKEASE_API VEResult veResizeRenderTarget(VEDevice *device, VERenderTarget *target, uint32_t width, uint32_t height);
VULKEASE_API VERenderTarget veCreateSimpleRenderTarget(VEDevice *device, uint32_t width, uint32_t height,
                                                        VkFormat colorFormat, VkFormat depthFormat,
                                                        VEColor clearColor, float clearDepth,
                                                        VETexture *outColorTexture,
                                                        VETexture *outDepthTexture);
VULKEASE_API VEResult veBarrierVertexToFragment(VECommandBuffer *cmd);
VULKEASE_API VEResult veBarrierComputeToVertex(VECommandBuffer *cmd);
VULKEASE_API VEResult veBarrierComputeToCompute(VECommandBuffer *cmd);
VULKEASE_API VEResult veBarrierGraphicsToPresent(VECommandBuffer *cmd);
VULKEASE_API VEResult veTransitionTextureForShaderRead(VECommandBuffer *cmd, VETexture texture);
VULKEASE_API VEResult veTransitionTextureForColorAttachment(VECommandBuffer *cmd, VETexture texture);
VULKEASE_API VEResult veTransitionTextureForDepthAttachment(VECommandBuffer *cmd, VETexture texture);
VULKEASE_API VEResult veTransitionTextureForTransferSrc(VECommandBuffer *cmd, VETexture texture);
VULKEASE_API VEResult veTransitionTextureForTransferDst(VECommandBuffer *cmd, VETexture texture);
VULKEASE_API VEResult veTransitionTextureForPresent(VECommandBuffer *cmd, VETexture texture);

#ifdef __cplusplus
}
#endif

#endif
