/**
 * @file ve_debug_stub.c
 * @brief Debug function stubs for VulkEase
 * 
 * Provides minimal stub implementations for debug functions
 * to allow the library to compile successfully.
 */

#include "ve_internal.h"

// =============================================================================
// Debug Label Functions (Stubs)
// =============================================================================

void veBeginDebugLabel(VECommandBuffer* cmd, const char* label, VEColor color) {
    (void)cmd; (void)label; (void)color;
    // Stub implementation - no debug labels in minimal build
}

void veEndDebugLabel(VECommandBuffer* cmd) {
    (void)cmd;
    // Stub implementation
}

void veInsertDebugLabel(VECommandBuffer* cmd, const char* label, VEColor color) {
    (void)cmd; (void)label; (void)color;
    // Stub implementation
}

// =============================================================================
// Debug Name Functions (Stubs)
// =============================================================================

VEResult veSetBufferDebugName(VEDevice* device, VEBufferAddress address, const char* name) {
    (void)device; (void)address; (void)name;
    return VE_SUCCESS;
}

VEResult veSetTextureDebugName(VEDevice* device, VETextureIndex texture, const char* name) {
    (void)device; (void)texture; (void)name;
    return VE_SUCCESS;
}

VEResult veSetSamplerDebugName(VEDevice* device, VESamplerIndex sampler, const char* name) {
    (void)device; (void)sampler; (void)name;
    return VE_SUCCESS;
}

// =============================================================================
// Performance and Memory Statistics (Stubs)
// =============================================================================

VEResult veGetPerformanceStats(VEDevice* device, VEPerformanceStats* stats) {
    if (!device || !stats) {
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    // Return empty stats
    memset(stats, 0, sizeof(VEPerformanceStats));
    return VE_SUCCESS;
}

VEResult veGetMemoryStats(VEDevice* device, VEMemoryStats* stats) {
    if (!device || !stats) {
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    // Return empty stats
    memset(stats, 0, sizeof(VEMemoryStats));
    return VE_SUCCESS;
}

VEResult veGetRenderConfigStats(VEDevice* device, VERenderConfigStats* stats) {
    if (!device || !stats) {
        return VE_ERROR_INVALID_PARAMETER;
    }
    
    // Return empty stats
    memset(stats, 0, sizeof(VERenderConfigStats));
    return VE_SUCCESS;
}

// =============================================================================
// Debug Information (Stubs)
// =============================================================================

void vePrintDebugInfo(VEDevice* device) {
    (void)device;
    printf("VulkEase Debug Info: Stub implementation\n");
}

void vePrintRenderConfig(VERenderConfig* config) {
    (void)config;
    printf("VulkEase Render Config: Stub implementation\n");
}

VEResult veValidateRenderConfig(VERenderConfig* config) {
    (void)config;
    return VE_SUCCESS;
}
