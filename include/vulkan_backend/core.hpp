#ifndef VULKAN_BACKEND_CORE_HPP_
#define VULKAN_BACKEND_CORE_HPP_

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <memory>
#include <cstdint>
#include <span>
#include <string_view>
#elif !defined(_VB_INCLUDE_IN_MODULE)
import std;
#endif

#if !defined(VB_USE_VULKAN_MODULE) || !VB_USE_VULKAN_MODULE
#include <vulkan/vulkan.hpp>
#elif !defined(_VB_INCLUDE_IN_MODULE)
import vulkan_hpp;
#endif

#include <vulkan_backend/config.hpp>
#include <vulkan_backend/structs.hpp>

#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

VB_EXPORT
namespace VB_NAMESPACE {
using i32 = std::int32_t;
using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using vk::DeviceAddress;
using vk::Format;

using vk::Flags;
using vk::BufferUsageFlags;
using vk::ImageUsageFlags;
using vk::QueueFlags;
using vk::CullModeFlags;
using vk::ResolveModeFlags;

using vk::operator&;
using vk::operator|;
using vk::operator^;
using vk::operator~;

using BufferUsage    = vk::BufferUsageFlagBits;
using ImageUsage     = vk::ImageUsageFlagBits;
using Aspect         = vk::ImageAspectFlagBits;
using ShaderStage    = vk::ShaderStageFlagBits;
using ResolveMode    = vk::ResolveModeFlagBits;
using LoadOp         = vk::AttachmentLoadOp;
using StoreOp        = vk::AttachmentStoreOp;
using DynamicState   = vk::DynamicState;
using CullMode       = vk::CullModeFlagBits;
using DescriptorType = vk::DescriptorType;
using QueueFlagBits  = vk::QueueFlagBits;
using PipelinePoint  = vk::PipelineBindPoint;
using PresentMode    = vk::PresentModeKHR;
using ColorSpace     = vk::ColorSpaceKHR;
using Surface        = vk::SurfaceKHR;

auto constexpr inline DebugUtils = vk::EXTDebugUtilsExtensionName;

struct PhysicalDeviceResource;
struct InstanceResource;
struct DeviceResource;
struct DescriptorResource;
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
class Instance;
class Device;
class Image;
class Swapchain;
class Queue;
class Pipeline;

u32 constexpr inline kFramesInFlight = 3;
u32 constexpr inline kAdditionalImages  = 0;
u32 constexpr inline kStagingSize = 64 * 1024 * 1024;

enum class LogLevel {
	Trace,
	Info,
	Warning,
	Error,
	None,
};

enum class Memory {
	GPU = vk::MemoryPropertyFlags::MaskType(vk::MemoryPropertyFlagBits::eDeviceLocal),
	CPU = vk::MemoryPropertyFlags::MaskType(vk::MemoryPropertyFlagBits::eHostVisible)
		| vk::MemoryPropertyFlags::MaskType(vk::MemoryPropertyFlagBits::eHostCoherent),
};

using MemoryFlags = vk::Flags<Memory>;

struct BufferInfo {
	u64              size;
	BufferUsageFlags usage;
	MemoryFlags      memory = Memory::GPU;
	std::string_view name = "";
};

class Buffer {
	std::shared_ptr<BufferResource> resource;
	friend Device;
	friend Command;
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
	friend Swapchain;
	friend DeviceResource;
	friend Command;
public:
	auto GetResourceID() const -> u32;
	auto GetFormat()     const -> Format;
};

struct ImageInfo {
	Extent3D          extent;
	Format            format;
	ImageUsageFlags   usage;
	SampleCount       samples = SampleCount::e1;
	SamplerInfo       sampler = {};
	u32               layers  = 1;
	std::string_view  name    = "";
};

class Queue {
	QueueResource* resource = nullptr;
	friend SwapChainResource;
	friend Swapchain;
	friend CommandResource;
	friend DeviceResource;
	friend Device;
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
	QueueFlags flags = QueueFlagBits::eGraphics;
	bool       separate_family   = false;    // Prefer separate family
	Surface    supported_surface = nullptr;
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

		// Option to not recompile the shader if respective 'filename'.spv exists and is up-to-date.
		// But be careful with this, because if 'compile_options' such as define macros are changed,
		// this might use old .spv file and cause hard to find bugs
		bool allow_skip_compilation = false;

	};
private:
	std::shared_ptr<PipelineResource> resource;
	friend DeviceResource;
	friend Command;
};

struct PipelineInfo {
	// Necessary for any pipeline creation
	PipelinePoint                    point;
	std::span<Pipeline::Stage const> stages;
	
	// Data for graphics pipeline
	std::span<Format const>          vertexAttributes  = {};
	std::span<Format const>          color_formats     = {};
	bool                             use_depth         = false;
	Format                       depth_format          = Format::eUndefined;
	SampleCount                      samples           = SampleCount::e1;
	CullModeFlags                    cullMode          = CullMode::eNone;
	bool                             line_topology     = false;
	std::span<DynamicState const>    dynamicStates     = {{DynamicState::eViewport, DynamicState::eScissor}};
	std::string_view                 name              = "";
};


struct SubmitInfo {
	Semaphore*         waitSemaphore   = nullptr;
	PipelineStageFlags waitStages      = PipelineStage::eNone;
	Semaphore*         signalSemaphore = nullptr;
	PipelineStageFlags signalStages    = PipelineStage::eNone;
	u64                waitValue       = 0;
	u64                signalValue     = 0;
};

struct RenderingInfo {
	struct ColorAttachment {
		Image const&    colorImage;
		Image const&    resolveImage = {};
		LoadOp          loadOp       = LoadOp::eClear;
		StoreOp         storeOp      = StoreOp::eStore;
		ClearColorValue clearValue   = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	};

	struct DepthStencilAttachment {
		Image const&           image;
		LoadOp                 loadOp     = LoadOp::eClear;
		StoreOp                storeOp    = StoreOp::eStore;
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
	Filter filter                      = Filter::eLinear;
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

	[[nodiscard]] auto GetHandle() const -> vk::CommandBuffer;

private:
	std::shared_ptr<CommandResource> resource;
	friend Device;
	friend Swapchain;
	friend SwapChainResource;
	friend DeviceResource;
};

struct SwapchainInfo {
	Surface     surface;
	Extent2D    extent;
	Queue       queue;
	bool        destroy_surface   = false;
	u32         frames_in_flight  = kFramesInFlight;
	u32         additional_images = kAdditionalImages;
	// Preferred color format, not guaranteed, get actual format after creation
	Format  color_format          = Format::eR8G8B8A8Unorm;
	ColorSpace  color_space       = ColorSpace::eSrgbNonlinear;
	PresentMode present_mode      = PresentMode::eMailbox;
};

class Descriptor {
public:
	auto GetBinding(DescriptorType type) -> u32;
private:
	std::shared_ptr<DescriptorResource> resource;
	friend DeviceResource;
	friend BufferResource;
	friend ImageResource;
	friend Device;
	friend Command;
};

struct BindingInfo {
	u32 static constexpr kBindingAuto = ~0u;
	u32 static constexpr kMaxDescriptors = 8192;
	// Type of descriptor (e.g., DescriptorType::Sampler)
	DescriptorType type;
	// Binding number for this type.
	// If not specified, will be assigned automatically
	u32 binding = kBindingAuto;
	// Max number of descriptors for this binding
	u32 count = kMaxDescriptors;
	// Shader stages that can access this resource
	ShaderStage stage_flags = ShaderStage::eAll;
	// Use update after bind flag
	bool update_after_bind = false;
	bool partially_bound   = true;
};

class Device {
public:
	auto CreateImage(ImageInfo const& info)           -> Image;
	auto CreateBuffer(BufferInfo const& info)         -> Buffer;
	auto CreatePipeline(PipelineInfo const& info)     -> Pipeline;
	auto CreateSwapchain(SwapchainInfo const& info)   -> Swapchain;
	
	auto CreateDescriptor(std::span<BindingInfo const> bindings = {{
		{DescriptorType::eSampledImage},
		{DescriptorType::eStorageBuffer},
	}}) -> Descriptor;
	void UseDescriptor(Descriptor const& descriptor);
	auto GetBinding(DescriptorType const type) -> u32;

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
	friend Swapchain;
	friend Instance;
};

struct DeviceInfo {
	std::span<QueueInfo const> queues = {{{QueueFlagBits::eGraphics}}};

	u32 staging_buffer_size = kStagingSize;

	// Graphics pipeline library
	bool pipeline_library       = true;
	bool link_time_optimization = true;
		
	// Features
	bool unused_attachments     = false;
};

struct ImGuiInitInfo
{
    vk::Instance                      Instance;
    vk::PhysicalDevice                PhysicalDevice;
    vk::Device                        Device;
    u32                               QueueFamily;
    vk::Queue                         Queue;
    vk::DescriptorPool                DescriptorPool;
    vk::RenderPass                    RenderPass;
    u32                               MinImageCount;
    u32                               ImageCount;
    vk::SampleCountFlagBits           MSAASamples;

    vk::PipelineCache                 PipelineCache;

    bool                              UseDynamicRendering;

    const vk::AllocationCallbacks*    Allocator;
    void                              (*CheckVkResultFn)(vk::Result err);
    vk::DeviceSize                    MinAllocationSize;
};


class Swapchain {
public:
	auto GetImGuiInfo() -> ImGuiInitInfo;
	void Recreate(u32 width, u32 height);
	bool AcquireImage();
	void SubmitAndPresent();

	[[nodiscard]] auto GetCurrentImage()  -> Image&;
	[[nodiscard]] auto GetCommandBuffer() -> Command&;
	[[nodiscard]] auto GetDirty()         -> bool;
	[[nodiscard]] auto GetFormat()        -> Format;
private:
	std::shared_ptr<SwapChainResource> resource;
	friend Device;
};


class PhysicalDevice{
	std::shared_ptr<PhysicalDeviceResource> resource;
	friend PhysicalDeviceResource;
	friend DeviceResource;
	friend Device;
};

class Instance {
	std::shared_ptr<InstanceResource> resource;
	friend auto CreateInstance(InstanceInfo const& info) -> Instance;
public:
	auto CreateDevice(DeviceInfo const& info = {}) -> Device;
	[[nodiscard]] auto GetHandle() const -> vk::Instance;
};

auto StringFromVkResult(int result) -> char const*;
void CheckVkResultDefault(int result, char const* message);

struct InstanceInfo {
	// Function pointer to check VkResult values
	void (*checkVkResult)(int, char const*) = CheckVkResultDefault;

	// Validation
	bool validation_layers      = false;
	bool debug_report           = false;

	// Additional instance extensions
	std::span<char const*> extensions = {};
};

auto CreateInstance(InstanceInfo const& info = {}) -> Instance;
void SetLogLevel(LogLevel level);

} // namespace VB_NAMESPACE

#endif // VULKAN_BACKEND_CORE_HPP_
