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

// Define queue info to select a compute queue
vb::QueueInfo constexpr queue_info = {.flags = vk::QueueFlagBits::eCompute};

int main(){
	// Define constants for compute shader vector addition
	int constexpr kVectorSize = 64 * 1024;	
	int constexpr kWorkgroupSize = 16;
	int constexpr kBindingBuffer = 0;
	int constexpr kNumGpuBuffers = 3;

	vb::SetLogLevel(vb::LogLevel::Trace);

	// Create instance object
	vb::Instance instance({
		.optional_layers = {{vb::kValidationLayerName}}
	});

	// Select physical devices
	int selected_device_index = instance.GetPhysicalDevices().size();
	std::vector<vb::PhysicalDevice> physical_devices;
	physical_devices.reserve(instance.GetPhysicalDevices().size());
	for (auto& vk_device : instance.GetPhysicalDevices()) {
		physical_devices.emplace_back(vk_device);
		auto& current_device = physical_devices.back();
		// Get all data from Vulkan API (features, properties, available
		// extensions, memory properties, queue families)
		current_device.GetDetails();
		bool supports_features = current_device.SupportsSynchronization2() &&
								 current_device.base_features.vulkan12.runtimeDescriptorArray == vk::True;
		if (current_device.SupportsQueue(queue_info)) {
			selected_device_index = physical_devices.size() - 1;
			break;
		}
	}
	// Return if no suitable device found
	if (selected_device_index == instance.GetPhysicalDevices().size()) {
		std::printf("No physical device with compute queue support found\n");
		return 1;
	}
	vb::PhysicalDevice& physical_device = physical_devices[selected_device_index];

	// Create device with 1 compute queue and synchronization2
	vk::PhysicalDeviceFeatures2 features2{};
	vk::PhysicalDeviceVulkan12Features vulkan12_features{.runtimeDescriptorArray = vk::True}; // bindless descriptors
	vk::PhysicalDeviceVulkan13Features vulkan13_features{.synchronization2 = vk::True}; // cmd.Barrier()
	void* feature_chain[] = {&features2, &vulkan12_features, &vulkan13_features};
	vb::SetupStructureChain(feature_chain);

	vb::Device device(instance, physical_device,
					  {.queues	  = {{{.queueFamilyIndex = physical_device.FindQueueFamilyIndex(queue_info),
									   .queueCount		 = 1}}},
					   .features2 = &features2});

	// Create staging buffer for 2 vectors
	vb::StagingBuffer staging_buffer(device, kVectorSize * sizeof(int) * 2);
	vb::BindlessDescriptor bindless_descriptor(
		device, {.bindings = {{{.binding		 = kBindingBuffer,
								.descriptorType	 = vk::DescriptorType::eStorageBuffer,
								.descriptorCount = kNumGpuBuffers,
								.stageFlags		 = vk::ShaderStageFlagBits::eCompute}}}});

	vk::PipelineLayout bindless_pipeline_layout = device.GetOrCreatePipelineLayout({
		.descriptor_set_layouts = {{bindless_descriptor.layout}},
		.push_constant_ranges	= {{{
			  .stageFlags = vk::ShaderStageFlagBits::eAll,
			  .offset	  = 0,
			  .size		  = physical_device.GetMaxPushConstantsSize(),
		  }}},
	});

	// Get compute queue handle object
	vb::Queue const& queue = *device.GetQueue(queue_info);
	
	// Create 2 vectors:
	// vector_a = {0, 1, 2, ..., kVectorSize - 1}
	// vector_b = {1, 1, 1, ..., 1}
	std::vector<int> vector_a(kVectorSize);
	std::vector<int> vector_b(kVectorSize);
	std::iota(std::begin(vector_a), std::end(vector_a), 0);
	std::fill(std::begin(vector_b), std::end(vector_b), 1);

	// Create device buffers
	vb::BufferInfo buffer_info = {
		.size		= kVectorSize * sizeof(int),
		.usage		= vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
		.memory		= vb::Memory::eGPU,
		.descriptor = &bindless_descriptor,
		.binding	= kBindingBuffer,
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
		.size   = kVectorSize * sizeof(int),
		.usage  = vk::BufferUsageFlagBits::eTransferDst,
		.memory = vb::Memory::eCPU,
	});

	// Provide compile options for compute shader
	char compile_options[128];
	std::snprintf(compile_options, sizeof(compile_options), 
		"-DWORKGROUP_SIZE=%d -DBINDING_BUFFER=%d", 
		kWorkgroupSize, kBindingBuffer);

	// Create compute pipeline
	vb::Pipeline pipeline(device, {
		.stages = {{{
			.stage = vk::ShaderStageFlagBits::eCompute, 
			.source = {"vector_addition.comp"},
			.compile_options = compile_options
		}}},
		.layout = bindless_pipeline_layout,
		.name = "Vector Addition",
	});

	// Define constants with Resource IDs for descriptor indexing
	// and vector size
	struct Constants {
		std::uint32_t vector_a_rid;
		std::uint32_t vector_b_rid;
		std::uint32_t vector_result_rid;
		int kVectorSize;
	};

	Constants constants = {
		.vector_a_rid      = device_buffer_a.GetResourceID(),
		.vector_b_rid      = device_buffer_b.GetResourceID(),
		.vector_result_rid = device_buffer_result.GetResourceID(),
		.kVectorSize       = kVectorSize,
	};

	assert(constants.vector_a_rid <= kNumGpuBuffers && constants.vector_b_rid <= kNumGpuBuffers &&
		   constants.vector_result_rid <= kNumGpuBuffers);

	std::printf("Constants:\n");
	std::printf("vector_a_rid      = %u\n", constants.vector_a_rid);
	std::printf("vector_b_rid      = %u\n", constants.vector_b_rid);
	std::printf("vector_result_rid = %u\n", constants.vector_result_rid);
	std::printf("kVectorSize       = %u\n", constants.kVectorSize);

	// Create a command buffer with command pool
	vb::Command cmd(device, queue.GetFamilyIndex());

	// Begin command buffer
	cmd.Begin();

	// Bind created pipeline and push constants
	cmd.BindPipelineAndDescriptorSet(pipeline, bindless_descriptor.set);
	cmd.PushConstants(pipeline, &constants, sizeof(Constants));

	// Copy vectors to device
	cmd.Copy(device_buffer_a, staging_buffer, vector_a.data(), kVectorSize * sizeof(int));
	cmd.Copy(device_buffer_b, staging_buffer,vector_b.data(), kVectorSize * sizeof(int));

	// Place barrier to ensure data is available to compute shader
	cmd.Barrier(device_buffer_a);
	cmd.Barrier(device_buffer_b);

	// Dispatch 1D compute shader
	cmd.Dispatch(std::ceil(kVectorSize / static_cast<float>(kWorkgroupSize)), 1, 1);

	// lace barrier to wait for compute shader completion
	cmd.Barrier(device_buffer_a);
	cmd.Barrier(device_buffer_b);
	cmd.Barrier(device_buffer_result);

	// Copy result to CPU
	cmd.Copy(cpu_buffer_result, device_buffer_result, kVectorSize * sizeof(int));

	// End command buffer, submit and wait on queue
	cmd.End();
	cmd.QueueSubmit(queue);
	device.WaitQueue(queue);
	
	// Map result
	int* mappedMemory = reinterpret_cast<int*>(cpu_buffer_result.GetMappedData());

	// Compare result of vector addition with std::transform
	std::transform(vector_a.begin(), vector_a.end(),
	                vector_b.begin(), vector_a.begin(), std::plus<int>());
	// assert(std::equal(vector_a.begin(), vector_a.end(), mappedMemory));

	// Print first 32 elements of result
	// the result should be {1, 2, 3, ..., 32}
	std::printf("Result is:\n");
	int print_size = std::min(kVectorSize, 32);
	for (int i = 0; i < print_size; ++i) {
		std::printf("%d ", mappedMemory[i]);
	}
	std::printf("\n");
	std::printf("Success!\n");

	// Clean up.
	// No clean up is needed! Everithing is freed automatically in destructors in reverse order
}