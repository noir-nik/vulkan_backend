#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#ifndef VB_BUILD_CPP_MODULE
#include <vulkan_backend/core.hpp>
#else
import vulkan_backend;
#endif

int constexpr kBindingBufferCommon		 = 0;
int constexpr kMaxDescriptorBindingsCommon = 128;

struct TestContext {
	vb::Instance		   instance;
	vb::PhysicalDevice	   physical_device;
	vb::Device			   device;
	vb::BindlessDescriptor bindless_descriptor;
	vb::Queue const&	   queue;
	vk::PipelineLayout	   bindless_pipeline_layout;

	TestContext()
		: instance({.optional_layers = {{vb::kValidationLayerName}}}),
		  physical_device(
			  instance.GetPhysicalDevices()[0]),
		  device(instance, physical_device,
				 {.queues = {{{.queueFamilyIndex = physical_device.FindQueueFamilyIndex(
								   {.flags = vk::QueueFlagBits::eCompute}),
							   .queueCount = 1}}}}),
		  bindless_descriptor(device, {.bindings = {{{.binding		   = kBindingBufferCommon,
													  .descriptorType  = vk::DescriptorType::eStorageBuffer,
													  .descriptorCount = kMaxDescriptorBindingsCommon,
													  .stageFlags = vk::ShaderStageFlagBits::eCompute}}}}),
		  queue(*device.GetQueue({{vk::QueueFlagBits::eCompute}})),
		  bindless_pipeline_layout(device.GetOrCreatePipelineLayout({
			  .descriptor_set_layouts = {{bindless_descriptor.layout}},
			  .push_constant_ranges	  = {{{
					.stageFlags = vk::ShaderStageFlagBits::eAll,
					.offset		= 0,
					.size		= physical_device.GetProperties2().properties.limits.maxPushConstantsSize,
				}}},
		  })) {}
};

// Alternatively:
/* 
vb::Instance instance({.optional_layers = {{vb::kValidationLayerName}}});
vb::PhysicalDevice* physical_device(instance.SelectPhysicalDevice({.queues = {{{.flags = vk::QueueFlagBits::eCompute}}}}));
if (!physical_device) return 1;
vb::Device device(instance, physical_device, {.queues = {{{.queueFamilyIndex = physical_device->FindQueueFamilyIndex({.flags = vk::QueueFlagBits::eCompute}), .queueCount = 1}}}});
vb::BindlessDescriptor bindless_descriptor(device, {.bindings = {{{.binding = kBindingBuffer, .descriptorType = vk::DescriptorType::eStorageBuffer, .descriptorCount = kMaxDescriptorBindings, .stageFlags = vk::ShaderStageFlagBits::eCompute}}}});
vb::Queue const& queue(*device.GetQueue(vk::QueueFlagBits::eCompute));
vk::PipelineLayout bindless_pipeline_layout(device.GetOrCreatePipelineLayout({.descriptor_set_layouts = {{bindless_descriptor.layout}}, .push_constant_ranges = {{{.stageFlags = vk::ShaderStageFlagBits::eAll, .offset = 0, .size = physical_device->GetProperties2().properties.limits.maxPushConstantsSize,}}}}));
 */