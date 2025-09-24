# Vulkan Multithreading: Functions Requiring External Synchronization Protection

## Overview

When multithreading a Vulkan application, certain Vulkan objects and functions require **external synchronization**, meaning the application must protect them from concurrent access using mechanisms like mutexes, locks, or atomic operations. Vulkan's explicit design philosophy places the responsibility of thread safety squarely on the application developer rather than hiding it within the driver implementation.

## Core Threading Rules

Vulkan follows a fundamental principle: **all commands support being called concurrently from multiple threads**, but certain parameters or objects are marked as "externally synchronized". This means:[1]

- The application **must** guarantee that no more than one thread uses such parameters at any given time[1]
- If two commands access the same object and at least one declares it externally synchronized, the commands cannot execute simultaneously and must be separated by appropriate memory barriers if needed[1]
- Objects not labeled as externally synchronized are either internally synchronized by the implementation or not mutated by the command[1]

## Functions and Objects Requiring External Synchronization

### **VkQueue Operations**
The most critical functions requiring protection are queue operations:

- **`vkQueueSubmit`** and **`vkQueueSubmit2`** - These functions are **not thread-safe**. Multiple threads cannot submit work to the same queue simultaneously[2][3][4]
- **`vkQueuePresentKHR`** - Presentation operations require external synchronization when accessing the same queue[3][2]
- **`vkQueueWaitIdle`** - Must be synchronized with other queue operations[5]

**Solution**: Use a mutex per queue to serialize access:[2][3]
```cpp
std::mutex queueMutex;
// ...
queueMutex.lock();
vkQueueSubmit(queue, ...);
queueMutex.unlock();
```

### **VkCommandPool and Command Buffer Management**
Command pools are **externally synchronized** and represent one of the most important threading considerations:[6][7]

- **`VkCommandPool`** - Cannot be used concurrently across multiple threads[7][6]
- **Command buffer recording** (`vkCmd*` functions) - When recording to command buffers allocated from the same pool[8][6]
- **`vkAllocateCommandBuffers`**, **`vkFreeCommandBuffers`**, **`vkResetCommandPool`** - All pool operations require synchronization[6]

**Best Practice**: Create separate command pools per thread. Each thread should have its own command pool to avoid synchronization overhead.[9][10][7]

### **VkDevice Creation and Destruction**
Device lifecycle functions require careful synchronization:

- **`vkDestroyDevice`** - Must be externally synchronized[11][5]
- **`vkCreateDevice`** - Requires external synchronization in multi-threaded scenarios[12]

Most **`vkCreate*`** functions are generally thread-safe and can be called concurrently, but **`vkDestroy*`** functions typically require external synchronization.[13][10]

### **VkDescriptorPool Operations**
Descriptor pools follow similar rules to command pools:

- **`VkDescriptorPool`** - Externally synchronized for allocation, freeing, and reset operations[14]
- **`vkAllocateDescriptorSets`**, **`vkFreeDescriptorSets`**, **`vkResetDescriptorPool`** - Cannot be called concurrently on the same pool[14]

**Solution**: Use separate descriptor pools per thread or implement proper locking around pool operations.[7]

### **Memory and Resource Management**
While most creation functions are thread-safe, certain memory operations require care:

- **`vkAllocateMemory`**, **`vkFreeMemory`** - Generally thread-safe when called on different objects
- **Buffer and image operations** - Thread-safe for creation but may require synchronization for certain state changes

## Thread-Safe Operations

Several Vulkan operations **do not** require external synchronization and can be safely called from multiple threads:

- Most **`vkCreate*`** functions (pipelines, shaders, buffers, images)[10][13]
- **`vkGetDeviceProcAddr`** - Does not require external synchronization[12]
- **Pipeline compilation** (`vkCreateGraphicsPipeline`, `vkCreateShaderModule`)[15]
- **Memory allocation** when operating on different memory objects

## Best Practices for Multithreaded Vulkan

### **Per-Thread Resource Allocation**
- Create separate **command pools** per thread[9][10][7]
- Use separate **descriptor pools** per thread[7]
- Allocate per-thread command buffers from thread-specific pools[15]

### **Queue Management Strategies**
- Implement a **single submission thread** that collects command buffers from worker threads and submits them[4][15]
- Use **mutex protection** for queue operations when multiple threads need direct access[3][2]
- Consider using separate queues for different operations (graphics, compute, transfer) to reduce contention

### **Synchronization Patterns**
- Use **secondary command buffers** for parallel recording, then execute them from a primary command buffer[16][8]
- Implement **lock-free queues** for passing work between threads[15]
- Employ **proper memory barriers** when sharing data between threads accessing Vulkan objects[1]

## Validation and Debugging

The **Khronos validation layers** include thread-safety validation that can detect violations of external synchronization requirements. Enable `VK_LAYER_KHRONOS_validation` during development to catch threading errors, though it may not detect all race conditions due to timing dependencies.[5][11]

## Conclusion

Successful multithreading in Vulkan requires careful attention to which objects and functions require external synchronization. The key principle is that **queue operations, command pools, and descriptor pools** are the primary areas requiring protection, while most creation operations are inherently thread-safe. By following the pattern of per-thread resource allocation and proper synchronization of shared resources, applications can achieve significant performance benefits from Vulkan's explicit multithreading model.[17][9]

[1](https://docs.vulkan.org/spec/latest/chapters/fundamentals.html)
[2](https://www.reddit.com/r/vulkan/comments/7ravmb/threading_error_through_concurrent_access_to/)
[3](https://www.reddit.com/r/vulkan/comments/umgs26/synchronize_queue_submission/)
[4](https://www.vkguide.dev/docs/new_chapter_1/vulkan_command_flow/)
[5](https://community.khronos.org/t/not-very-clear-about-the-host-synchronization-parts-in-spec/108404)
[6](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html)
[7](https://docs.vulkan.org/guide/latest/threading.html)
[8](https://www.reddit.com/r/vulkan/comments/5cv8k3/building_a_command_buffer_on_different_threads/)
[9](https://community.arm.com/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/multi-threading-in-vulkan)
[10](https://www.reddit.com/r/gameenginedevs/comments/1m03v9d/where_and_how_can_i_take_advantage_of/)
[11](https://developer.nvidia.com/docs/drive/drive-os/6.0.10/public/drive-os-linux-sdk/common/topics/graphics_content/vulkan_sc_validation_layer.html)
[12](https://github.com/KhronosGroup/Vulkan-Loader/issues/200)
[13](https://stackoverflow.com/questions/51528553/can-i-use-vkdevice-from-multiple-threads-concurrently)
[14](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html)
[15](https://www.vkguide.dev/docs/extra-chapter/multithreading/)
[16](https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html)
[17](https://blog.imaginationtech.com/vulkan-scaling-to-multiple-threads/)
[18](https://docs.vulkan.org/tutorial/latest/17_Multithreading.html)
[19](https://stackoverflow.com/questions/48081352/vulkan-queue-synchronization-in-multithreading)
[20](https://community.khronos.org/t/official-vulkan-feedback-api-for-high-efficiency-graphics-and-compute-on-gpus/4938?page=13)
[21](https://www.reddit.com/r/vulkan/comments/52aodq/multithreading_in_vulkan_where_should_i_start/)
[22](https://vulkan.org/user/pages/09.events/vulkanised-2024/vulkanised-2024-grigory-dzhavadyan.pdf)
[23](https://themaister.net/blog/2019/08/14/yet-another-blog-explaining-vulkan-synchronization/)
[24](https://community.khronos.org/t/multi-threaded-rendering-how-do-you-actually-use-it/105292)
[25](https://community.arm.com/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/vulkan-mobile-best-practices-and-management)
[26](https://www.reddit.com/r/starcitizen/comments/1ge1zc5/vulkan_multithreading_already_working_in_cigs_dev/)
[27](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/general_considerations.html)
[28](https://community.khronos.org/t/is-vkacquirenextimagekhr-thread-safe/109741)
[29](https://www.lunarg.com/wp-content/uploads/2024/02/Guide-to-Vulkan-Synchronization-Validation-LunarG-John-Zulauf-02-01-2024.pdf)
[30](https://www.sctheblog.com/blog/vulkan-synchronization/)
[31](https://groups.google.com/g/angleproject/c/LVenRGBHCJM)
[32](https://stackoverflow.com/questions/76476077/vulkan-parameters-labeled-marked-as-externally-synchronized)
[33](https://www.reddit.com/r/vulkan/comments/1cpp63d/host_synchronization_when_not_specified_as/)
[34](https://www.vkguide.dev/docs/chapter-1/vulkan_command_flow/)
[35](https://community.khronos.org/t/parallel-execution-of-multiple-command-buffers-in-a-single-queue-family/107427)
[36](https://stackoverflow.com/questions/49920858/how-to-share-buffer-or-image-between-multiple-vkdevices)
[37](https://docs.vulkan.org/spec/latest/chapters/synchronization.html)
[38](https://www.vkguide.dev/docs/new_chapter_1/vulkan_commands_code/)
[39](https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Command_buffers)
[40](https://stackoverflow.com/questions/38318818/multi-thread-rendering-vs-command-pools)
[41](https://vulkan.lunarg.com/doc/view/1.3.283.0/windows/1.3-extensions/vkspec.html)
[42](https://www.vkguide.dev/docs/introduction/vulkan_execution/)
[43](https://www.scribd.com/document/754649809/vkspec)
[44](https://vulkan.lunarg.com/doc/view/1.3.268.0/mac/1.3/vkspec.html)
[45](https://developer.nvidia.com/blog/vulkan-dos-donts/)
[46](https://devdocs.io/vulkan/)
[47](https://vkdoc.net/chapters/fundamentals)
[48](https://stackoverflow.com/questions/75834717/do-vkqueuepresentkhr-and-vkqueuesubmit-behave-the-same-way-synchronisation-wise)
[49](https://vulkan.lunarg.com/doc/view/1.4.313.0/windows/antora/spec/latest/chapters/fundamentals.html)
[50](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html)
[51](https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/6177)
[52](https://stackoverflow.com/questions/79530721/is-secondary-vkcommandbuffer-needs-to-come-from-the-same-pool-as-the-primary-vkc)
[53](https://community.khronos.org/t/vkqueuepresentkhr-synchronization-ambiguity/7404)
[54](https://vulkan.lunarg.com/doc/view/1.4.304.1/windows/antora/spec/latest/chapters/cmdbuffers.html)
[55](https://www.kdab.com/synchronization-in-vulkan/)