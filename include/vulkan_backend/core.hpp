#ifndef VULKAN_BACKEND_CORE_HPP_
#define VULKAN_BACKEND_CORE_HPP_

#include <vulkan_backend/config.hpp>
#include <vulkan_backend/enums.hpp>
#include <vulkan_backend/structs.hpp>

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#include <cstdint>
#include <span>
#include <string_view>
#elif !defined(_VB_INCLUDE_IN_MODULE)
import std;
#endif

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

VB_EXPORT
namespace VB_NAMESPACE {
using i32 = std::int32_t;
using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using DeviceAddress = u64;

struct PhysicalDeviceResource;
struct InstanceResource;
struct DeviceResource;
struct SwapChainResource;
struct BufferResource;
struct ImageResource;
struct PipelineResource;
struct CommandResource;
struct QueueResource;
struct Semaphore;

struct BufferInfo;
struct ImageInfo;
struct PipelineInfo;
struct SubmitInfo;
struct RenderingInfo;
struct BlitInfo;
struct InstanceInfo;

class Buffer;
class Command;
class Device;
class Image;
class Swapchain;
class Queue;
class Pipeline;


enum class LogLevel {
	Trace,
	Info,
	Warning,
	Error,
	None,
};


struct BufferInfo {
	u64              size;
	BufferUsageFlags usage;
	MemoryFlags      memory = Memory::GPU;
	std::string_view name = "";
};

class Buffer {
	std::shared_ptr<BufferResource> resource;
	friend class Device;
	friend class Command;
	friend DeviceResource;
public:
	auto GetResourceID() const -> u32;
	auto GetSize() const -> u64;
	auto GetMappedData() -> void*;
	auto GetAddress() -> DeviceAddress;
	auto Map() -> void*;
	void Unmap();
};

class Image {
	std::shared_ptr<ImageResource> resource;
	friend SwapChainResource;
	friend class Swapchain;
	friend DeviceResource;
	friend class Command;
public:
	auto GetResourceID() const -> u32;
	auto GetFormat()     const -> Format;
};

struct ImageInfo {
	Extent3D          extent;
	Format            format;
	ImageUsageFlags   usage;
	SampleCount       samples = SampleCount::_1;
	SamplerInfo       sampler = {};
	u32               layers  = 1;
	std::string_view  name    = "";
};

class Queue {
	QueueResource* resource = nullptr;
	friend SwapChainResource;
	friend class Swapchain;
	friend CommandResource;
	friend DeviceResource;
	friend class Device;
public:
	inline operator bool() const {
		return resource != nullptr;
	}

	[[nodiscard]] auto GetCommandBuffer() -> Command&;

	auto GetFlags() const -> QueueFlags;
	auto HasSeparateFamily() const -> bool;
	auto SupportsPresentTo(void const* window) const -> bool;
};

struct QueueInfo {
	QueueFlags flags = QueueFlagBits::Graphics;
	bool  separate_family = false;   // Prefer separate family
	void* present_window = nullptr;  // Platform window
};

struct Source  {
	enum Type {
		File,
		RawGlsl,
		RawSlang,
		RawSpirv,
	};
	std::string_view data;
	Type type = File;
};

class Pipeline {
public:
	struct Stage {
		// Shader stage, e.g. Vertex, Fragment, Compute or other
		ShaderStage stage;

		// Shader code: any glsl or .slang or .spv. It can be file name or raw code.
		// Specify type if data is a string with shader code
		Source source;

		// Output path for compiled .spv. Has effect only if compilation is done
		std::string_view out_path = ".";

		// Shader entry point
		std::string_view entry_point = "main";
		
		// Shader compiler
		std::string_view compiler = "glslangValidator";

		// Additional compiler options
		std::string_view compile_options = "";

		// Do not recompile if respective 'filename'.spv exists and is up-to-date
		bool skip_compilation = true;

		friend DeviceResource;
	};
private:
	std::shared_ptr<PipelineResource> resource;
	friend DeviceResource;
	friend Command;
};

struct PipelineInfo {
	// Necessary for any pipeline creation
	PipelinePoint                    point             = PipelinePoint::MaxEnum;
	std::span<Pipeline::Stage const> stages            = {};
	
	// Data for graphics pipeline
	std::span<Format const>          vertexAttributes  = {};
	std::span<Format const>          color_formats     = {};
	bool                             use_depth         = false;
	Format                           depth_format      = Format::Undefined;
	SampleCount                      samples           = SampleCount::_1;
	CullModeFlags                    cullMode          = CullMode::None;
	bool                             line_topology     = false;
	std::span<DynamicState const>    dynamicStates     = {{DynamicState::ViewPort, DynamicState::Scissor}};
	std::string_view                 name              = "";
};


struct SubmitInfo {
	Semaphore*         waitSemaphore   = nullptr;
	PipelineStageFlags waitStages      = PipelineStage::None;
	Semaphore*         signalSemaphore = nullptr;
	PipelineStageFlags signalStages    = PipelineStage::None;
	u64                waitValue       = 0;
	u64                signalValue     = 0;
};

struct RenderingInfo {
	struct ColorAttachment {
		Image const&    colorImage;
		Image const&    resolveImage = {};
		LoadOp          loadOp       = LoadOp::Clear;
		StoreOp         storeOp      = StoreOp::Store;
		ClearColorValue clearValue   = {{0.0f, 0.0f, 0.0f, 0.0f}};
	};

	struct DepthStencilAttachment {
		Image const&           image;
		LoadOp                 loadOp     = LoadOp::Clear;
		StoreOp                storeOp    = StoreOp::Store;
		ClearDepthStencilValue clearValue = {1.0f, 0};
	};

	std::span<ColorAttachment const> colorAttachs;
	DepthStencilAttachment const&    depth      = {.image = {}};
	DepthStencilAttachment const&    stencil    = {.image = {}};
	Rect2D                           renderArea = Rect2D{};  // == use size of colorAttachs[0] or depth
	u32                              layerCount = 1;
};

struct BlitInfo {
	Image dst;
	Image src;
	std::span<ImageBlit const> regions = {};
	Filter filter                      = Filter::Linear;
};

struct BarrierInfo {
};

class Command {
public:
	bool Copy(Image  const& dst, const void*   data, u32 size);
	bool Copy(Buffer const& dst, const void*   data, u32 size, u32 dst_offset = 0);
	void Copy(Buffer const& dst, Buffer const& src,  u32 size, u32 dst_offset = 0, u32 src_offset = 0);
	void Copy(Image  const& dst, Buffer const& src,  u32 src_offset = 0);
	void Copy(Buffer const& dst, Image  const& src,  u32 dst_offset, Offset3D image_offset, Extent3D image_extent);

	void Barrier(Image const& img,  ImageBarrier  const& barrier = {});
	void Barrier(Buffer const& buf, BufferBarrier const& barrier = {});
	void Barrier(MemoryBarrier const& barrier = {});

	void Blit(BlitInfo const& info);
	void ClearColorImage(Image const& image, ClearColorValue const& color);

	void BeginRendering(RenderingInfo const& info);
	void SetViewport(Viewport const& viewport);
	void SetScissor(Rect2D const& scissor);
	void EndRendering();
	void BindPipeline(Pipeline const& pipeline);
	void PushConstants(Pipeline const& pipeline, const void* data, u32 size);

	void BindVertexBuffer(Buffer const& vertexBuffer);
	void BindIndexBuffer(Buffer const& indexBuffer);

	void Draw(u32 vertex_count, u32 instance_count, u32 first_vertex, u32 first_instance);
	void DrawIndexed(u32 index_count, u32 instance_count, u32 first_index, i32 vertex_offset, u32 first_instance);
	void DrawMesh(Buffer const& vertex_buffer, Buffer const& index_buffer, u32 index_count);
	void DrawImGui(void* imDrawData);

	void Dispatch(u32 groupCountX, u32 groupCountY, u32 groupCountZ);

	void Begin();
	void End();
	void QueueSubmit(SubmitInfo const& submitInfo = {});

private:
	std::shared_ptr<CommandResource> resource;
	friend class Device;
	friend class Swapchain;
	friend SwapChainResource;
	friend DeviceResource;
};

struct SwapchainInfo {
	void*       window;
	Extent2D    extent;
	Queue       queue;
	u32         frames_in_flight  = 3;
	u32         additional_images = 0;
	// Preferred color format, not guaranteed, get actual format after creation
	Format      color_format      = Format::RGBA8Unorm;
	ColorSpace  color_space       = ColorSpace::SrgbNonlinear;
	PresentMode present_mode      = PresentMode::Mailbox;
};

class Device {
public:
	auto CreateImage(ImageInfo const& info)         -> Image;
	auto CreateBuffer(BufferInfo const& info)       -> Buffer;
	auto CreatePipeline(PipelineInfo const& info)   -> Pipeline;
	auto CreateSwapchain(SwapchainInfo const& info) -> Swapchain;

	void WaitQueue(Queue const& queue);
	void WaitIdle();

	void ResetStaging();
	[[nodiscard]] auto GetStagingPtr()    -> u8*;
	[[nodiscard]] auto GetStagingOffset() -> u32;
	[[nodiscard]] auto GetStagingSize()   -> u32;

	[[nodiscard]] auto GetMaxSamples()    -> SampleCount;

	[[nodiscard]] auto GetQueue(QueueInfo const& info = {}) -> Queue;
	[[nodiscard]] auto GetQueues() -> std::span<Queue>;
	
private:
	std::shared_ptr<DeviceResource> resource;
	friend class Swapchain;
	friend class Instance;
};

struct DescriptorInfo {
	DescriptorType type;
	u32            binding;
	u32            count;
};

struct DeviceInfo {
	std::span<QueueInfo const> queues = {{{QueueFlagBits::Graphics}}};

	u32 staging_buffer_size = 64 * 1024 * 1024;

	// bindless descriprors, must not collide
	u32 binding_sampler         = 0;
	u32 binding_buffer          = 1;
	u32 binding_tlas            = 2;
	u32 binding_storage_image   = 3;

	std::span<DescriptorInfo const> bindings = {{
		{DescriptorType::Sampler,       0, 1},
		{DescriptorType::StorageBuffer, 1, 1},
	}};

	u32 max_storage_buffers     = 8192;
	u32 max_sampled_images      = 8192;
	u32 max_tlas                = 8192;
	u32 max_storage_images      = 8192;

	// Graphics pipeline library
	bool pipeline_library       = true;
	bool link_time_optimization = true;
		
	// Features
	bool unused_attachments     = false;
};

#ifdef VB_GLFW
class Swapchain {
public:
#ifdef VB_IMGUI
	void CreateImGui(SampleCount sample_count);
#endif
	void Recreate(u32 width, u32 height);
	bool AcquireImage();
	void SubmitAndPresent();

	[[nodiscard]] auto GetCurrentImage()  -> Image&;
	[[nodiscard]] auto GetCommandBuffer() -> Command&;
	[[nodiscard]] auto GetDirty()         -> bool;
private:
	std::shared_ptr<SwapChainResource> resource;
	friend class Device;
};
#endif

class PhysicalDevice{
	std::shared_ptr<PhysicalDeviceResource> resource;
	friend PhysicalDeviceResource;
	friend DeviceResource;
	friend class Device;
};

class Instance {
	std::shared_ptr<InstanceResource> resource;
	friend auto CreateInstance(InstanceInfo const& info) -> Instance;
public:
	auto CreateDevice(DeviceInfo const& info = {}) -> Device;
};

auto StringFromVkResult(int result) -> char const*;
void CheckVkResultDefault(int result, char const* message);

struct InstanceInfo {
	void (*checkVkResult)(int, char const*) = CheckVkResultDefault;
	// LogLevel messageCallbackLevel = LogLevel::Warning;
	
	// Extensions
	
	// Validation
	bool validation_layers      = false;
	bool debug_report           = false;

	// Platform
	bool glfw_extensions        = false;
};

auto CreateInstance(InstanceInfo const& info = {}) -> Instance;
void SetLogLevel(LogLevel level);

#ifdef VB_IMGUI
void ImGuiNewFrame();
void ImGuiShutdown();
#endif


} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_CORE_HPP_
