
#ifndef VULKAN_BACKEND_ENUMS_HPP_
#define VULKAN_BACKEND_ENUMS_HPP_

#include <vulkan_backend/config.hpp>

#if !defined(VB_USE_STD_MODULE) || !VB_USE_STD_MODULE
#include <cstdint>
#elif !defined(_VB_INCLUDE_IN_MODULE)
import std;
#endif

VB_EXPORT
namespace VB_NAMESPACE {
using Flags   = std::uint32_t;
using Flags64 = std::uint64_t;

namespace Memory {
	enum {
		DeviceLocal     = 0x00000001,
		HostVisible     = 0x00000002,
		HostCoherent    = 0x00000004,
		HostCached      = 0x00000008,
		LazilyAllocated = 0x00000010,
		Protected       = 0x00000020,
		MaxEnum         = 0x7FFFFFFF,
		GPU             = DeviceLocal,
		CPU             = HostVisible | HostCoherent,
	};
};

using MemoryFlags = Flags;

namespace BufferUsage {
	enum {
		TransferSrc                                 = 0x00000001,
		TransferDst                                 = 0x00000002,
		UniformTexel                                = 0x00000004,
		StorageTexel                                = 0x00000008,
		Uniform                                     = 0x00000010,
		Storage                                     = 0x00000020,
		Index                                       = 0x00000040,
		Vertex                                      = 0x00000080,
		Indirect                                    = 0x00000100,
		Address                                     = 0x00020000,
		VideoDecodeSrcKHR                           = 0x00002000,
		VideoDecodeDstKHR                           = 0x00004000,
		TransformFeedbackBufferEXT                  = 0x00000800,
		TransformFeedbackCounterBufferEXT           = 0x00001000,
		ConditionalRenderingEXT                     = 0x00000200,
		AccelerationStructureBuildInputReadOnlyKHR  = 0x00080000,
		AccelerationStructureStorageKHR             = 0x00100000,
		ShaderBindingTableKHR                       = 0x00000400,
		VideoEncodeDstKHR                           = 0x00008000,
		VideoEncodeSrcKHR                           = 0x00010000,
		SamplerDescriptorBufferEXT                  = 0x00200000,
		ResourceDescriptorBufferEXT                 = 0x00400000,
		PushDescriptorsDescriptorBufferEXT          = 0x04000000,
		MicromapBuildInputReadOnlyEXT               = 0x00800000,
		MicromapStorageEXT                          = 0x01000000,
		RayTracingNV                                = ShaderBindingTableKHR,
		ShaderDeviceAddressEXT                      = Address,
		ShaderDeviceAddressKHR                      = Address,
	};
}
using BufferUsageFlags = Flags;

enum class Format {
	Undefined        = 0,

	R8Unorm          = 9,  // Sampled, heightmap

	RGBA8Unorm       = 37, // ColorAttachment
	RGBA8Srgb        = 43,
	BGRA8Unorm       = 44,
	BGRA8Srgb        = 50,
	RGBA16Sfloat     = 97,

	RG32Sfloat       = 103, // VertexAttribute
	RGB32Sfloat      = 106, 
	RGBA32Sfloat     = 109, 

	D16Unorm         = 124, // DepthStencilAttachment
	D32Sfloat        = 126, // DepthAttachment
	D24UnormS8Uint   = 129, // DepthStencilAttachment
};

namespace ImageUsage {
	enum {
		TransferSrc            = 0x00000001,
		TransferDst            = 0x00000002,
		Sampled                = 0x00000004,
		Storage                = 0x00000008,
		ColorAttachment        = 0x00000010,
		DepthStencilAttachment = 0x00000020,
		TransientAttachment    = 0x00000040,
		InputAttachment        = 0x00000080,
	};
}
using ImageUsageFlags = Flags;

namespace Aspect {
	enum {
		Color = 1,
		Depth = 2,
		Stencil = 4,
	};
}
using AspectFlags = Flags;

enum class ImageLayout {
	Undefined                      = 0,
	General                        = 1,
	ColorAttachment                = 2,
	DepthStencilAttachment         = 3,
	DepthStencilRead               = 4,
	ShaderRead                     = 5,
	TransferSrc                    = 6,
	TransferDst                    = 7,
	Preinitialized                 = 8,
	DepthReadOnlyStencilAttachment = 1000117000,
	DepthAttachmentStencilRead     = 1000117001,
	DepthAttachment                = 1000241000,
	DepthReadOnly                  = 1000241001,
	StencilAttachment              = 1000241002,
	StencilReadOnly                = 1000241003,
	ReadOnly                       = 1000314000,
	Attachment                     = 1000314001,
	Present                        = 1000001002,

	MaxEnum = 0x7FFFFFFF
};


enum class SampleCount {
	_1  = 0x00000001,
	_2  = 0x00000002,
	_4  = 0x00000004,
	_8  = 0x00000008,
	_16 = 0x00000010,
	_32 = 0x00000020,
	_64 = 0x00000040,
	MaxEnum = 0x7FFFFFFF
};

enum class Filter {
	Nearest = 0,
	Linear  = 1,
	Cubic_Ext = 1000015000,
};

enum class MipmapMode {
	Nearest = 0,
	Linear  = 1,
};

enum class WrapMode : std::uint16_t {
	Repeat            = 0,
	MirroredRepeat    = 1,
	ClampToEdge       = 2,
	ClampToBorder     = 3,
	MirrorClampToEdge = 4,
};

enum class CompareOp {
	Never          = 0,
	Less           = 1,
	Equal          = 2,
	LessOrEqual    = 3,
	Greater        = 4,
	NotEqual       = 5,
	GreaterOrEqual = 6,
	Always         = 7,
	MaxEnum        = 0x7FFFFFFF
};

enum class BorderColor {
	IntOpaqueBlack   = 0,
	IntOpaqueWhite   = 1,
	FloatOpaqueBlack = 2,
	FloatOpaqueWhite = 3,
	MaxEnum = 0x7FFFFFFF
};

enum class DescriptorType {
	Sampler                  = 0,
	CombinedImageSampler     = 1,
	SampledImage             = 2,
	StorageImage             = 3,
	UniformTexelBuffer       = 4,
	StorageTexelBuffer       = 5,
	UniformBuffer            = 6,
	StorageBuffer            = 7,
	UniformBufferDynamic     = 8,
	StorageBufferDynamic     = 9,
	InputAttachment          = 10,
	InlineUniformBlock       = 1000138000,
	AccelerationStructureKHR = 1000150000,
	AccelerationStructureNV  = 1000165000,
	SampleWeightImageQCOM    = 1000440000,
	BlockMatchImageQCOM      = 1000440001,
	MutableEXT               = 1000351000,
	MaxEnum                  = 0x7FFFFFFF
};

namespace QueueFlagBits {
	enum {
		Graphics      = 0x00000001,
		Compute       = 0x00000002,
		Transfer      = 0x00000004,
		SparseBinding = 0x00000008, 
		Protected     = 0x00000010,
		VideoDecode   = 0x00000020,
		VideoEncode   = 0x00000040,
		OpticalFlow   = 0x00000100,
		MaxEnum       = 0x7FFFFFFF
	};
}

using QueueFlags = Flags;

enum class PipelinePoint {
	Graphics   = 0,
	Compute    = 1,
	RayTracing = 1000165000,
	MaxEnum    = 0x7FFFFFFF
};

enum class ShaderStage {
	Vertex                 = 0x00000001,
	TessellationControl    = 0x00000002,
	TessellationEvaluation = 0x00000004,
	Geometry               = 0x00000008,
	Fragment               = 0x00000010,
	Compute                = 0x00000020,
	AllGraphics            = 0x0000001F,
	All                    = 0x7FFFFFFF,

	Raygen                 = 0x00000100,
	AnyHit                 = 0x00000200,
	ClosestHit             = 0x00000400,
	Miss                   = 0x00000800,
	Intersection           = 0x00001000,
	Callable               = 0x00002000,

	Task                   = 0x00000040,
	Mesh                   = 0x00000080,
	MaxEnum                = 0x7FFFFFFF
};

namespace CullMode {
	enum {
		None         = 0,
		Front        = 1,
		Back         = 2,
		FrontAndBack = 3,
		MaxEnum      = 0x7FFFFFFF
	};
}
using CullModeFlags = Flags;

enum class DynamicState {
	ViewPort                    = 0,
	Scissor                     = 1,
	Line_Width                  = 2,
	Depth_Bias                  = 3,
	Blend_Constants             = 4,
	Depth_Bounds                = 5,
	Stencil_Compare_Mask        = 6,
	Stencil_Write_Mask          = 7,
	Stencil_Reference           = 8,
	Cull_Mode                   = 1000267000,
	Front_Face                  = 1000267001,
	Primitive_Topology          = 1000267002,
	ViewPort_With_Count         = 1000267003,
	Scissor_With_Count          = 1000267004,
	Vertex_Input_Binding_Stride = 1000267005,
	Depth_Test_Enable           = 1000267006,
	Depth_Write_Enable          = 1000267007,
	Depth_Compare_Op            = 1000267008,
	Depth_Bounds_Test_Enable    = 1000267009,
	Stencil_Test_Enable         = 1000267010,
	Stencil_Op                  = 1000267011,
	Rasterizer_Discard_Enable   = 1000377001,
	Depth_Bias_Enable           = 1000377002,
	Primitive_Restart_Enable    = 1000377004,
};

/* ===== Synchronization 2 Stages ===== */
namespace PipelineStage {
	enum : Flags64 {
		None                         = 0ULL,
		TopOfPipe                    = 0x00000001ULL, // == None as src | AllCommands with AccessFlags set to 0 as dst
		DrawIndirect                 = 0x00000002ULL,
		VertexInput                  = 0x00000004ULL,
		VertexShader                 = 0x00000008ULL,
		TessellationControlShader    = 0x00000010ULL,
		TessellationEvaluationShader = 0x00000020ULL,
		GeometryShader               = 0x00000040ULL,
		FragmentShader               = 0x00000080ULL,
		EarlyFragmentTests           = 0x00000100ULL,
		LateFragmentTests            = 0x00000200ULL,
		ColorAttachmentOutput        = 0x00000400ULL,
		ComputeShader                = 0x00000800ULL,
		AllTransfer                  = 0x00001000ULL,
		Transfer                     = 0x00001000ULL,
		BottomOfPipe                 = 0x00002000ULL, // == None as dst | AllCommands with AccessFlags set to 0 as src
		Host                         = 0x00004000ULL,
		AllGraphics                  = 0x00008000ULL,
		AllCommands                  = 0x00010000ULL,
		Copy                         = 0x100000000ULL,
		Resolve                      = 0x200000000ULL,
		Blit                         = 0x400000000ULL,
		Clear                        = 0x800000000ULL,
		IndexInput                   = 0x1000000000ULL,
		VertexAttributeInput         = 0x2000000000ULL,
		PreRasterizationShaders      = 0x4000000000ULL
	};
}
using PipelineStageFlags = Flags64;

/* ===== Synchronization 2 Access ===== */
namespace Access {
	enum : Flags64 {
		None                        = 0ULL,
		IndirectCommandRead         = 0x00000001ULL,
		IndexRead                   = 0x00000002ULL,
		VertexAttributeRead         = 0x00000004ULL,
		UniformRead                 = 0x00000008ULL,
		InputAttachmentRead         = 0x00000010ULL,
		ShaderRead                  = 0x00000020ULL,
		ShaderWrite                 = 0x00000040ULL,
		ColorAttachmentRead         = 0x00000080ULL,
		ColorAttachmentWrite        = 0x00000100ULL,
		DepthStencilAttachmentRead  = 0x00000200ULL,
		DepthStencilAttachmentWrite = 0x00000400ULL,
		TransferRead                = 0x00000800ULL,
		TransferWrite               = 0x00001000ULL,
		HostRead                    = 0x00002000ULL,
		HostWrite                   = 0x00004000ULL,
		MemoryRead                  = 0x00008000ULL,
		MemoryWrite                 = 0x00010000ULL,
		ShaderSampledRead           = 0x100000000ULL,
		ShaderStorageRead           = 0x200000000ULL,
		ShaderStorageWrite          = 0x400000000ULL,
	};
}
using AccessFlags = Flags64;


enum class LoadOp {
	Load     = 0,
	Clear    = 1,
	DontCare = 2,
	NoneExt  = 1000400000,
	MaxEnum  = 0x7FFFFFFF, 
};


enum class StoreOp {
	Store    = 0,
	DontCare = 1,
	None     = 1000301000,
	MaxEnum  = 0x7FFFFFFF
};

namespace ResolveMode {
	enum  {
		None       = 0,
		SampleZero = 0x00000001,
		Average    = 0x00000002,
		Min        = 0x00000004,
		Max        = 0x00000008,
		MaxEnum    = 0x7FFFFFFF
	};
};

using ResolveModeFlags = Flags;

enum class PresentMode {
    Immediate               = 0,
    Mailbox                 = 1,
    Fifo                    = 2,
    FifoRelaxed             = 3,
    SharedDemandRefresh     = 1000111000,
    SharedContinuousRefresh = 1000111001,
};

enum class ColorSpace {
	SrgbNonlinear = 0,
};

} // VB_NAMESPACE

#endif // VULKAN_BACKEND_ENUMS_HPP_