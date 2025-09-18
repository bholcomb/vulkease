/**
 * @file ve_vma_impl.cpp
 * @brief VMA (Vulkan Memory Allocator) Implementation
 * 
 * This file provides the VMA implementation in C++ to satisfy its requirements,
 * while the rest of VulkEase remains in C.
 */

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "../external/vk_mem_alloc.h"
