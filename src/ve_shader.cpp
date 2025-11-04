/**
 * @file ve_shader.c
 * @brief Shader Objects Implementation
 */

#include "ve_internal.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

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

   VkDescriptorSetLayout setLayouts[2] = {
       deviceInternal->textureDescriptorSetLayout,
       deviceInternal->samplerDescriptorSetLayout};

   VkShaderStageFlagBits stageFlags =
       (stage == VK_SHADER_STAGE_COMPUTE_BIT) ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_ALL_GRAPHICS;

   VkPushConstantRange shaderPushRange = {
       .stageFlags = stageFlags,
       .offset = 0,
       .size = VE_MAX_PUSH_CONSTANT_BYTES};

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

VEShader *veCreateShaderFromSPIRV(VEDevice *device, VkShaderStageFlags stage, const uint32_t *code, uint64_t codeSize,
                                  const char *entryPoint, const char *debugName)
{
   if (!device || !code || codeSize == 0 || !entryPoint)
   {
      veSetError("Invalid parameters for shader creation");
      return NULL;
   }

   VEDeviceInternal *deviceInternal = (VEDeviceInternal *)device;

   if (deviceInternal->shaderCount >= deviceInternal->maxShaders)
   {
      veSetError("Maximum number of shaders reached");
      return NULL;
   }

   // Allocate shader object
   VEShaderInternal *shader = static_cast<VEShaderInternal *>(calloc(1, sizeof(VEShaderInternal)));
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

   shader->shaderObject = createShaderObject(deviceInternal, stage, code, codeSize, entryPoint, shader->debugName);
   if (shader->shaderObject == VK_NULL_HANDLE)
   {
      free(shader);
      return NULL;
   }

   shader->device = deviceInternal;
   shader->isValid = true;
   deviceInternal->shaderCount += 1;

   return (VEShader *)shader;
}

VEShader *veCreateShaderFromGLSL(VEDevice *device, VkShaderStageFlags stage, const char *source, const char *entryPoint,
                                 const char *debugName)
{
   if (!device || !source || !entryPoint)
   {
      veSetError("Invalid parameters for GLSL shader creation");
      return NULL;
   }

   uint32_t *spirvCode;
   uint64_t spirvSize;

   if (!compileGLSLToSPIRV(source, stage, &spirvCode, &spirvSize))
   {
      return NULL;
   }

   VEShader *shader = veCreateShaderFromSPIRV(device, stage, spirvCode, spirvSize, entryPoint, debugName);

   free(spirvCode);
   return shader;
}

VEShader *veLoadShader(VEDevice *device, const char *filename, VkShaderStageFlags stage, const char *entryPoint,
                       const char *debugName)
{
   if (!device || !filename || !entryPoint)
   {
      veSetError("Invalid parameters for shader loading");
      return NULL;
   }

   FILE *file = fopen(filename, "rb");
   if (!file)
   {
      veSetError("Failed to open shader file: %s", filename);
      return NULL;
   }

   fseek(file, 0, SEEK_END);
   size_t fileSize = (size_t)ftell(file);
   fseek(file, 0, SEEK_SET);

   if (fileSize <= 0 || fileSize % 4 != 0)
   {
      veSetError("Invalid SPIR-V file size: %ld", fileSize);
      fclose(file);
      return NULL;
   }

   std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
   size_t bytesRead = fread(code.data(), sizeof(uint32_t), code.size(), file);
   fclose(file);

   if (bytesRead != code.size())
   {
      veSetError("Failed to read entire shader file");
      return NULL;
   }

   VEShader *shader = veCreateShaderFromSPIRV(device, stage, reinterpret_cast<const uint32_t *>(code.data()), fileSize,
                                              entryPoint, debugName);

   if (shader)
   {
      VEShaderInternal *internal = (VEShaderInternal *)shader;
      strncpy(internal->sourceFile, filename, sizeof(internal->sourceFile) - 1);
      internal->device = (VEDeviceInternal *)device;
   }

   return shader;
}

void veDestroyShader(VEShader *shader)
{
   if (!shader)
      return;

   VEShaderInternal *internal = (VEShaderInternal *)shader;

   if (internal->shaderObject)
   {
      veFuncs.vkDestroyShaderEXT(internal->device->device, internal->shaderObject, NULL);
   }

   if (internal->device && internal->device->shaderCount > 0)
   {
      internal->device->shaderCount -= 1;
   }

   free(internal);
}

// =============================================================================
// Shader Hot-Reload Support
// =============================================================================

VEResult veEnableShaderHotReload(VEShader *shader, const char *sourceFile)
{
   if (!shader || !sourceFile)
   {
      veSetError("Invalid parameters for shader hot-reload");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEShaderInternal *internal = (VEShaderInternal *)shader;

   strncpy(internal->sourceFile, sourceFile, sizeof(internal->sourceFile) - 1);
   internal->hotReloadEnabled = true;

   return VE_SUCCESS;
}

VEResult veReloadShader(VEShader *shader)
{
   if (!shader)
   {
      veSetError("Shader cannot be NULL");
      return VE_ERROR_INVALID_PARAMETER;
   }

   VEShaderInternal *internal = (VEShaderInternal *)shader;

   if (!internal->hotReloadEnabled || !internal->sourceFile[0])
   {
      veSetError("Shader hot-reload not enabled or no source file specified");
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

   VkShaderEXT newShaderObject =
       createShaderObject(deviceInternal, internal->stage, codePtr, codeSize, internal->entryPoint, internal->debugName);

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