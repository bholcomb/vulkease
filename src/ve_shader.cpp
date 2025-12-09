/**
 * @file ve_shader.c
 * @brief Shader Objects Implementation
 */

#include "ve_internal.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <sys/stat.h>
#include <thread>
#include <vector>
#include <limits>

// VEShaderHotReloadState is now defined in ve_internal.h

static uint64_t queryFileTimestamp(const char *path)
{
   if (!path)
   {
      return 0;
   }

   struct stat fileStat;
   if (stat(path, &fileStat) != 0)
   {
      return 0;
   }

#if defined(_WIN32)
   return static_cast<uint64_t>(fileStat.st_mtime);
#else
   return static_cast<uint64_t>(fileStat.st_mtime);
#endif
}

static VEShaderHotReloadState *ensureHotReloadState(VEDeviceInternal *deviceInternal)
{
   if (!deviceInternal)
   {
      return nullptr;
   }

   if (!deviceInternal->shaderHotReloadState)
   {
      deviceInternal->shaderHotReloadState = std::make_unique<VEShaderHotReloadState>();
   }

   return deviceInternal->shaderHotReloadState.get();
}

static void shaderHotReloadThread(VEShaderHotReloadState *state)
{
   constexpr std::chrono::milliseconds pollInterval(250);

   std::unique_lock<std::mutex> lock(state->mutex);
   while (!state->stopRequested)
   {
      if (!state->enabled || state->trackedShaders.empty())
      {
         state->cv.wait(lock, [&] {
            return state->stopRequested || (state->enabled && !state->trackedShaders.empty());
         });
         continue;
      }

      for (VEShaderInternal *shader : state->trackedShaders)
      {
         if (state->stopRequested)
         {
            break;
         }

         if (!shader || !shader->sourceFile[0])
         {
            continue;
         }

         uint64_t timestamp = queryFileTimestamp(shader->sourceFile);
         if (timestamp == 0)
         {
            continue;
         }

         if (timestamp > shader->lastWriteTimestamp)
         {
            shader->lastWriteTimestamp = timestamp;
            shader->pendingReload.store(true, std::memory_order_relaxed);
         }
      }

      lock.unlock();
      std::this_thread::sleep_for(pollInterval);
      lock.lock();
   }

   state->threadRunning = false;
}

static void registerFileShader(VEDeviceInternal *deviceInternal, VEShaderInternal *shader, const char *path,
                               uint64_t timestamp)
{
   if (!deviceInternal || !shader || !path)
   {
      return;
   }

   VEShaderHotReloadState *state = ensureHotReloadState(deviceInternal);
   if (!state)
   {
      return;
   }

   {
      std::lock_guard<std::mutex> lock(state->mutex);

      shader->fromFile = true;
      shader->pendingReload.store(false);
      shader->lastWriteTimestamp = timestamp;
      strncpy(shader->sourceFile, path, sizeof(shader->sourceFile) - 1);
      shader->sourceFile[sizeof(shader->sourceFile) - 1] = '\0';

      auto it = std::find(state->trackedShaders.begin(), state->trackedShaders.end(), shader);
      if (it == state->trackedShaders.end())
      {
         state->trackedShaders.push_back(shader);
      }
   }

   state->cv.notify_all();
}

static void unregisterFileShader(VEDeviceInternal *deviceInternal, VEShaderInternal *shader)
{
   if (!deviceInternal || !shader)
   {
      return;
   }

   VEShaderHotReloadState *state = deviceInternal->shaderHotReloadState.get();
   if (!state)
   {
      return;
   }

   std::lock_guard<std::mutex> lock(state->mutex);
   auto it = std::remove(state->trackedShaders.begin(), state->trackedShaders.end(), shader);
   if (it != state->trackedShaders.end())
   {
      state->trackedShaders.erase(it, state->trackedShaders.end());
   }

   shader->sourceFile[0] = '\0';
   shader->fromFile = false;
   shader->pendingReload.store(false);
   shader->lastWriteTimestamp = 0;

   state->cv.notify_all();
}

void veShutdownShaderHotReload(VEDeviceInternal *deviceInternal)
{
   if (!deviceInternal || !deviceInternal->shaderHotReloadState)
   {
      return;
   }

   VEShaderHotReloadState *state = deviceInternal->shaderHotReloadState.get();

   {
      std::unique_lock<std::mutex> lock(state->mutex);
      if (state->threadRunning)
      {
         state->stopRequested = true;
         state->cv.notify_all();
         lock.unlock();
         if (state->watcherThread.joinable())
         {
            state->watcherThread.join();
         }
         lock.lock();
      }
      state->threadRunning = false;
      state->enabled = false;
      state->stopRequested = false;
      state->trackedShaders.clear();
   }

   // unique_ptr automatically handles deletion
   deviceInternal->shaderHotReloadState.reset();
}

// =============================================================================
// Shader Compilation (Stub - would need glslang integration)
// =============================================================================

static bool compileGLSLToSPIRV(const char *glslSource, VkShaderStageFlags stage, uint32_t **spirvCode,
                               uint64_t *spirvSize)
{
   if (!glslSource || !spirvCode || !spirvSize)
   {
      veSetError("Invalid parameters for GLSL compilation");
      return false;
   }

   // Create temporary file names
   char tempGLSL[256];
   char tempSPIRV[256];
   snprintf(tempGLSL, sizeof(tempGLSL), "/tmp/vulkease_shader_%p.glsl", (void *)glslSource);
   snprintf(tempSPIRV, sizeof(tempSPIRV), "/tmp/vulkease_shader_%p.spv", (void *)glslSource);

   // Write GLSL source to temporary file
   FILE *glslFile = fopen(tempGLSL, "w");
   if (!glslFile)
   {
      veSetError("Failed to create temporary GLSL file");
      return false;
   }

   if (fprintf(glslFile, "%s", glslSource) < 0)
   {
      veSetError("Failed to write GLSL source to temporary file");
      fclose(glslFile);
      remove(tempGLSL);
      return false;
   }
   fclose(glslFile);

   // Determine stage flag for glslangValidator
   const char *stageFlag;
   switch (stage)
   {
   case VK_SHADER_STAGE_VERTEX_BIT:
      stageFlag = "-S vert";
      break;
   case VK_SHADER_STAGE_FRAGMENT_BIT:
      stageFlag = "-S frag";
      break;
   case VK_SHADER_STAGE_COMPUTE_BIT:
      stageFlag = "-S comp";
      break;
   case VK_SHADER_STAGE_GEOMETRY_BIT:
      stageFlag = "-S geom";
      break;
   case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
      stageFlag = "-S tesc";
      break;
   case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
      stageFlag = "-S tese";
      break;
   default:
      veSetError("Unsupported shader stage for compilation");
      remove(tempGLSL);
      return false;
   }

   // Compile GLSL to SPIR-V using glslangValidator
   char command[1024];
   snprintf(command, sizeof(command), "glslangValidator %s -V --target-env vulkan1.3 -o %s %s 2>/dev/null", stageFlag,
            tempSPIRV, tempGLSL);

   int result = system(command);
   remove(tempGLSL); // Clean up GLSL file

   if (result != 0)
   {
      veSetError("GLSL compilation failed - ensure glslangValidator is installed");
      remove(tempSPIRV);
      return false;
   }

   // Read compiled SPIR-V
   FILE *spirvFile = fopen(tempSPIRV, "rb");
   if (!spirvFile)
   {
      veSetError("Failed to open compiled SPIR-V file");
      remove(tempSPIRV);
      return false;
   }

   // Get file size
   fseek(spirvFile, 0, SEEK_END);
   size_t fileSize = (size_t)ftell(spirvFile);
   fseek(spirvFile, 0, SEEK_SET);

   if (fileSize <= 0 || fileSize % 4 != 0)
   {
      veSetError("Invalid SPIR-V file size: %ld", fileSize);
      fclose(spirvFile);
      remove(tempSPIRV);
      return false;
   }

   // Allocate and read SPIR-V code
   std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
   size_t bytesRead = fread(buffer.data(), sizeof(uint32_t), buffer.size(), spirvFile);
   fclose(spirvFile);
   remove(tempSPIRV); // Clean up SPIR-V file

   if (bytesRead != buffer.size())
   {
      veSetError("Failed to read complete SPIR-V file");
      return false;
   }

   *spirvSize = fileSize;
   uint32_t *spirvData = static_cast<uint32_t *>(malloc(fileSize));
   if (!spirvData)
   {
      veSetError("Failed to allocate memory for SPIR-V code");
      return false;
   }

   memcpy(spirvData, buffer.data(), fileSize);
   *spirvCode = spirvData;
   return true;
}

static VkShaderStageFlags getPossibleNextStages(VkShaderStageFlags stage);

static bool hasExtension(const char *path, const char *ext)
{
   if (!path || !ext)
   {
      return false;
   }

   const char *dot = strrchr(path, '.');
   if (!dot)
   {
      return false;
   }

   dot++; // Skip the '.'
   if (*ext == '.')
   {
      ext++;
   }

   while (*dot && *ext)
   {
      if (std::tolower(static_cast<unsigned char>(*dot)) != std::tolower(static_cast<unsigned char>(*ext)))
      {
         return false;
      }
      dot++;
      ext++;
   }

   return *dot == '\0' && *ext == '\0';
}

static VkShaderEXT createShaderObject(VEDeviceInternal *deviceInternal, VkShaderStageFlags stage, const uint32_t *code,
                                      uint64_t codeSize, const char *entryPoint, const char *debugName)
{
   if (!deviceInternal || !code || codeSize == 0 || !entryPoint)
   {
      veSetError("Invalid parameters for shader creation");
      return VK_NULL_HANDLE;
   }

   if (deviceInternal->textureDescriptorSetLayout == VK_NULL_HANDLE ||
       deviceInternal->samplerDescriptorSetLayout == VK_NULL_HANDLE)
   {
      veSetError("Descriptor set layouts are not initialized for shader creation");
      return VK_NULL_HANDLE;
   }

   VkDescriptorSetLayout setLayouts[2] = {deviceInternal->textureDescriptorSetLayout,
                                          deviceInternal->samplerDescriptorSetLayout};

   VkShaderStageFlagBits stageFlags =
       (stage == VK_SHADER_STAGE_COMPUTE_BIT) ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ALL_GRAPHICS;

   VkPushConstantRange shaderPushRange = {.stageFlags = stageFlags, .offset = 0, .size = VE_MAX_PUSH_CONSTANT_BYTES};

   VkShaderCreateInfoEXT shaderCreateInfo{};
   shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
   shaderCreateInfo.stage = static_cast<VkShaderStageFlagBits>(stage);
   shaderCreateInfo.nextStage = getPossibleNextStages(stage);
   shaderCreateInfo.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
   shaderCreateInfo.codeSize = codeSize;
   shaderCreateInfo.pCode = code;
   shaderCreateInfo.pName = entryPoint;
   shaderCreateInfo.setLayoutCount = 2;
   shaderCreateInfo.pSetLayouts = setLayouts;
   shaderCreateInfo.pushConstantRangeCount = 1;
   shaderCreateInfo.pPushConstantRanges = &shaderPushRange;
   VkShaderEXT shaderObject = VK_NULL_HANDLE;
   VkResult result = veFuncs.vkCreateShadersEXT(deviceInternal->device, 1, &shaderCreateInfo, NULL, &shaderObject);
   if (result != VK_SUCCESS)
   {
      veSetError("Failed to create shader object (VkResult: %d)", result);
      return VK_NULL_HANDLE;
   }

   if (deviceInternal->context && deviceInternal->context->validationEnabled)
   {
      const char *name = (debugName && debugName[0]) ? debugName : "Shader";
      veSetObjectDebugName(deviceInternal, (uint64_t)shaderObject, VK_OBJECT_TYPE_SHADER_EXT, name);
   }

   return shaderObject;
}

static bool loadSpirvFromFile(const char *filename, std::vector<uint32_t> &outCode, uint64_t &outSize)
{
   FILE *file = fopen(filename, "rb");
   if (!file)
   {
      veSetError("Failed to open shader file: %s", filename);
      return false;
   }

   fseek(file, 0, SEEK_END);
   size_t fileSize = (size_t)ftell(file);
   fseek(file, 0, SEEK_SET);

   if (fileSize <= 0 || fileSize % 4 != 0)
   {
      veSetError("Invalid SPIR-V file size: %ld", fileSize);
      fclose(file);
      return false;
   }

   outCode.resize(fileSize / sizeof(uint32_t));
   size_t bytesRead = fread(outCode.data(), sizeof(uint32_t), outCode.size(), file);
   fclose(file);

   if (bytesRead != outCode.size())
   {
      veSetError("Failed to read entire shader file: %s", filename);
      return false;
   }

   outSize = static_cast<uint64_t>(fileSize);
   return true;
}

static bool loadTextFile(const char *filename, std::vector<char> &outBuffer)
{
   FILE *file = fopen(filename, "rb");
   if (!file)
   {
      veSetError("Failed to open shader source file: %s", filename);
      return false;
   }

   fseek(file, 0, SEEK_END);
   size_t fileSize = (size_t)ftell(file);
   fseek(file, 0, SEEK_SET);

   outBuffer.resize(fileSize + 1);
   size_t bytesRead = fread(outBuffer.data(), 1, fileSize, file);
   fclose(file);

   if (bytesRead != fileSize)
   {
      veSetError("Failed to read shader source file: %s", filename);
      return false;
   }

   outBuffer[bytesRead] = '\0';
   return true;
}

// =============================================================================
// Shader Creation
// =============================================================================

static VkShaderStageFlags getPossibleNextStages(VkShaderStageFlags stage)
{
   switch (stage)
   {
   case VK_SHADER_STAGE_VERTEX_BIT:
      return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
   case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
      return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
   case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
      return VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
   case VK_SHADER_STAGE_GEOMETRY_BIT:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
   case VK_SHADER_STAGE_FRAGMENT_BIT:
      return 0;
   case VK_SHADER_STAGE_COMPUTE_BIT:
      return 0;
   default:
      return 0;
   }
}

VEShader *veLoadShaderFromBuffer(VEDevice *device, VkShaderStageFlags stage, const void *code, size_t codeSize,
                                 const char *entryPoint, const char *debugName)
{
   if (!device || !code || codeSize == 0 || !entryPoint)
   {
      veSetError("Invalid parameters for shader creation");
      return NULL;
   }

   if ((codeSize % sizeof(uint32_t)) != 0)
   {
      veSetError("SPIR-V code size must be a multiple of 4 bytes");
      return NULL;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   if (deviceInternal->shaderCount >= deviceInternal->maxShaders)
   {
      veSetError("Maximum number of shaders reached");
      return NULL;
   }

   // Allocate shader object
   VEShaderInternal *shader = new (std::nothrow) VEShaderInternal();
   if (!shader)
   {
      veSetError("Failed to allocate shader memory");
      return NULL;
   }

   shader->stage = stage;
   strncpy(shader->entryPoint, entryPoint, sizeof(shader->entryPoint) - 1);

   if (debugName)
   {
      strncpy(shader->debugName, debugName, sizeof(shader->debugName) - 1);
   }
   else
   {
      snprintf(shader->debugName, sizeof(shader->debugName), "Shader_%p", shader);
   }

   const uint32_t *codeWords = static_cast<const uint32_t *>(code);
   shader->shaderObject =
       createShaderObject(deviceInternal, stage, codeWords, static_cast<uint64_t>(codeSize), entryPoint,
                          shader->debugName);
   if (shader->shaderObject == VK_NULL_HANDLE)
   {
      delete shader;
      return NULL;
   }

   shader->device = deviceInternal;
   shader->isValid = true;
   shader->fromFile = false;
   shader->pendingReload.store(false);
   shader->lastWriteTimestamp = 0;
   shader->sourceFile[0] = '\0';

   deviceInternal->shaderCount += 1;

   return (VEShader *)shader;
}

VEShader *veLoadShaderFromFile(VEDevice *device, const char *filename, VkShaderStageFlags stage, const char *entryPoint,
                              const char *debugName)
{
   if (!device || !filename || !entryPoint)
   {
      veSetError("Invalid parameters for shader loading");
      return NULL;
   }

   bool isSpirv = hasExtension(filename, "spv") || hasExtension(filename, "spirv");
   VEShader *shader = NULL;

   if (isSpirv)
   {
      std::vector<uint32_t> code;
      uint64_t codeSize = 0;

      if (!loadSpirvFromFile(filename, code, codeSize))
      {
         return NULL;
      }

      shader = veLoadShaderFromBuffer(device, stage, code.data(), static_cast<size_t>(codeSize), entryPoint, debugName);
   }
   else
   {
      std::vector<char> textSource;
      if (!loadTextFile(filename, textSource))
      {
         return NULL;
      }

      uint32_t *compiledCode = NULL;
      uint64_t compiledSize = 0;
      if (!compileGLSLToSPIRV(textSource.data(), stage, &compiledCode, &compiledSize))
      {
         return NULL;
      }

      shader =
          veLoadShaderFromBuffer(device, stage, compiledCode, static_cast<size_t>(compiledSize), entryPoint, debugName);
      free(compiledCode);
   }

   if (!shader)
   {
      return NULL;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VEShaderInternal *internal = (VEShaderInternal *)shader;

   uint64_t timestamp = queryFileTimestamp(filename);
   registerFileShader(deviceInternal, internal, filename, timestamp);

   return shader;
}

void veDestroyShaderImmediate(VEShader *shader)
{
   if (!shader)
      return;

   VEShaderInternal *internal = (VEShaderInternal *)shader;

   if (internal->shaderObject)
   {
      veFuncs.vkDestroyShaderEXT(internal->device->device, internal->shaderObject, NULL);
   }

   if (internal->device)
   {
      unregisterFileShader(internal->device, internal);
   }

   if (internal->device && internal->device->shaderCount > 0)
   {
      internal->device->shaderCount -= 1;
   }

   delete internal;
}

void veDestroyShader(VEShader *shader)
{
   if (!shader)
      return;

   VEShaderInternal *internal = (VEShaderInternal *)shader;
   VEDeviceInternal *deviceInternal = internal->device;
   VEDeferredDeletionQueue *queue = deviceInternal ? deviceInternal->deferredDeletionQueue.get() : nullptr;

   if (queue)
   {
      queue->enqueueShader(shader);
      return;
   }

   veDestroyShaderImmediate(shader);
}

// =============================================================================
// Shader Hot-Reload Support
// =============================================================================

void veSetShaderHotReloadEnabled(VEDevice *device, bool enable)
{
   if (!device)
   {
      return;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;
   VEShaderHotReloadState *state = ensureHotReloadState(deviceInternal);
   if (!state)
   {
      return;
   }

   if (enable)
   {
      std::unique_lock<std::mutex> lock(state->mutex);
      if (state->enabled)
      {
         return;
      }

      state->enabled = true;
      state->stopRequested = false;

      if (!state->threadRunning)
      {
         state->threadRunning = true;
         state->watcherThread = std::thread(shaderHotReloadThread, state);
      }

      state->cv.notify_all();
   }
   else
   {
      std::unique_lock<std::mutex> lock(state->mutex);
      if (!state->enabled && !state->threadRunning)
      {
         return;
      }

      state->enabled = false;

      if (state->threadRunning)
      {
         state->stopRequested = true;
         state->cv.notify_all();
         lock.unlock();
         if (state->watcherThread.joinable())
         {
            state->watcherThread.join();
         }
         lock.lock();
         state->stopRequested = false;
         state->threadRunning = false;
         state->watcherThread = std::thread();
      }
   }
}

bool veShaderNeedsReload(VEShader *shader)
{
   if (!shader)
   {
      return false;
   }

   VEShaderInternal *internal = (VEShaderInternal *)shader;
   return internal->pendingReload.load(std::memory_order_relaxed);
}

VEResult veReloadShader(VEShader *shader)
{
   if (!shader)
   {
      veSetError("Shader cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEShaderInternal *internal = (VEShaderInternal *)shader;

   internal->pendingReload.store(false);

   if (!internal->fromFile || !internal->sourceFile[0])
   {
      veSetError("Shader hot-reload requires shader to be loaded from file");
      return VE_ERROR_FEATURE_NOT_SUPPORTED;
   }

   VEDeviceInternal *deviceInternal = internal->device;
   if (!deviceInternal)
   {
      veSetError("Shader is not associated with a device");
      return VE_ERROR_INVALID_PARAMETER;
   }

   const char *sourcePath = internal->sourceFile;

   std::vector<uint32_t> fileCode;
   std::vector<char> textSource;
   uint32_t *compiledCode = NULL;
   uint64_t codeSize = 0;
   const uint32_t *codePtr = NULL;

   if (hasExtension(sourcePath, "spv") || hasExtension(sourcePath, "spirv"))
   {
      if (!loadSpirvFromFile(sourcePath, fileCode, codeSize))
      {
         return VE_ERROR_INVALID_PARAMETER;
      }
      codePtr = fileCode.data();
   }
   else
   {
      if (!loadTextFile(sourcePath, textSource))
      {
         return VE_ERROR_INVALID_PARAMETER;
      }

      if (!compileGLSLToSPIRV(textSource.data(), internal->stage, &compiledCode, &codeSize))
      {
         return VE_ERROR_SHADER_COMPILATION_FAILED;
      }
      codePtr = compiledCode;
   }

   VkShaderEXT newShaderObject = createShaderObject(deviceInternal, internal->stage, codePtr, codeSize,
                                                    internal->entryPoint, internal->debugName);

   if (compiledCode)
   {
      free(compiledCode);
   }

   if (newShaderObject == VK_NULL_HANDLE)
   {
      return VE_ERROR_UNKNOWN;
   }

   if (internal->shaderObject)
   {
      veFuncs.vkDestroyShaderEXT(deviceInternal->device, internal->shaderObject, NULL);
   }

   internal->shaderObject = newShaderObject;
   internal->isValid = true;

   uint64_t timestamp = queryFileTimestamp(sourcePath);
   if (timestamp == 0)
   {
      timestamp = internal->lastWriteTimestamp;
   }
   registerFileShader(deviceInternal, internal, sourcePath, timestamp);

   return VE_SUCCESS;
}

// =============================================================================
// Shader Configuration Management
// =============================================================================

VEShaderConfig *veCreateShaderConfig(VEDevice *device, const VEShaderConfigDesc *desc)
{
   if (!device || !desc)
   {
      veSetError("Invalid parameters for shader config creation");
      return NULL;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   // Find free slot in shader configs array
   uint32_t index = UINT32_MAX;
   for (uint32_t i = 0; i < deviceInternal->maxShaderConfigs; i++)
   {
      if (!deviceInternal->shaderConfigs[i].isValid)
      {
         index = i;
         break;
      }
   }

   if (index == UINT32_MAX)
   {
      veSetError("No free shader config slots available");
      return NULL;
   }

   VEShaderConfigInternal *config = &deviceInternal->shaderConfigs[index];
   memset(config, 0, sizeof(VEShaderConfigInternal));

   config->vertexShader = desc->vertexShader;
   config->fragmentShader = desc->fragmentShader;
   config->geometryShader = desc->geometryShader;
   config->tessControlShader = desc->tessControlShader;
   config->tessEvalShader = desc->tessEvalShader;
   config->computeShader = desc->computeShader;

   if (desc->debugName)
   {
      strncpy(config->debugName, desc->debugName, sizeof(config->debugName) - 1);
   }
   else
   {
      snprintf(config->debugName, sizeof(config->debugName), "ShaderConfig_%u", index);
   }

   config->device = deviceInternal;
   config->isValid = true;
   deviceInternal->shaderConfigCount++;

   return (VEShaderConfig *)config;
}

void veDestroyShaderConfig(VEShaderConfig *config)
{
   if (!config)
      return;

   VEShaderConfigInternal *internal = (VEShaderConfigInternal *)config;

   internal->device->shaderConfigCount--;
   // Note: We don't destroy the individual shaders here as they might be used
   // elsewhere The user is responsible for managing shader lifetimes

   memset(internal, 0, sizeof(VEShaderConfigInternal));
}