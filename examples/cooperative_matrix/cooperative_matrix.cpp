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
#include "float16_t.hpp"
#include "matrix.hpp"

using std::uint32_t;
using numeric::float16_t;

bool bVerbose = false;
char constexpr const* kExtensions[]		  = {vk::KHRCooperativeMatrixExtensionName,
											 vk::NVCooperativeMatrix2ExtensionName};
vb::QueueInfo constexpr kComputeQueueInfo = {.flags = vk::QueueFlagBits::eCompute};

struct PhysicalDevice : public vb::PhysicalDevice {
	vk::PhysicalDeviceCooperativeMatrixFeaturesKHR	 cooperative_matrix_features{};
	vk::PhysicalDeviceCooperativeMatrixPropertiesKHR cooperative_matrix_properties{};
	vk::PhysicalDeviceCooperativeMatrix2FeaturesNV	 cooperative_matrix_2_features{};
	vk::PhysicalDeviceCooperativeMatrix2PropertiesNV cooperative_matrix_properties2{};

	PhysicalDevice(vk::PhysicalDevice const& physical_device) : vb::PhysicalDevice(physical_device) {
		// Link feature chains for GetDetails()
		GetFeatures().LinkNextStructure(cooperative_matrix_features);
		GetFeatures().LinkNextStructure(cooperative_matrix_2_features);
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
		bool has_extensions					   = SupportsExtensions(kExtensions);
		bool has_queue						   = SupportsQueue(kComputeQueueInfo);
		bool has_cooperative_matrix_features   = SupportsCooperativeMatrixFeatures();
		bool has_cooperative_matrix_2_features = SupportsCooperativeMatrix2Features();
		bool is_suitable = has_extensions && has_queue && has_cooperative_matrix_features &&
						   has_cooperative_matrix_2_features;
		if (bVerbose) {
			std::printf("Device: %s\n", GetProperties().GetCore10().deviceName.data());
			std::printf("Supports extensions: %s\n", FormatBool(has_extensions));
			std::printf("Supports queue: %s\n", FormatBool(has_queue));
			std::printf("Supports cooperative matrix features: %s\n",
						FormatBool(has_cooperative_matrix_features));
			std::printf("Supports cooperative matrix 2 features: %s\n",
						FormatBool(has_cooperative_matrix_2_features));
			std::printf("IsSuitable: %d\n", is_suitable);
		}	
		return is_suitable;
	}
};

struct TestParams{
	uint32_t M, N, K;
	uint32_t tileM, tileN, tileK;
	uint32_t workgroupSize;

	float alpha;
	float beta;

	vb::Device&			device;
	vb::Queue const&	queue;
	vb::Command&		cmd;
	vk::PipelineLayout& pipeline_layout;
	std::span<vk::CooperativeMatrixFlexibleDimensionsPropertiesNV const> const& flexibleDimensionsProperties;
};

template <typename AType, typename ResultType>
void RunTest(TestParams const& params) {
	uint32_t tileM = params.tileM;
	uint32_t tileN = params.tileN;
	uint32_t tileK = params.tileK == uint32_t{} ? GetOptimalTileK<AType, ResultType>() : params.tileK;

	std::printf("Running test with AType=%s, ResultType=%s, M=%d, N=%d, K=%d, tileM=%d, tileN=%d, tileK=%d, "
				"workgroupSize=%d\n",
				TypeToString<AType>(), TypeToString<ResultType>(), params.M, params.N, params.K, tileM, tileN,
				tileK, params.workgroupSize);

	if (!ValidateParameters<AType, ResultType>(params.M, params.N, params.K, tileM, tileN,
											  tileK, params.workgroupSize,
											  params.flexibleDimensionsProperties)) {
		std::printf("No suitable flexible dimension property found.");
		return;
	}

	auto& device = params.device;
	auto& queue	 = params.queue;
	auto& cmd	 = params.cmd;

	Matrix<AType>	   matA{device, params.M, params.K};
	Matrix<AType>	   matB{device, params.K, params.N};
	Matrix<ResultType> matC{device, params.M, params.N};
	Matrix<ResultType> matD{device, params.M, params.N};

	// Fill CPU matrices
	InitMatrixAB(matA);
	InitMatrixAB(matB);
	InitMatrixC(matC);
	InitMatrixD(matD);

	if (bVerbose) {
		std::printf("Matrix A:\n");
		PrintMatrix(matA);
		std::printf("Matrix B:\n");
		PrintMatrix(matB);
		std::printf("Matrix C:\n");
		PrintMatrix(matC);
		std::printf("Matrix D:\n");
		PrintMatrix(matD);
	}

	uint32_t const specData[] = {
		params.M, // Matrix size
		params.N,
		params.K,
		tileM, // Cooperative matrix size
		tileN,
		tileK,
		params.workgroupSize,
		reinterpret_cast<uint32_t const&>(params.alpha),
		reinterpret_cast<uint32_t const&>(params.beta),
	};

	struct Constants {
		vk::DeviceAddress inputA;
		vk::DeviceAddress inputB;
		vk::DeviceAddress inputC;
		vk::DeviceAddress outputD;
	};

	Constants constants{
		.inputA	 = matA.deviceBuffer.GetAddress(),
		.inputB	 = matB.deviceBuffer.GetAddress(),
		.inputC	 = matC.deviceBuffer.GetAddress(),
		.outputD = matD.deviceBuffer.GetAddress(),
	};

	char spv_file[256];
	std::snprintf(spv_file, sizeof(spv_file) - 1, "cooperative_matrix_%c%llu_%c%llu.comp.spv",
				  TypeToString<AType>()[0], sizeof(AType) * 8, TypeToString<ResultType>()[0],
				  sizeof(ResultType) * 8);

	vb::Pipeline pipeline(device, {
	  .stages = {{{
		  .stage			   = vk::ShaderStageFlagBits::eCompute,
		  .source			   = {spv_file, vb::Source::Type::FileSpirV},
		  .specialization_info = vb::util::MakeSpecializationInfo(specData),
	  }}},
	  .layout = params.pipeline_layout,
	  .name = "Cooperative Matrix",
  });

	// Record and submit command buffer
	cmd.Begin();
	cmd.Copy(matA.deviceBuffer, matA.hostBuffer);
	cmd.Copy(matB.deviceBuffer, matB.hostBuffer);
	cmd.Copy(matC.deviceBuffer, matC.hostBuffer);
	cmd.Barrier(matA.deviceBuffer);
	cmd.Barrier(matB.deviceBuffer);
	cmd.Barrier(matC.deviceBuffer);
	cmd.BindPipeline(pipeline);
	cmd.PushConstants(pipeline,  &constants, sizeof(constants));
	cmd.Dispatch(params.N / tileN, params.M / tileM, 1);
	cmd.Barrier(matD.deviceBuffer);
	cmd.Copy(matD.hostBuffer, matD.deviceBuffer);
	cmd.End();
	cmd.Submit(queue);
	queue.Wait();

	if (bVerbose) {
		std::printf("Matrix Result:\n");
		PrintMatrix(matD);
	}

	std::printf("Validating result...\n");
	uint32_t error_count = ValidateMultiplication(matA, matB, matC, matD, ResultType(params.alpha), ResultType(params.beta));
	std::printf("Total elements: %zu, errors: %d\n", matD.GetTotalElements(), error_count);
	if (error_count > 0) {
		std::printf("Actual:\n");
		PrintMatrix(matD);
		std::printf("Expected:\n");
		MatMulAdd(matA, matB, matC, matD, ResultType(params.alpha), ResultType(params.beta));
		PrintMatrix(matD);
	}
}

int main(int argc, char* argv[]) {
	// Parse arguments
	constexpr std::string_view kVerboseArg = "--verbose";
	std::printf("Usage: %s [%s]\n", argv[0], kVerboseArg.data());
	for (char const* arg : std::span(argv, argc)) {
		if (arg == kVerboseArg) {
			bVerbose = true;
		}
	}

	// Setup 
	vb::SetLogLevel(vb::LogLevel::Info);

	vb::Instance instance({.optional_layers = {{vb::kValidationLayerName}}, 
						   .optional_extensions = {{vk::EXTDebugUtilsExtensionName}}});
	int	 selected_device_index = instance.GetPhysicalDevices().size();;
	std::vector<PhysicalDevice> physical_devices;  
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

	PhysicalDevice& physical_device = physical_devices[selected_device_index];

	// Create device
	vk::PhysicalDeviceFeatures2		   features2{};
	vk::PhysicalDeviceVulkan11Features vulkan11_features{.storageBuffer16BitAccess = vk::True};
	vk::PhysicalDeviceVulkan12Features vulkan12_features{
		.storageBuffer8BitAccess = vk::True,
		.shaderFloat16			 = vk::True,
		.shaderInt8				 = vk::True,
		.bufferDeviceAddress	 = vk::True,
		.vulkanMemoryModel		 = vk::True,
	};
	vk::PhysicalDeviceVulkan13Features vulkan13_features{.synchronization2 = vk::True,
														 .maintenance4	   = vk::True};

	vk::PhysicalDeviceCooperativeMatrixFeaturesKHR cooperative_matrix_features{
		.cooperativeMatrix					 = vk::True,
	};

	vk::PhysicalDeviceCooperativeMatrix2FeaturesNV cooperative_matrix_2_features{
		.cooperativeMatrixWorkgroupScope	 = vk::True,
		.cooperativeMatrixFlexibleDimensions = vk::True,
		.cooperativeMatrixTensorAddressing	 = vk::True,
	};

	vb::SetupStructureChain({
		{&features2, &vulkan11_features, &vulkan12_features, &vulkan13_features, &cooperative_matrix_features,
		 &cooperative_matrix_2_features}
	});

	vb::Device device(instance, physical_device,
					  {.queues	   = {{{.queueFamilyIndex = physical_device.FindQueueFamilyIndex(kComputeQueueInfo),
										.queueCount		  = 1}}},
					   .extensions = kExtensions,
					   .features2 = &features2});


	vb::PipelineLayout pipeline_layout(device, {.push_constant_ranges = {{{
											  .stageFlags = vk::ShaderStageFlagBits::eAll,
											  .offset	  = 0,
											  .size		  = physical_device.GetMaxPushConstantsSize(),
										  }}}});

	vb::Queue const& queue = *device.GetQueue(kComputeQueueInfo);
	vb::Command cmd = device.CreateCommand(queue.GetFamilyIndex());

	// Load extension functions from dynamic library
	vb::LoadInstanceCooperativeMatrixFunctionsKHR(instance);
	vb::LoadInstanceCooperativeMatrix2FunctionsNV(instance);

	auto [result_1, cooperativeMatrixProperties] = physical_device.getCooperativeMatrixPropertiesKHR();
	vb::CheckVkResult(result_1, "Failed to get cooperative matrix properties");
	auto [result_2, flexibleDimensionsProperties] = physical_device.getCooperativeMatrixFlexibleDimensionsPropertiesNV();
	vb::CheckVkResult(result_2, "Failed to get cooperative matrix 2 properties");
	if (bVerbose) {
		PrintCooperativeMatrixProperties(cooperativeMatrixProperties);
		PrintCooperativeMatrix2Properties(flexibleDimensionsProperties);
	}

	// Run tests
	TestParams params{
		// M, N, K should be multiple of tileM, tileN, tileK
		.M = 256,
		.N = 256,
		.K = 256,

		// tileM, tileN, tileK should be multiple of MGranularity, NGranularity, KGranularity
		.tileM = 128,
		.tileN = 128,
		.tileK{}, // Select automatically

		// workgroupSize should match workgroupInvocations
		.workgroupSize = 128,

		// Coefficients of matrix multiply alpha*A*B + beta*C = Output
		.alpha = 2.0f,
		.beta  = 3.0f,

		// Handles
		.device			 = device,
		.queue			 = queue,
		.cmd			 = cmd,
		.pipeline_layout = pipeline_layout,
		.flexibleDimensionsProperties = flexibleDimensionsProperties,
	};

	RunTest<uint8_t, uint32_t>(params);
	RunTest<int8_t, int32_t>(params);
	RunTest<float16_t, float16_t>(params);
	RunTest<float16_t, float>(params);
}