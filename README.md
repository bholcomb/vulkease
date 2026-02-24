# VulkEase (Working title...may change it later)

Small abstraction library over the Vulkan API.  This API is fairly opinionated and targets desktop environments that support Vulkan 1.4 with some of the dynamic state extensions.  

A few of the design choices:
   - Minimalistic API.  01_trinagle example uses 29 function calls to setup, run, and shutdown the application
   - Vulkan 1.4+ only.  This is because of the dynamic state features and extensions used
   - Shader Objects only.  No pipelines to setup, instead create a renderstate object and use it apply render settings
   - Bindless resourcees only.  Textures and Buffers accessed directly in shaders
   - No descriptor sets, Push constants for all resource references
   - Public C API, for binding to other languages

This is a work in progress.  The examples should build and run without validation errors, but don't exercise every feature yet.  It still has a bit of AI slop that needs to be cleaned up.

 
