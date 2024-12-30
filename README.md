# vulkan_backend

Vulkan library with focus on:
- **Performance**
- **Ease of use**
- **Speed of development**

It is designed to eliminate the need to write and debug Vulkan code while keeping ability to leverage full explicitness of the Vulkan API. Users can focus on creating high-performance, low-level graphics applications without getting bogged down by Vulkan's setup and management processes. 

## Features:
- Familiar Vulkan-like interface
- Automatic physical device and queue selection including separate transfer and compute queues if requested.
- Runtime shader compilation / loading from cache and pipeline creation in a single `CreatePipeline` function.
- Use of `VK_EXT_graphics_pipeline_library` if available for fast pipeline loading from precompiled parts with shader stage hashing from description.
- Fully automatic and efficient resource management based on smart pointers with custom deleters. **Zero** memory leaks or double free possible.
- Customizable bindless descriptors that are updated on resource creation.
- Easy integration with one header or using C++20 [named module](https://en.cppreference.com/w/cpp/language/modules) `import vulkan_backend;`
- No exceptions or unnecessary C++ features overuse.

## Examples
You can find examples in `examples/` directory of this repository. Also look at the project that uses this library: [NRay](https://github.com/noir-nik/NRay)

## Requirements
- Vukan SDK
- Compiler with C++20 support. It is possible to tell the library to use C++23 std module by defining VB_USE_STD_MODULE=1

### Dependencies:
This library only depends on [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) for buffer and image creation.

## Installation
1. Clone the Repository:

```sh
git clone https://github.com/noir-nik/vulkan_backend.git
cd vulkan_backend
git submodule update --init --recursive
```

2. Build the Library:
```sh
mkdir build
cd build
cmake ..
cmake --build . -j8
```

Using CMake vulkan_backend can be added to the project like most other libraries.

## License

The vulkan_backend library is licensed under the MIT License.
