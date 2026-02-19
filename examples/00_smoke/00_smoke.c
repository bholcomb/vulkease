// Minimal smoke test for VulkEase public API.
// Goals:
// - Create context/device
// - Install message callback + verify invalid-input errors are emitted
// - Create buffer/texture and exercise escape hatches
// - Exercise veBarrier + command submission
// - Exercise VK_EXT_host_image_copy host write/read (if supported)

#include "vulkease.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MSVC C compiler doesn't fully support C11 atomics - use platform-specific alternatives
#ifdef _MSC_VER
#include <windows.h>
static volatile LONG g_errorMessages = 0;
#define ATOMIC_FETCH_ADD(ptr, val) InterlockedExchangeAdd((ptr), (val))
#define ATOMIC_LOAD(ptr) InterlockedCompareExchange((ptr), 0, 0)
#define ATOMIC_STORE(ptr, val) InterlockedExchange((ptr), (val))
#else
#include <stdatomic.h>
static atomic_uint g_errorMessages = 0;
#define ATOMIC_FETCH_ADD(ptr, val) atomic_fetch_add((ptr), (val))
#define ATOMIC_LOAD(ptr) atomic_load((ptr))
#define ATOMIC_STORE(ptr, val) atomic_store((ptr), (val))
#endif

static void onMessage(VEMessageSeverity severity, const char *message, void *userData)
{
   (void)userData;
   if (!message)
      return;
   if (severity == VE_MESSAGE_SEVERITY_ERROR)
      ATOMIC_FETCH_ADD(&g_errorMessages, 1);
   fprintf(stderr, "[smoke] severity=%d msg=%s\n", (int)severity, message);
}

static int fail(const char *what, VEResult r)
{
   fprintf(stderr, "[smoke] FAIL: %s (VEResult=%s)\n", what, veResultToString(r));
   return 1;
}

int main(void)
{
   VEContext *context = NULL;
   VEDevice *device = NULL;

   // Install callback as early as possible (context must exist first).
   VEResult result = veCreateContext("VulkEase Smoke", NULL, 0, &context);
   if (result != VE_SUCCESS || !context)
      return fail("veCreateContext", result);

   VEMessageCallbackDesc cb = {.callback = onMessage, .userData = NULL};
   result = veSetMessageCallback(&cb);
   if (result != VE_SUCCESS)
      return fail("veSetMessageCallback", result);
   (void)veSetMinMessageSeverity(VE_MESSAGE_SEVERITY_VERBOSE);

   result = veCreateDevice(context, VK_NULL_HANDLE, NULL, 0, &device);
   if (result != VE_SUCCESS || !device)
      return fail("veCreateDevice", result);

   // Buffer create + escape hatch
   VEBufferAddress addr = VE_INVALID_ADDRESS;
   const uint32_t initWords[16] = {0xDEADBEEF, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
   VEBufferDesc bdesc = {
       .size = sizeof(initWords),
       .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
       .initialData = initWords,
       .initialDataSize = sizeof(initWords),
       .persistentlyMapped = false,
       .debugName = "smoke_buffer",
   };
   result = veCreateBuffer(device, &bdesc, &addr);
   if (result != VE_SUCCESS || addr == VE_INVALID_ADDRESS)
      return fail("veCreateBuffer", result);

   VkBuffer vkBuf = veGetVkBufferFromAddress(device, addr);
   if (vkBuf == VK_NULL_HANDLE)
   {
      fprintf(stderr, "[smoke] FAIL: veGetVkBufferFromAddress returned VK_NULL_HANDLE\n");
      return 1;
   }

   fprintf(stderr, "[smoke] Intentionally failing veGetBufferSize and veGetSwapchain\n");
   // Verify invalid-input paths emit ERROR messages now (sentinel return remains).
   (void)veGetBufferSize(NULL, addr);
   (void)veGetSwapchainSize(NULL);
   if (ATOMIC_LOAD(&g_errorMessages) == 0)
   {
      fprintf(stderr, "[smoke] FAIL: expected at least one ERROR message from invalid input\n");
      return 1;
   }
   // Those errors were intentional; reset so we can detect unexpected errors later.
   ATOMIC_STORE(&g_errorMessages, 0);
   

   // Texture create + escape hatches + host copy
   VETextureIndex tex = VE_INVALID_TEXTURE_INDEX;
#define SMOKE_TEX_W 4
#define SMOKE_TEX_H 4
   const uint32_t w = SMOKE_TEX_W, h = SMOKE_TEX_H;
   uint8_t pixels[SMOKE_TEX_W * SMOKE_TEX_H * 4];
   for (uint32_t i = 0; i < w * h; ++i)
   {
      pixels[i * 4 + 0] = (uint8_t)(i);
      pixels[i * 4 + 1] = (uint8_t)(i ^ 0x55);
      pixels[i * 4 + 2] = (uint8_t)(i ^ 0xAA);
      pixels[i * 4 + 3] = 255;
   }

   VETextureDesc tdesc = {
       .width = w,
       .height = h,
       .depth = 1,
       .mipLevels = 1,
       .arrayLayers = 1,
       .format = VK_FORMAT_R8G8B8A8_UNORM,
       .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_HOST_TRANSFER_BIT_EXT,
       .sampleCount = VK_SAMPLE_COUNT_1_BIT,
       .initialData = NULL,
       .initialDataSize = 0,
       .debugName = "smoke_texture",
   };

   result = veCreateTexture(device, &tdesc, &tex);
   if (result != VE_SUCCESS || tex == VE_INVALID_TEXTURE_INDEX)
      return fail("veCreateTexture", result);

   VkImage vkImage = veGetVkImageFromTexture(device, tex);
   VkImageView vkView = veGetVkImageViewFromTexture(device, tex);
   if (vkImage == VK_NULL_HANDLE || vkView == VK_NULL_HANDLE)
   {
      fprintf(stderr, "[smoke] FAIL: texture escape hatches returned NULL handles\n");
      return 1;
   }

   // Host write/read (may be unsupported depending on device/driver).
   result = veHostWriteTextureRegion(device, tex, pixels, sizeof(pixels), NULL);
   if (result == VE_SUCCESS)
   {
      uint8_t readback[sizeof(pixels)];
      memset(readback, 0, sizeof(readback));
      result = veHostReadTextureRegion(device, tex, readback, sizeof(readback), NULL);
      if (result != VE_SUCCESS)
         return fail("veHostReadTextureRegion", result);
      if (memcmp(pixels, readback, sizeof(pixels)) != 0)
      {
         fprintf(stderr, "[smoke] FAIL: host readback data mismatch\n");
         return 1;
      }
   }
   else if (result != VE_ERROR_FEATURE_NOT_SUPPORTED && result != VE_ERROR_UNSUPPORTED)
   {
      return fail("veHostWriteTextureRegion", result);
   }

   // Barrier + submission
   VECommandBuffer *cmd = NULL;
   result = veBeginCommandBuffer(device, &cmd);
   if (result != VE_SUCCESS || !cmd)
      return fail("veBeginCommandBuffer", result);

   VEMemoryBarrier mem = {
       .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
       .srcAccessMask = 0,
       .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
       .dstAccessMask = 0,
   };
   VEBarrierDesc b = {
       .memoryBarrierCount = 1,
       .memoryBarriers = &mem,
       .bufferBarrierCount = 0,
       .bufferBarriers = NULL,
       .imageBarrierCount = 0,
       .imageBarriers = NULL,
   };
   result = veBarrier(cmd, &b);
   if (result != VE_SUCCESS)
      return fail("veBarrier", result);

   result = veEndCommandBuffer(cmd);
   if (result != VE_SUCCESS)
      return fail("veEndCommandBuffer", result);

   VESubmitInfo si = {0};
   si.waitForCompletion = true;
   result = veSubmitCommandBuffer(cmd, &si);
   if (result != VE_SUCCESS)
      return fail("veSubmitCommandBuffer", result);

   // Cleanup
   (void)veReleaseCommandBuffer(cmd);
   (void)veDestroyTexture(device, tex);
   (void)veDestroyBuffer(device, addr);
   (void)veDestroyDevice(device);
   (void)veDestroyContext(context);

   if (ATOMIC_LOAD(&g_errorMessages) > 0)
   {
      fprintf(stderr, "[smoke] FAIL: expected no ERROR messages\n");
      return 1;
   }
   fprintf(stderr, "[smoke] PASS\n");
   return 0;
}

