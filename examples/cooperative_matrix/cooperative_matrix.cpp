#ifndef VB_USE_STD_MODULE
#else
import std;
#endif

#ifndef VB_BUILD_CPP_MODULE
#include <vulkan_backend/core.hpp>
#else
import vulkan_backend;
#endif

#include "util.hpp"

int constexpr kBindingBuffer		 = 0;
int constexpr kMaxDescriptorBindings = 128;
char constexpr const* extensions[]	 = {vk::KHRCooperativeMatrixExtensionName,
										vk::NVCooperativeMatrix2ExtensionName};
vb::QueueInfo constexpr queue_info = {.flags = vk::QueueFlagBits::eCompute};

struct PhysicalDeviceExt : public vb::PhysicalDevice {
	vk::PhysicalDeviceCooperativeMatrixFeaturesKHR cooperative_matrix_features{};
	vk::PhysicalDeviceCooperativeMatrixPropertiesKHR cooperative_matrix_properties{};
	vk::PhysicalDeviceCooperativeMatrix2FeaturesNV cooperative_matrix_2_features{};
	vk::PhysicalDeviceCooperativeMatrix2PropertiesNV cooperative_matrix_properties2{};

	PhysicalDeviceExt(vk::PhysicalDevice const& physical_device) : PhysicalDevice(physical_device) {
		// Link feature chains for GetDetails()
		GetFeatureChain().LinkNextStructure(&cooperative_matrix_features);
		GetFeatureChain().LinkNextStructure(&cooperative_matrix_2_features);
	}

	bool SupportsCooperativeMatrixFeatures() const {
		return cooperative_matrix_features.cooperativeMatrix == vk::True;
	}

	bool SupportsCooperativeMatrix2Features() const {
		return cooperative_matrix_2_features.cooperativeMatrixWorkgroupScope == vk::True &&
			   cooperative_matrix_2_features.cooperativeMatrixFlexibleDimensions == vk::True &&
			   cooperative_matrix_2_features.cooperativeMatrixTensorAddressing == vk::True;
	}

	bool IsSuitable() const {
		bool has_extensions					   = SupportsExtensions(extensions);
		bool has_queue						   = SupportsQueue(queue_info);
		bool has_cooperative_matrix_features   = SupportsCooperativeMatrixFeatures();
		bool has_cooperative_matrix_2_features = SupportsCooperativeMatrix2Features();
		bool is_suitable = has_extensions && has_queue && has_cooperative_matrix_features &&
						   has_cooperative_matrix_2_features;
		std::printf("Device: %s\n", GetProperties2().properties.deviceName.data());
		std::printf("Supports extensions: %s\n", FormatBool(has_extensions));
		std::printf("Supports queue: %s\n", FormatBool(has_queue));
		std::printf("Supports cooperative matrix features: %s\n",
					FormatBool(has_cooperative_matrix_features));
		std::printf("Supports cooperative matrix 2 features: %s\n",
					FormatBool(has_cooperative_matrix_2_features));
		std::printf("IsSuitable: %d\n", is_suitable);
		return is_suitable;
	}
};

int main() {
	// === Setup ===
	vb::SetLogLevel(vb::LogLevel::Trace);

	vb::Instance instance({.optional_layers = {{vb::kValidationLayerName}}});

	int selected_device_index = instance.GetPhysicalDevices().size();
	std::vector<PhysicalDeviceExt> physical_devices;
	physical_devices.reserve(instance.GetPhysicalDevices().size());
	for (auto& vk_device : instance.GetPhysicalDevices()) {
		physical_devices.emplace_back(vk_device);
		auto& current_device = physical_devices.back();
		current_device.GetDetails();
		if (current_device.IsSuitable()) {
			selected_device_index = physical_devices.size() - 1;
			break;
		}
	}

	if (selected_device_index == instance.GetPhysicalDevices().size()) {
		std::printf("No physical device with cooperative matrix 2 support found\n");
		return 1;
	}

	PhysicalDeviceExt& physical_device = physical_devices[selected_device_index];


	// Create device
	vk::PhysicalDeviceFeatures2 features2{};

	vk::PhysicalDeviceCooperativeMatrixFeaturesKHR cooperative_matrix_features{
		.cooperativeMatrix					 = vk::True,
	};

	vk::PhysicalDeviceCooperativeMatrix2FeaturesNV cooperative_matrix_2_features{
		.cooperativeMatrixWorkgroupScope	 = vk::True,
		.cooperativeMatrixFlexibleDimensions = vk::True,
		.cooperativeMatrixTensorAddressing	 = vk::True,
	};

	void* feature_chain[] = {&features2, &cooperative_matrix_features, &cooperative_matrix_2_features};
	vb::SetupStructureChain(feature_chain);

	vb::Device device(instance, physical_device,
					  {.queues	   = {{{.queueFamilyIndex = physical_device.FindQueueFamilyIndex(queue_info),
										.queueCount		  = 1}}},
					   .extensions = extensions,
					   .features2 = &features2});

	vb::BindlessDescriptor bindless_descriptor(
		device, {.bindings = {{{.binding		 = kBindingBuffer,
								.descriptorType	 = vk::DescriptorType::eStorageBuffer,
								.descriptorCount = kMaxDescriptorBindings,
								.stageFlags		 = vk::ShaderStageFlagBits::eCompute}}}});

	vb::Queue const& queue = *device.GetQueue(queue_info);

	vk::PipelineLayout bindless_pipeline_layout(device.GetOrCreatePipelineLayout(
		{.descriptor_set_layouts = {{bindless_descriptor.layout}},
		 .push_constant_ranges	 = {{{
			   .stageFlags = vk::ShaderStageFlagBits::eAll,
			   .offset	   = 0,
			   .size	   = physical_device.GetMaxPushConstantsSize(),
		   }}}}));

	// Load extension functions from dynamic library
	vb::LoadInstanceCooperativeMatrixFunctionsKHR(instance);
	vb::LoadInstanceCooperativeMatrix2FunctionsNV(instance);

	// === Application ===
	auto [result, cooperative_matrix_properties] = physical_device.getCooperativeMatrixPropertiesKHR();
	vb::CheckVkResult(result, "Failed to get cooperative matrix properties");
	PrintCooperativeMatrixProperties(cooperative_matrix_properties);

	auto [result_2, cooperative_matrix_2_properties] = physical_device.getCooperativeMatrixFlexibleDimensionsPropertiesNV();
	vb::CheckVkResult(result_2, "Failed to get cooperative matrix 2 properties");
	PrintCooperativeMatrix2Properties(cooperative_matrix_2_properties);
}