#ifndef VB_USE_STD_MODULE
#include <algorithm>
#include <vector>
#include <numeric>
#include <cmath>
#include <cassert>
#include <cstdio>
#else
import std;
#endif

#include <cassert>

#ifndef VB_BUILD_CPP_MODULE
#include <vulkan_backend/core.hpp>
#else
import vulkan_backend;
#endif

int main(){
	// Define constants for compute shader vector addition
	int constexpr vector_size = 64 * 1024;	
	int constexpr workgroup_size = 16;
	int constexpr binding_buffer = vb::defaults::kBindingStorageBuffer;

	vb::SetLogLevel(vb::LogLevel::Trace);

	// Create instance object
	auto instance = vb::Instance::Create({
		.optional_layers = {{vb::kValidationLayerName}}
	});
	
	// Create device with 1 compute queue and staging buffer for 2 vectors
	std::shared_ptr<vb::Device> device = instance->CreateDevice(instance->SelectPhysicalDevice({}), {
		.queues = {{{vk::QueueFlagBits::eCompute}}},
		.staging_buffer_size = vector_size * sizeof(int) * 2,
	});

	// Get compute queue handle object
	vb::Queue& queue = *device->GetQueue({vk::QueueFlagBits::eCompute});
	
	// Create 2 vectors:
	// vector_a = {0, 1, 2, ..., vector_size - 1}
	// vector_b = {1, 1, 1, ..., 1}
	std::vector<int> vector_a(vector_size);
	std::vector<int> vector_b(vector_size);
	std::iota(std::begin(vector_a), std::end(vector_a), 0);
	std::fill(std::begin(vector_b), std::end(vector_b), 1);

	// Create device buffers
	vb::BufferInfo buffer_info = {
		.size = vector_size * sizeof(int),
		.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
		.memory = vb::Memory::GPU,
	};

	// Create Buffer for vector_a and vector_b
	vb::Buffer device_buffer_a(device, buffer_info);
	vb::Buffer device_buffer_b(device, buffer_info);

	// Create Buffer for result of vector addition
	buffer_info.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc;
	vb::Buffer device_buffer_result(device, buffer_info);

	// Create Buffer for transfering result to CPU,
	// we could also use staging for this
	vb::Buffer cpu_buffer_result(device, {
		.size = vector_size * sizeof(int),
		.usage = vk::BufferUsageFlagBits::eTransferDst,
		.memory = vb::Memory::CPU,
	});

	// Provide compile options for compute shader
	char compile_options[128];
	std::snprintf(compile_options, sizeof(compile_options), 
		"-DWORKGROUP_SIZE=%d -DBINDING_BUFFER=%d", 
		workgroup_size, binding_buffer);

	// Create compute pipeline
	vb::Pipeline pipeline(device, {
		.stages = {{{
			.stage = vk::ShaderStageFlagBits::eCompute, 
			.source = {"vector_addition.comp"},
			.compile_options = compile_options
		}}},
		.name = "Vector Addition",
	});

	// Define constants with Resource IDs for descriptor indexing
	// and vector size
	struct Constants {
		std::uint32_t vector_a_RID;
		std::uint32_t vector_b_RID;
		std::uint32_t vector_result_RID;
		int vector_size;
	};

	Constants constants = {
		.vector_a_RID      = device_buffer_a.GetResourceID(),
		.vector_b_RID      = device_buffer_b.GetResourceID(),
		.vector_result_RID = device_buffer_result.GetResourceID(),
		.vector_size       = vector_size,
	};

	// Create a command buffer with command pool
	vb::Command cmd(device, queue.GetFamilyIndex());

	// Begin command buffer
	cmd.Begin();

	// Bind created pipeline and push constants
	cmd.BindPipeline(pipeline);
	cmd.PushConstants(pipeline, &constants, sizeof(Constants));

	// // Copy vectors to device
	cmd.Copy(device_buffer_a, vector_a.data(), vector_size * sizeof(int));
	// cmd.Copy(device_buffer_b, vector_b.data(), vector_size * sizeof(int));

	// // Place barrier to ensure data is available to compute shader
	// cmd.Barrier(device_buffer_a);
	// cmd.Barrier(device_buffer_b);

	// // Dispatch 1D compute shader
	// cmd.Dispatch(std::ceil(vector_size / static_cast<float>(workgroup_size)), 1, 1);

	// // Place barrier to wait for compute shader completion
	// cmd.Barrier(device_buffer_a);
	// cmd.Barrier(device_buffer_b);
	// cmd.Barrier(device_buffer_result);

	// // Copy result to CPU
	// cmd.Copy(cpu_buffer_result, device_buffer_result, vector_size * sizeof(int));

	// End command buffer, submit and wait on queue
	cmd.End();
	// cmd.QueueSubmit(queue);
	device->WaitQueue(queue);
	
	// Map result
	int* mappedMemory = reinterpret_cast<int*>(cpu_buffer_result.GetMappedData());

	// Compare result of vector addition with std::transform
	std::transform(vector_a.begin(), vector_a.end(),
	                vector_b.begin(), vector_a.begin(), std::plus<int>());
	// assert(std::equal(vector_a.begin(), vector_a.end(), mappedMemory));

	// Print first 32 elements of result
	// the result should be {1, 2, 3, ..., 32}
	std::printf("Result is:\n");
	int print_size = std::min(vector_size, 32);
	for (int i = 0; i < print_size; ++i) {
		std::printf("%d ", mappedMemory[i]);
	}
	std::printf("\n");
	std::printf("Success!\n");

	// Clean up.
	// No clean up is needed! Everithing is freed automatically with custom deleters
}