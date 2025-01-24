TARGET := vector_addition

AR := ar
ARFLAGS = rcs

INCLUDE := \
	-I../../include \
	-I../../deps/VulkanMemoryAllocator/include \

EXAMPLE_SRCS = $(wildcard *.cpp)
VB_SRC := ../../src

ifeq ($(BUILD_DIR),)
	BUILD_DIR := .
endif

ifeq ($(BUILD_TYPE),)
	BUILD_TYPE := Debug
endif

EXAMPLE_OBJS := $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(EXAMPLE_SRCS))
VB_OBJS := \
	$(patsubst $(VB_SRC)/%.cpp, $(BUILD_DIR)/%.o, $(wildcard $(VB_SRC)/*.cpp)) \
	$(patsubst $(VB_SRC)/resource/%.cpp, $(BUILD_DIR)/%.o, $(wildcard $(VB_SRC)/resource/*.cpp))
OBJS := $(EXAMPLE_OBJS) $(VB_OBJS)

DEPFILES := $(OBJS:.o=.d)

VB_LIB := $(BUILD_DIR)/libvulkan_backend.a

ifeq ($(findstring clang,$(CC)),clang)
WARNINGS_DISABLE := -Wno-nullability-completeness
endif

VULKAN_HPP_FLAGS = \
    -DVULKAN_HPP_NO_EXCEPTIONS \
    -DVULKAN_HPP_RAII_NO_EXCEPTIONS \
    -DVULKAN_HPP_NO_SMART_HANDLE \
    -DVULKAN_HPP_NO_CONSTRUCTORS \
    -DVULKAN_HPP_NO_UNION_CONSTRUCTORS

CXXFLAGS := -MMD -MP -std=c++20 $(INCLUDE) $(WARNINGS_DISABLE) $(VULKAN_HPP_FLAGS) -fpermissive

ifeq ($(BUILD_TYPE),Debug)
	CXXFLAGS += -g -ggdb -O0
else
	CXXFLAGS += -O3
endif

# LDFLAGS := -lvulkan_backend
# -l:vulkan_backend.a 

ifeq ($(OS),Windows_NT)
	LDFLAGS += -lvulkan-1
else
	LDFLAGS += -lvulkan -lm
endif

# $(info $(OBJS))
# $(info $(DEPFILES))

all: $(TARGET)

ar: $(VB_LIB)

$(VB_LIB): $(VB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(BUILD_DIR)/%.o: %.cpp
	@echo "Compiling $(notdir $<)"
	@$(CXX) $(CXXFLAGS) -o$@ -c $<
	
$(BUILD_DIR)/%.o: $(VB_SRC)/%.cpp
	@echo "Compiling $(notdir $<)"
	@$(CXX) $(CXXFLAGS) -o$@ -c $<

$(BUILD_DIR)/%.o: $(VB_SRC)/resource/%.cpp
	@echo "Compiling $(notdir $<)"
	@$(CXX) $(CXXFLAGS) -o$@ -c $<


$(TARGET): $(EXAMPLE_OBJS) $(VB_OBJS)
	@$(CXX) -o$@ $^ $(LDFLAGS)

rm:
	@$(RM) \
	$(wildcard $(BUILD_DIR)/*.o) \
	$(wildcard $(BUILD_DIR)/*.d) \
	$(wildcard $(BUILD_DIR)/*.a) \
	$(TARGET)

-include $(DEPFILES)

# Example coomand
# make BUILD_DIR=build BUILD_TYPE=Debug -j8
