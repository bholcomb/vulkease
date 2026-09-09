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
/**
 * @brief Create a vertex buffer with initial data.
 *
 * Convenience function that creates a buffer optimized for vertex data with
 * appropriate usage flags and uploads the initial data.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertices Pointer to vertex data to upload.
 * @param[in] size Size of vertex data in bytes.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateVertexBuffer(VEDevice *device, const void *vertices, uint64_t size,
                                            const char *debugName, VEBufferAddress *outAddress);
/**
 * @brief Create an index buffer with initial data.
 *
 * Convenience function that creates a buffer optimized for index data with
 * appropriate usage flags and uploads the initial data.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] indices Pointer to index data to upload.
 * @param[in] size Size of index data in bytes.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateIndexBuffer(VEDevice *device, const void *indices, uint64_t size,
                                           const char *debugName, VEBufferAddress *outAddress);
/**
 * @brief Create a uniform buffer.
 *
 * Convenience function that creates a buffer optimized for uniform data
 * (shader constants) with appropriate usage flags.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] size Size of buffer in bytes.
 * @param[in] persistentlyMapped If true, the buffer remains mapped for efficient updates.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateUniformBuffer(VEDevice *device, uint64_t size, bool persistentlyMapped,
                                             const char *debugName, VEBufferAddress *outAddress);
/**
 * @brief Create a storage buffer.
 *
 * Convenience function that creates a buffer for shader storage with
 * read/write access from shaders.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] size Size of buffer in bytes.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer
 */
VULKEASE_API VEResult veCreateStorageBuffer(VEDevice *device, uint64_t size, const char *debugName,
                                             VEBufferAddress *outAddress);
/**
 * @brief Create an indirect draw/dispatch buffer.
 *
 * Convenience function that creates a buffer for indirect drawing or
 * compute dispatch commands populated by the GPU.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] size Size of buffer in bytes.
 * @param[in] debugName Optional debug name for the buffer. Can be NULL.
 * @param[out] outAddress Receives the buffer device address.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateBuffer, veDestroyBuffer, veDrawIndirect, veDispatchIndirect
 */
VULKEASE_API VEResult veCreateIndirectBuffer(VEDevice *device, uint64_t size, const char *debugName,
                                              VEBufferAddress *outAddress);
/**
 * @brief Copy buffer data on the GPU (immediate version).
 *
 * Records, submits, and waits for a buffer copy operation.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source buffer address.
 * @param[in] dst Destination buffer address.
 * @param[in] srcOffset Byte offset into source buffer.
 * @param[in] dstOffset Byte offset into destination buffer.
 * @param[in] size Number of bytes to copy.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdCopyBuffer
 */
VULKEASE_API VEResult veCopyBuffer(VEDevice *device, VEBufferAddress src, VEBufferAddress dst,
                                    uint64_t srcOffset, uint64_t dstOffset, uint64_t size);

/* Texture construction, file I/O, and synchronous transfer helpers. */
/**
 * @brief Create a 1D texture.
 *
 * Convenience function for creating a simple 1D texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] format Pixel format (e.g., VK_FORMAT_R8G8B8A8_UNORM).
 * @param[in] usage Usage flags (e.g., VK_IMAGE_USAGE_SAMPLED_BIT).
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture1D(VEDevice *device, uint32_t width, VkFormat format,
                                         VkImageUsageFlags usage, const char *debugName,
                                         VETexture *outIndex);
/**
 * @brief Create a 2D texture.
 *
 * Convenience function for creating a simple 2D texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] height Texture height in pixels.
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture2D(VEDevice *device, uint32_t width, uint32_t height, VkFormat format,
                                         VkImageUsageFlags usage, const char *debugName,
                                         VETexture *outIndex);
/**
 * @brief Create a 3D texture.
 *
 * Convenience function for creating a 3D volume texture.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] height Texture height in pixels.
 * @param[in] depth Texture depth in pixels.
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture3D(VEDevice *device, uint32_t width, uint32_t height, uint32_t depth,
                                         VkFormat format, VkImageUsageFlags usage, const char *debugName,
                                         VETexture *outIndex);
/**
 * @brief Create a 2D texture array.
 *
 * Convenience function for creating an array of 2D textures.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] height Texture height in pixels.
 * @param[in] layers Number of array layers.
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture2DArray(VEDevice *device, uint32_t width, uint32_t height, uint32_t layers,
                                              VkFormat format, VkImageUsageFlags usage, const char *debugName,
                                              VETexture *outIndex);
/**
 * @brief Create a cube map texture.
 *
 * Convenience function for creating a cube map with 6 faces.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] size Width and height of each cube face (must be square).
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTextureCube(VEDevice *device, uint32_t size, VkFormat format,
                                           VkImageUsageFlags usage, const char *debugName,
                                           VETexture *outIndex);
/**
 * @brief Create a multisampled 2D texture.
 *
 * Creates a 2D texture with multisampling for MSAA rendering.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Texture width in pixels.
 * @param[in] height Texture height in pixels.
 * @param[in] format Pixel format.
 * @param[in] usage Usage flags.
 * @param[in] samples Number of samples (e.g., VK_SAMPLE_COUNT_4_BIT).
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateTexture, veDestroyTexture
 */
VULKEASE_API VEResult veCreateTexture2DMultisample(VEDevice *device, uint32_t width, uint32_t height,
                                                    VkFormat format, VkImageUsageFlags usage,
                                                    VkSampleCountFlagBits samples, const char *debugName,
                                                    VETexture *outIndex);
/**
 * @brief Load a texture from an image file.
 *
 * Loads common image formats (PNG, JPG, BMP, TGA, etc.) using STB Image.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] filename Path to the image file.
 * @param[in] usage Usage flags to apply to the texture.
 * @param[in] generateMips If true, automatically generate mipmaps.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_NOT_FOUND if the file doesn't exist
 *         - VE_ERROR_UNSUPPORTED if the format is not supported
 *
 * @see veDestroyTexture
 */
VULKEASE_API VEResult veLoadTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage,
                                     bool generateMips, VETexture *outIndex);
/**
 * @brief Load an HDR texture from a file.
 *
 * Loads HDR image files (.hdr format) using STB Image.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] filename Path to the HDR image file.
 * @param[in] usage Usage flags to apply to the texture.
 * @param[in] generateMips If true, automatically generate mipmaps.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veLoadTexture, veDestroyTexture
 */
VULKEASE_API VEResult veLoadHDRTexture(VEDevice *device, const char *filename, VkImageUsageFlags usage,
                                        bool generateMips, VETexture *outIndex);
/**
 * @brief Load a cube map texture from 6 image files.
 *
 * Loads 6 images for the cube map faces: +X, -X, +Y, -Y, +Z, -Z.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] filenames Array of 6 file paths for each cube face.
 * @param[in] usage Usage flags to apply to the texture.
 * @param[in] generateMips If true, automatically generate mipmaps.
 * @param[out] outIndex Receives the texture index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veDestroyTexture
 */
VULKEASE_API VEResult veLoadCubeTexture(VEDevice *device, const char *filenames[6], VkImageUsageFlags usage,
                                         bool generateMips, VETexture *outIndex);
/**
 * @brief Generate mipmaps for a texture (immediate/blocking version).
 *
 * Generates mipmaps immediately, blocking until complete.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Texture to generate mipmaps for.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdGenerateMipmaps
 */
VULKEASE_API VEResult veGenerateMipmaps(VEDevice *device, VETexture texture);
/**
 * @brief Save a texture to an image file.
 *
 * Saves the texture to disk. Format is determined by file extension
 * (supports .png, .bmp, .tga, .jpg).
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] texture Texture to save.
 * @param[in] filename Output file path with extension.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 */
VULKEASE_API VEResult veSaveTexture(VEDevice *device, VETexture texture, const char *filename);
/**
 * @brief Copy texture to texture (immediate version).
 *
 * Records, submits, and waits for a texture copy.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source texture index.
 * @param[in] dst Destination texture index.
 * @param[in] regions Copy regions.
 * @param[in] regionCount Number of entries in @p regions.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdCopyTexture
 */
VULKEASE_API VEResult veCopyTexture(VEDevice *device, VETexture src, VETexture dst,
                                     const VETextureCopyRegion *regions, uint32_t regionCount);
/**
 * @brief Blit (scaled copy) texture to texture (immediate version).
 *
 * Records, submits, and waits for a texture blit.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source texture index.
 * @param[in] dst Destination texture index.
 * @param[in] regions Blit regions with source and destination bounds.
 * @param[in] regionCount Number of entries in @p regions.
 * @param[in] filter Filtering mode.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdBlitTexture
 */
VULKEASE_API VEResult veBlitTexture(VEDevice *device, VETexture src, VETexture dst,
                                     const VETextureBlitRegion *regions, uint32_t regionCount, VkFilter filter);
/**
 * @brief Copy buffer to texture (immediate version).
 *
 * Records, submits, and waits for a buffer-to-texture copy.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source buffer address.
 * @param[in] dst Destination texture index.
 * @param[in] regions Copy regions.
 * @param[in] regionCount Number of entries in @p regions.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdCopyBufferToTexture
 */
VULKEASE_API VEResult veCopyBufferToTexture(VEDevice *device, VEBufferAddress src, VETexture dst,
                                             const VEBufferTextureCopyRegion *regions, uint32_t regionCount);
/**
 * @brief Copy texture to buffer (immediate version).
 *
 * Records, submits, and waits for a texture-to-buffer copy.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] src Source texture index.
 * @param[in] dst Destination buffer address.
 * @param[in] regions Copy regions.
 * @param[in] regionCount Number of entries in @p regions.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCmdCopyTextureToBuffer
 */
VULKEASE_API VEResult veCopyTextureToBuffer(VEDevice *device, VETexture src, VEBufferAddress dst,
                                             const VEBufferTextureCopyRegion *regions, uint32_t regionCount);

/* Sampler, shader, graphics-state, and render-target conveniences. */
/**
 * @brief Create a sampler with linear (bilinear) filtering.
 *
 * Convenience function that creates a sampler using linear min/mag filtering
 * with repeat addressing and mipmap support.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outIndex Receives the sampler index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateSampler, veDestroySampler
 */
VULKEASE_API VEResult veCreateLinearSampler(VEDevice *device, VESamplerIndex *outIndex);
/**
 * @brief Create a sampler with nearest (point) filtering.
 *
 * Convenience function that creates a sampler using nearest-neighbor filtering
 * with repeat addressing. Ideal for pixel art or when exact texel values are needed.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outIndex Receives the sampler index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateSampler, veDestroySampler
 */
VULKEASE_API VEResult veCreateNearestSampler(VEDevice *device, VESamplerIndex *outIndex);
/**
 * @brief Create a sampler with anisotropic filtering.
 *
 * Convenience function that creates a sampler with anisotropic filtering for
 * high-quality texture sampling at oblique angles.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] maxAnisotropy Maximum anisotropy level (typically 2, 4, 8, or 16).
 * @param[out] outIndex Receives the sampler index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateSampler, veDestroySampler
 */
VULKEASE_API VEResult veCreateAnisotropicSampler(VEDevice *device, float maxAnisotropy,
                                                  VESamplerIndex *outIndex);
/**
 * @brief Create a sampler optimized for shadow mapping.
 *
 * Convenience function that creates a sampler with comparison enabled for
 * hardware PCF shadow sampling.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outIndex Receives the sampler index.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateSampler, veDestroySampler
 */
VULKEASE_API VEResult veCreateShadowSampler(VEDevice *device, VESamplerIndex *outIndex);
/**
 * @brief Get a pre-created default sampler.
 *
 * Returns one of the built-in samplers created at device initialization.
 * These samplers are managed by VulkEase and should not be destroyed.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] sampler Type of default sampler to retrieve.
 *
 * @return Sampler index, or VE_INVALID_SAMPLER_INDEX if device is NULL.
 *
 * @see VEDefaultSampler
 */
VULKEASE_API VESamplerIndex veGetDefaultSampler(VEDevice *device, VEDefaultSampler sampler);
/**
 * @brief Load a shader from a SPIR-V file.
 *
 * Loads and creates a shader object from a .spv file.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] filename Path to the .spv file.
 * @param[in] stage Shader stage.
 * @param[in] entryPoint Entry point function name. Can be NULL for "main".
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outShader Receives the shader object handle.
 *
 * @return VE_SUCCESS on success, or:
 *         - VE_ERROR_NOT_FOUND if the file doesn't exist
 *         - VE_ERROR_SHADER_COMPILATION_FAILED if shader creation failed
 *
 * @see veLoadShaderFromBuffer, veDestroyShader
 */
VULKEASE_API VEResult veLoadShaderFromFile(VEDevice *device, const char *filename, VkShaderStageFlags stage,
                                            const char *entryPoint, const char *debugName, VEShader **outShader);
/**
 * @brief Enable or disable shader hot-reload for development.
 *
 * When enabled, shaders loaded from files will track their source files
 * for changes. Use veShaderNeedsReload() and veReloadShader() to update.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] enable True to enable hot-reload, false to disable.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @note Hot-reload is intended for development only. Disable in release builds.
 * @see veShaderNeedsReload, veReloadShader
 */
VULKEASE_API VEResult veSetShaderHotReloadEnabled(VEDevice *device, bool enable);
/**
 * @brief Check whether a file-backed shader has changed on disk.
 *
 * @param[in] shader Valid shader loaded from a file.
 * @return true when the shader source is newer and should be reloaded.
 */
VULKEASE_API bool veShaderNeedsReload(VEShader *shader);
/**
 * @brief Reload a changed file-backed shader.
 *
 * Recreates the shader object from its source file while preserving the public
 * shader handle.
 *
 * @param[in] shader Valid shader loaded from a file.
 * @return VE_SUCCESS on success, or an error code on failure.
 */
VULKEASE_API VEResult veReloadShader(VEShader *shader);
/**
 * @brief Get a default-initialized graphics pipeline descriptor.
 *
 * Returns a VEGraphicsPipelineDesc with sensible defaults. Modify the returned
 * descriptor and pass to veCreateGraphicsPipeline().
 *
 * @return Default-initialized VEGraphicsPipelineDesc.
 *
 * @see VEGraphicsPipelineDesc, veCreateGraphicsPipeline
 */
VULKEASE_API VEGraphicsPipelineDesc veDefaultGraphicsPipelineDesc(void);
/**
 * @brief Get a default-initialized draw state descriptor.
 *
 * Returns a VEDrawStateDesc with sensible defaults. Modify the returned
 * descriptor and pass to veCreateDrawState().
 *
 * @return Default-initialized VEDrawStateDesc.
 *
 * @see VEDrawStateDesc, veCreateDrawState
 */
VULKEASE_API VEDrawStateDesc veDefaultDrawStateDesc(void);
/**
 * @brief Create a default draw state.
 *
 * Convenience function that creates a draw state with standard defaults:
 * triangle list topology, filled polygons, back-face culling, CCW front face.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outDrawState Receives the draw state handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateDrawState, veDestroyDrawState
 */
VULKEASE_API VEResult veCreateDefaultDrawState(VEDevice *device, VEDrawState **outDrawState);
/**
 * @brief Create a wireframe draw state.
 *
 * Convenience function that creates a draw state for wireframe rendering:
 * line polygon mode, no culling.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outDrawState Receives the draw state handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateDrawState, veDestroyDrawState
 */
VULKEASE_API VEResult veCreateWireframeDrawState(VEDevice *device, VEDrawState **outDrawState);
/**
 * @brief Create a draw state for shadow map rendering.
 *
 * Convenience function that creates a draw state optimized for shadow maps:
 * front-face culling and depth bias enabled to reduce shadow acne.
 *
 * @param[in] device Valid VulkEase device.
 * @param[out] outDrawState Receives the draw state handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateDrawState, veDestroyDrawState
 */
VULKEASE_API VEResult veCreateShadowDrawState(VEDevice *device, VEDrawState **outDrawState);
/**
 * @brief Create a VERenderTarget with the specified render area.
 *
 * Initializes a render target structure for use with veBeginRendering().
 * The render area starts at (0, 0) with the specified dimensions.
 *
 * @param[in] width Width of the render area in pixels.
 * @param[in] height Height of the render area in pixels.
 *
 * @return Initialized VERenderTarget structure.
 *
 * @see veCreateRenderTargetWithOffset, veBeginRendering
 */
VULKEASE_API VERenderTarget veCreateRenderTarget(uint32_t width, uint32_t height);
/**
 * @brief Create a VERenderTarget with offset and dimensions.
 *
 * Initializes a render target structure with a specific render area offset.
 * Useful for rendering to a sub-region of an attachment.
 *
 * @param[in] x X offset of the render area.
 * @param[in] y Y offset of the render area.
 * @param[in] width Width of the render area in pixels.
 * @param[in] height Height of the render area in pixels.
 *
 * @return Initialized VERenderTarget structure.
 *
 * @see veCreateRenderTarget, veBeginRendering
 */
VULKEASE_API VERenderTarget veCreateRenderTargetWithOffset(int32_t x, int32_t y, uint32_t width, uint32_t height);
/**
 * @brief Resize all textures attached to a render target.
 *
 * Destroys the existing textures and creates new ones with the specified dimensions.
 * The render target configuration (attachments, load ops, clear values) is preserved.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in,out] target Render target to resize.
 * @param[in] width New width in pixels.
 * @param[in] height New height in pixels.
 *
 * @return VE_SUCCESS on success.
 */
VULKEASE_API VEResult veResizeRenderTarget(VEDevice *device, VERenderTarget *target, uint32_t width, uint32_t height);
/**
 * @brief Create a simple render target with color and optional depth.
 *
 * Convenience function that creates textures and configures a render target
 * with a single color attachment and optional depth attachment.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] width Width in pixels.
 * @param[in] height Height in pixels.
 * @param[in] colorFormat Color texture format.
 * @param[in] depthFormat Depth texture format (VK_FORMAT_UNDEFINED for no depth).
 * @param[in] clearColor Clear color value.
 * @param[in] clearDepth Clear depth value.
 * @param[out] outColorTexture Receives the owned color texture. Required when
 *                             colorFormat is not VK_FORMAT_UNDEFINED.
 * @param[out] outDepthTexture Receives the owned depth texture. Required when
 *                             depthFormat is not VK_FORMAT_UNDEFINED.
 *
 * @return Configured non-owning VERenderTarget structure. The caller must
 *         destroy every returned texture after the render target is no longer used.
 */
VULKEASE_API VERenderTarget veCreateSimpleRenderTarget(VEDevice *device, uint32_t width, uint32_t height,
                                                        VkFormat colorFormat, VkFormat depthFormat,
                                                        VEColor clearColor, float clearDepth,
                                                        VETexture *outColorTexture,
                                                        VETexture *outDepthTexture);
/** @brief Insert a barrier for vertex shader output to fragment shader read. */
VULKEASE_API VEResult veBarrierVertexToFragment(VECommandBuffer *cmd);
/** @brief Insert a barrier for compute shader output to vertex shader read. */
VULKEASE_API VEResult veBarrierComputeToVertex(VECommandBuffer *cmd);
/** @brief Insert a barrier between compute shader dispatches. */
VULKEASE_API VEResult veBarrierComputeToCompute(VECommandBuffer *cmd);
/** @brief Insert a barrier for graphics output to presentation. */
VULKEASE_API VEResult veBarrierGraphicsToPresent(VECommandBuffer *cmd);
/** @brief Transition texture for shader read access. */
VULKEASE_API VEResult veTransitionTextureForShaderRead(VECommandBuffer *cmd, VETexture texture);
/** @brief Transition texture for use as a color attachment. */
VULKEASE_API VEResult veTransitionTextureForColorAttachment(VECommandBuffer *cmd, VETexture texture);
/** @brief Transition texture for use as a depth attachment. */
VULKEASE_API VEResult veTransitionTextureForDepthAttachment(VECommandBuffer *cmd, VETexture texture);
/** @brief Transition texture for use as a transfer source. */
VULKEASE_API VEResult veTransitionTextureForTransferSrc(VECommandBuffer *cmd, VETexture texture);
/** @brief Transition texture for use as a transfer destination. */
VULKEASE_API VEResult veTransitionTextureForTransferDst(VECommandBuffer *cmd, VETexture texture);
/** @brief Transition texture for presentation to swapchain. */
VULKEASE_API VEResult veTransitionTextureForPresent(VECommandBuffer *cmd, VETexture texture);

// High-level graphics and diagnostic conveniences.

/**
 * @brief Create an opaque rendering pipeline.
 *
 * Convenience function that creates a pipeline configured for standard opaque
 * geometry: depth test enabled, depth write enabled, no blending.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateOpaquePipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                             const VEVertexBinding *bindings, uint32_t bindingCount,
                                             const VEVertexAttribute *attrs, uint32_t attrCount, const char *debugName,
                                             VEGraphicsPipeline **outPipeline);

/**
 * @brief Create a transparent rendering pipeline.
 *
 * Convenience function that creates a pipeline configured for alpha-blended
 * transparent geometry: depth test enabled, depth write disabled, alpha blending.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateTransparentPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                                  const VEVertexBinding *bindings, uint32_t bindingCount,
                                                  const VEVertexAttribute *attrs, uint32_t attrCount,
                                                  const char *debugName, VEGraphicsPipeline **outPipeline);

/**
 * @brief Create an additive blending pipeline.
 *
 * Convenience function that creates a pipeline configured for additive blending,
 * commonly used for particles, glows, and light effects.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateAdditivePipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                               const VEVertexBinding *bindings, uint32_t bindingCount,
                                               const VEVertexAttribute *attrs, uint32_t attrCount,
                                               const char *debugName, VEGraphicsPipeline **outPipeline);

/**
 * @brief Create a shadow map rendering pipeline.
 *
 * Convenience function that creates a pipeline optimized for shadow map generation:
 * depth-only rendering with optional depth bias.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object. Can be NULL for depth-only.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateShadowPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                             const VEVertexBinding *bindings, uint32_t bindingCount,
                                             const VEVertexAttribute *attrs, uint32_t attrCount, const char *debugName,
                                             VEGraphicsPipeline **outPipeline);

/**
 * @brief Create a UI overlay rendering pipeline.
 *
 * Convenience function that creates a pipeline optimized for UI rendering:
 * no depth testing, alpha blending enabled.
 *
 * @param[in] device Valid VulkEase device.
 * @param[in] vertexShader Vertex shader object.
 * @param[in] fragmentShader Fragment shader object.
 * @param[in] bindings Vertex buffer bindings array. Can be NULL if bindingCount is 0.
 * @param[in] bindingCount Number of vertex bindings.
 * @param[in] attrs Vertex attribute array. Can be NULL if attrCount is 0.
 * @param[in] attrCount Number of vertex attributes.
 * @param[in] debugName Optional debug name. Can be NULL.
 * @param[out] outPipeline Receives the pipeline handle.
 *
 * @return VE_SUCCESS on success, or an error code on failure.
 *
 * @see veCreateGraphicsPipeline, veDestroyGraphicsPipeline
 */
VULKEASE_API VEResult veCreateUIOverlayPipeline(VEDevice *device, VEShader *vertexShader, VEShader *fragmentShader,
                                                const VEVertexBinding *bindings, uint32_t bindingCount,
                                                const VEVertexAttribute *attrs, uint32_t attrCount,
                                                const char *debugName, VEGraphicsPipeline **outPipeline);

/**
 * @brief Blit a texture to a swapchain for presentation.
 *
 * Copies/scales a texture to the current swapchain image. Acquires the next
 * swapchain image and transitions it for presentation.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] texture Source texture to blit.
 * @param[in] swapchain Destination swapchain.
 * @param[in] filter Filtering mode (VK_FILTER_NEAREST or VK_FILTER_LINEAR).
 *
 * @return VE_SUCCESS on success, or VE_ERROR_SWAPCHAIN_OUT_OF_DATE if resize needed.
 */
VULKEASE_API VEResult veBlitTextureToSwapchain(VECommandBuffer *cmd, VETexture texture, VESwapchain *swapchain,
                                               VkFilter filter);

/** @brief Convenience function to enable or disable wireframe rendering. */
VULKEASE_API VEResult veSetWireframe(VECommandBuffer *cmd, bool enabled);

/**
 * @brief Convenience function to configure depth testing.
 *
 * @param[in] cmd Command buffer in recording state.
 * @param[in] testEnabled Whether depth testing is enabled.
 * @param[in] writeEnabled Whether depth writing is enabled.
 */
VULKEASE_API VEResult veSetDepthTesting(VECommandBuffer *cmd, bool testEnabled, bool writeEnabled);

/** @brief Print debug information about the device to the message callback. */
VULKEASE_API VEResult vePrintDebugInfo(VEDevice *device);

/** @brief Print profiling information to the message callback. */
VULKEASE_API VEResult vePrintProfileInfo(VEDevice *device);

/** @brief Print graphics pipeline configuration to the message callback. */
VULKEASE_API VEResult vePrintGraphicsPipeline(VEGraphicsPipeline *pipeline);

/** @brief Validate a graphics pipeline configuration for common errors. */
VULKEASE_API VEResult veValidateGraphicsPipeline(VEGraphicsPipeline *pipeline);

#ifdef __cplusplus
}
#endif

#endif
