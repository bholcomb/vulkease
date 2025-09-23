/**
 * @file ve_shader.c
 * @brief Shader Objects Implementation
 */

#include "ve_internal.h"

// =============================================================================
// Shader Stage Conversion
// =============================================================================

VkShaderStageFlagBits veShaderStageToVk(VEShaderStage stage) {
    switch (stage) {
        case VE_SHADER_STAGE_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
        case VE_SHADER_STAGE_TESSELLATION_CONTROL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case VE_SHADER_STAGE_TESSELLATION_EVALUATION: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case VE_SHADER_STAGE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case VE_SHADER_STAGE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case VE_SHADER_STAGE_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
        default: return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

// =============================================================================
// Shader Compilation (Stub - would need glslang integration)
// =============================================================================

static bool compileGLSLToSPIRV(const char* glslSource, VEShaderStage stage, 
                               uint32_t** spirvCode, size_t* spirvSize) {
    if (!glslSource || !spirvCode || !spirvSize) {
        veSetError("Invalid parameters for GLSL compilation");
        return false;
    }
    
    // Create temporary file names
    char tempGLSL[256];
    char tempSPIRV[256];
    snprintf(tempGLSL, sizeof(tempGLSL), "/tmp/vulkease_shader_%p.glsl", (void*)glslSource);
    snprintf(tempSPIRV, sizeof(tempSPIRV), "/tmp/vulkease_shader_%p.spv", (void*)glslSource);
    
    // Write GLSL source to temporary file
    FILE* glslFile = fopen(tempGLSL, "w");
    if (!glslFile) {
        veSetError("Failed to create temporary GLSL file");
        return false;
    }
    
    if (fprintf(glslFile, "%s", glslSource) < 0) {
        veSetError("Failed to write GLSL source to temporary file");
        fclose(glslFile);
        remove(tempGLSL);
        return false;
    }
    fclose(glslFile);
    
    // Determine stage flag for glslangValidator
    const char* stageFlag;
    switch (stage) {
        case VE_SHADER_STAGE_VERTEX: stageFlag = "-S vert"; break;
        case VE_SHADER_STAGE_FRAGMENT: stageFlag = "-S frag"; break;
        case VE_SHADER_STAGE_COMPUTE: stageFlag = "-S comp"; break;
        case VE_SHADER_STAGE_GEOMETRY: stageFlag = "-S geom"; break;
        case VE_SHADER_STAGE_TESSELLATION_CONTROL: stageFlag = "-S tesc"; break;
        case VE_SHADER_STAGE_TESSELLATION_EVALUATION: stageFlag = "-S tese"; break;
        default:
            veSetError("Unsupported shader stage for compilation");
            remove(tempGLSL);
            return false;
    }
    
    // Compile GLSL to SPIR-V using glslangValidator
    char command[512];
    snprintf(command, sizeof(command), "glslangValidator %s -V --target-env vulkan1.3 -o %s %s 2>/dev/null", 
             stageFlag, tempSPIRV, tempGLSL);
    
    int result = system(command);
    remove(tempGLSL); // Clean up GLSL file
    
    if (result != 0) {
        veSetError("GLSL compilation failed - ensure glslangValidator is installed");
        remove(tempSPIRV);
        return false;
    }
    
    // Read compiled SPIR-V
    FILE* spirvFile = fopen(tempSPIRV, "rb");
    if (!spirvFile) {
        veSetError("Failed to open compiled SPIR-V file");
        remove(tempSPIRV);
        return false;
    }
    
    // Get file size
    fseek(spirvFile, 0, SEEK_END);
    long fileSize = ftell(spirvFile);
    fseek(spirvFile, 0, SEEK_SET);
    
    if (fileSize <= 0 || fileSize % 4 != 0) {
        veSetError("Invalid SPIR-V file size: %ld", fileSize);
        fclose(spirvFile);
        remove(tempSPIRV);
        return false;
    }
    
    // Allocate and read SPIR-V code
    *spirvCode = malloc(fileSize);
    if (!*spirvCode) {
        veSetError("Failed to allocate memory for SPIR-V code");
        fclose(spirvFile);
        remove(tempSPIRV);
        return false;
    }
    
    size_t bytesRead = fread(*spirvCode, 1, fileSize, spirvFile);
    fclose(spirvFile);
    remove(tempSPIRV); // Clean up SPIR-V file
    
    if (bytesRead != fileSize) {
        veSetError("Failed to read complete SPIR-V file");
        free(*spirvCode);
        *spirvCode = NULL;
        return false;
    }
    
    *spirvSize = fileSize;
    return true;
}

// =============================================================================
// Shader Creation
// =============================================================================

VEShader* veCreateShaderFromSPIRV(VEDevice* device, VEShaderStage stage,
                                 const uint32_t* code, size_t codeSize,
                                 const char* entryPoint, const char* debugName) {
    if (!device || !code || codeSize == 0 || !entryPoint) {
        veSetError("Invalid parameters for shader creation");
        return NULL;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    // Allocate shader object
    VEShaderInternal* shader = calloc(1, sizeof(VEShaderInternal));
    if (!shader) {
        veSetError("Failed to allocate shader memory");
        return NULL;
    }
    
    shader->stage = stage;
    strncpy(shader->entryPoint, entryPoint, sizeof(shader->entryPoint) - 1);
    
    if (debugName) {
        strncpy(shader->debugName, debugName, sizeof(shader->debugName) - 1);
    } else {
        snprintf(shader->debugName, sizeof(shader->debugName), "Shader_%p", shader);
    }

    //create descriptor set
    VkDescriptorSetLayout setLayouts[2] = {
        deviceInternal->textureDescriptorSetLayout,  // will be set = 0 in shaders
        deviceInternal->samplerDescriptorSetLayout   // will be set = 1 in shaders
    };
    
    VkPushConstantRange shaderPushRange = {
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset = 0,
        .size = 128
    };
    
    // Create shader object using VK_EXT_shader_object
    VkShaderCreateInfoEXT shaderCreateInfo = {0};
    shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
    shaderCreateInfo.stage = veShaderStageToVk(stage);
    shaderCreateInfo.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    shaderCreateInfo.codeSize = codeSize;
    shaderCreateInfo.pCode = code;
    shaderCreateInfo.pName = entryPoint;
    shaderCreateInfo.setLayoutCount = 2;
    shaderCreateInfo.pSetLayouts = setLayouts;
    shaderCreateInfo.pushConstantRangeCount = 1;
    shaderCreateInfo.pPushConstantRanges = &shaderPushRange;
        
    VkResult result = veFuncs.vkCreateShadersEXT(deviceInternal->device, 1, &shaderCreateInfo, NULL, &shader->shaderObject);
    if (result != VK_SUCCESS) {
        veSetError("Failed to create shader object (VkResult: %d)", result);
        free(shader);
        return NULL;
    }
    
    if (deviceInternal->context->validationEnabled) {
        veSetObjectDebugName(deviceInternal, (uint64_t)shader->shaderObject, VK_OBJECT_TYPE_SHADER_EXT, shader->debugName);
    }
    
    shader->isValid = true;
    
    return (VEShader*)shader;
}

VEShader* veCreateShaderFromGLSL(VEDevice* device, VEShaderStage stage,
                                const char* source, const char* entryPoint,
                                const char* debugName) {
    if (!device || !source || !entryPoint) {
        veSetError("Invalid parameters for GLSL shader creation");
        return NULL;
    }
    
    uint32_t* spirvCode;
    size_t spirvSize;
    
    if (!compileGLSLToSPIRV(source, stage, &spirvCode, &spirvSize)) {
        return NULL;
    }
    
    VEShader* shader = veCreateShaderFromSPIRV(device, stage, spirvCode, spirvSize, entryPoint, debugName);
    
    free(spirvCode);
    return shader;
}

VEShader* veLoadShader(VEDevice* device, const char* filename, VEShaderStage stage,
                      const char* entryPoint, const char* debugName) {
    if (!device || !filename || !entryPoint) {
        veSetError("Invalid parameters for shader loading");
        return NULL;
    }
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        veSetError("Failed to open shader file: %s", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (fileSize <= 0 || fileSize % 4 != 0) {
        veSetError("Invalid SPIR-V file size: %ld", fileSize);
        fclose(file);
        return NULL;
    }
    
    uint32_t* code = malloc(fileSize);
    if (!code) {
        veSetError("Failed to allocate memory for shader code");
        fclose(file);
        return NULL;
    }
    
    size_t bytesRead = fread(code, 1, fileSize, file);
    fclose(file);
    
    if (bytesRead != fileSize) {
        veSetError("Failed to read entire shader file");
        free(code);
        return NULL;
    }
    
    VEShader* shader = veCreateShaderFromSPIRV(device, stage, code, fileSize, entryPoint, debugName);
    
    if (shader) {
        VEShaderInternal* internal = (VEShaderInternal*)shader;
        strncpy(internal->sourceFile, filename, sizeof(internal->sourceFile) - 1);
        internal->device = (VEDeviceInternal*)device;
    }
    
    free(code);
    return shader;
}

void veDestroyShader(VEShader* shader) {
    if (!shader) return;
    
    VEShaderInternal* internal = (VEShaderInternal*)shader;

    if (internal->shaderObject) {
        veFuncs.vkDestroyShaderEXT(internal->device->device, internal->shaderObject, NULL);
    }
    
    free(internal);
}

// =============================================================================
// Shader Hot-Reload Support
// =============================================================================

VEResult veEnableShaderHotReload(VEShader* shader, const char* sourceFile) {
    if (!shader || !sourceFile) {
        veSetError("Invalid parameters for shader hot-reload");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEShaderInternal* internal = (VEShaderInternal*)shader;
    
    strncpy(internal->sourceFile, sourceFile, sizeof(internal->sourceFile) - 1);
    internal->hotReloadEnabled = true;
    
    return VE_SUCCESS;
}

VEResult veReloadShader(VEShader* shader) {
    if (!shader) {
        veSetError("Shader cannot be NULL");
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    VEShaderInternal* internal = (VEShaderInternal*)shader;
    
    if (!internal->hotReloadEnabled || !internal->sourceFile[0]) {
        veSetError("Shader hot-reload not enabled or no source file specified");
        return VE_ERROR_FEATURE_NOT_SUPPORTED;
    }
    
    // TODO: Implement shader reloading
    veSetError("Shader hot-reload not implemented");
    return VE_ERROR_FEATURE_NOT_SUPPORTED;
}

// =============================================================================
// Shader Configuration Management  
// =============================================================================

VEShaderConfig* veCreateShaderConfig(VEDevice* device, const VEShaderConfigDesc* desc) {
    if (!device || !desc) {
        veSetError("Invalid parameters for shader config creation");
        return NULL;
    }
    
    VEDeviceInternal* deviceInternal = (VEDeviceInternal*)device;
    
    // Find free slot in shader configs array
    uint32_t index = UINT32_MAX;
    for (uint32_t i = 0; i < deviceInternal->maxShaderConfigs; i++) {
        if (!deviceInternal->shaderConfigs[i].isValid) {
            index = i;
            break;
        }
    }
    
    if (index == UINT32_MAX) {
        veSetError("No free shader config slots available");
        return NULL;
    }
    
    VEShaderConfigInternal* config = &deviceInternal->shaderConfigs[index];
    memset(config, 0, sizeof(VEShaderConfigInternal));
    
    config->vertexShader = desc->vertexShader;
    config->fragmentShader = desc->fragmentShader;
    config->geometryShader = desc->geometryShader;
    config->tessControlShader = desc->tessControlShader;
    config->tessEvalShader = desc->tessEvalShader;
    config->computeShader = desc->computeShader;
    
    if (desc->debugName) {
        strncpy(config->debugName, desc->debugName, sizeof(config->debugName) - 1);
    } else {
        snprintf(config->debugName, sizeof(config->debugName), "ShaderConfig_%u", index);
    }
    
    config->device = device;
    config->isValid = true;
    deviceInternal->shaderConfigCount++;
    
    return (VEShaderConfig*)config;
}

void veDestroyShaderConfig(VEShaderConfig* config) {
    if (!config) return;
    
    VEShaderConfigInternal* internal = (VEShaderConfigInternal*)config;
    
    internal->device->shaderConfigCount--;
    // Note: We don't destroy the individual shaders here as they might be used elsewhere
    // The user is responsible for managing shader lifetimes
    
    memset(internal, 0, sizeof(VEShaderConfigInternal));

}