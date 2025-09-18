# VulkEase 2.0 Examples

This collection demonstrates the key features of VulkEase 2.0, a modern bindless graphics API built on Vulkan 1.3+.

## Examples Overview

### 1. **01_triangle.c** - Basic Triangle Rendering
**Features demonstrated:**
- Basic VulkEase setup with GLFW
- Vertex buffer creation and usage via buffer device address
- Simple shader binding and rendering
- Debug regions and error handling

**Key concepts:**
- All vertex data loaded into buffers (no hardcoded vertices in shaders)
- Buffer device addresses passed via push constants
- Modern dynamic rendering (no render passes)

### 2. **02_textured_quad.c** - Textured Quad with Index Buffer  
**Features demonstrated:**
- Indexed rendering with index buffers
- Texture loading with STB Image integration
- Uniform buffers with persistent mapping
- Matrix transformations and animation
- Sampler creation and usage

**Key concepts:**
- Bindless textures accessed by index
- Automatic mipmap generation
- Uniform buffer updates with persistent mapping
- Procedural texture fallback if file loading fails

### 3. **03_compute_particles.c** - GPU Particle System
**Features demonstrated:**
- Compute shader dispatch and synchronization
- Storage buffers for particle data
- GPU-driven rendering with instancing
- Compute-graphics pipeline barriers
- Interactive mouse-based attraction

**Key concepts:**
- 10,000 particles simulated entirely on GPU
- Compute workgroup size calculation
- Storage buffer access from both compute and graphics shaders
- Real-time performance statistics

### 4. **04_deferred_rendering.c** - Multi-Pass Deferred Rendering
**Features demonstrated:**
- Multiple render targets (G-buffer)
- Depth buffer usage
- Multi-pass rendering pipeline
- Deferred shading techniques
- Resource management and cleanup

**Key concepts:**
- G-buffer layout: Albedo+Metallic, Normal+Roughness, Depth
- Geometry pass fills G-buffer
- Lighting pass samples G-buffer for final output
- Multiple render configurations for different passes

## Building the Examples

### Prerequisites

**System Requirements:**
- Vulkan 1.3+ capable GPU and drivers
- Modern C compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.15+

**Dependencies:**
- GLFW 3.3+ (window management)
- VulkEase 2.0 library (build separately)
- Vulkan SDK with glslc shader compiler

**Install dependencies on Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake pkg-config
sudo apt install libglfw3-dev libvulkan-dev vulkan-tools
sudo apt install glslc  # or vulkan-sdk
```

**Install dependencies on macOS:**
```bash
brew install cmake glfw vulkan-headers vulkan-loader
# Install Vulkan SDK from LunarG for glslc
```

**Install dependencies on Windows:**
```powershell
# Install Vulkan SDK from LunarG
# Install GLFW via vcpkg or build from source
vcpkg install glfw3:x64-windows
```

### Build Steps

1. **Clone and build VulkEase 2.0 first:**
```bash
git clone https://github.com/your-org/vulkease2.git
cd vulkease2
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
sudo make install  # or copy lib/libvulkease2.* to examples/lib/
```

2. **Build the examples:**
```bash
cd vulkease-examples
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

3. **Run an example:**
```bash
./01_triangle
```

### Project Structure
```
vulkease-examples/
├── 01_triangle.c              # Basic triangle example
├── 02_textured_quad.c         # Textured quad with matrices
├── 03_compute_particles.c     # GPU particle system  
├── 04_deferred_rendering.c    # Multi-pass deferred rendering
├── CMakeLists.txt             # Build configuration
├── shaders/                   # GLSL shader source files
│   ├── triangle.vert
│   ├── triangle.frag
│   ├── textured_quad.vert
│   ├── textured_quad.frag
│   ├── particles.comp
│   ├── particles.vert
│   ├── particles.frag
│   ├── deferred_geometry.vert
│   ├── deferred_geometry.frag
│   ├── deferred_lighting.vert
│   └── deferred_lighting.frag
├── textures/                  # Sample texture files
│   └── sample.jpg
├── include/                   # Header files
│   └── vulkease.h
└── lib/                       # VulkEase library files
    └── libvulkease2.so
```

## Running the Examples

### Controls
- **All examples:** ESC to quit, window resizing supported
- **03_compute_particles:** Move mouse to attract particles
- **02_textured_quad:** Automatic rotation animation
- **04_deferred_rendering:** Automatic cube rotation with lighting

### Expected Output

**01_triangle:**
```
VulkEase Version: 2.0.0
Device: NVIDIA GeForce RTX 4080
Driver: 535.104.05
Created vertex buffer at address: 0x12a4000000
Starting render loop...
```

**02_textured_quad:**  
```
Device: NVIDIA GeForce RTX 4080
Created buffers: VB=0x12a4000000, IB=0x12a4001000, UB=0x12a4002000
Loaded texture from file
Created texture (index 1) and sampler (index 0)
Starting render loop...
```

**03_compute_particles:**
```
Device: NVIDIA GeForce RTX 4080
Max compute workgroup size: 1024x1024x64
Optimal workgroup size: 128
Created particle buffer (0x12a4000000) for 10000 particles
Starting particle simulation with 10000 particles...
Move mouse to attract particles
FPS: 165.3 | Frame: 6.04ms | Buffers: 2 | Memory: 42MB
```

### Troubleshooting

**Common Issues:**

1. **"VulkEase library not found"**
   - Ensure libvulkease2 is built and in `lib/` directory
   - Or install VulkEase system-wide

2. **"Failed to create VulkEase context"**
   - Update GPU drivers to support Vulkan 1.3
   - Check that VK_EXT_shader_object is supported

3. **"Failed to load shader"**
   - Shaders need to be compiled to SPIR-V first
   - Run `make shaders` or check shader compilation in CMake

4. **Black screen or no rendering**
   - Check validation layers output for errors
   - Ensure vertex/index data is correctly formatted
   - Verify buffer addresses are valid

**Debug Build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
# Run with validation layers enabled
VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d ./01_triangle
```

**Performance Profiling:**
```bash
# Using RenderDoc (if installed)
renderdoc ./03_compute_particles

# Using GPU profiler
nsight-graphics ./04_deferred_rendering
```

## Key VulkEase 2.0 Features Highlighted

### 🚀 **Modern Vulkan 1.3+ Features**
- **Buffer device address:** Direct GPU memory access without descriptors
- **Dynamic rendering:** No render pass objects needed
- **Shader objects:** No pipeline creation overhead  
- **Extended dynamic state:** Runtime state changes

### 🎯 **Bindless Resource Access**
- **Texture indexing:** Access any texture by index
- **Sampler indexing:** Bind samplers by index
- **Unlimited resources:** No descriptor set limitations

### ⚡ **Performance Optimizations**
- **Zero-binding draws:** All resources via addresses/indices
- **GPU-driven rendering:** Indirect draws and compute dispatch
- **Persistent buffer mapping:** Minimize CPU-GPU synchronization
- **Automatic memory management:** VMA integration

### 🛠 **Developer Experience**
- **Simple API:** Complex Vulkan features made easy
- **Automatic validation:** Built-in error checking and reporting
- **Debug integration:** RenderDoc, Nsight Graphics support
- **Hot reload:** Shader reloading during development

## Next Steps

After running these examples, consider:

1. **Modify the shaders** to experiment with different effects
2. **Add new geometry** by creating more complex vertex data
3. **Implement post-processing** using additional compute shaders
4. **Profile performance** using GPU profiling tools
5. **Create your own renderer** using VulkEase 2.0 as the foundation

## Resources

- **VulkEase 2.0 Documentation:** [docs/](../docs/)
- **Vulkan 1.3 Specification:** https://www.khronos.org/vulkan/
- **GLSL Language Specification:** https://www.khronos.org/opengl/wiki/GLSL
- **Vulkan Samples:** https://github.com/KhronosGroup/Vulkan-Samples

---

**VulkEase 2.0** - Making Modern Graphics Programming Accessible  
© 2025 VulkEase Project