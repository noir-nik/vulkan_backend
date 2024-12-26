TARGET := vector_add

AR := ar
ARFLAGS = rcs

INCLUDE := \
	-I../../include \
	-I../../deps/VulkanMemoryAllocator/include \

EXAMPLE_SRCS = $(wildcard *.cpp)
VB_SRC := ../../src

EXAMPLE_OBJS := $(patsubst %.cpp, %.o, $(EXAMPLE_SRCS))
VB_OBJS := $(patsubst $(VB_SRC)/%.cpp, %.o, $(wildcard $(VB_SRC)/*.cpp))
OBJS := $(EXAMPLE_OBJS) $(VB_OBJS)

DEPFILES := $(OBJS:.o=.d)

VB_LIB := libvulkan_backend.a

# ifeq ($(findstring clang,$(CC)),clang)
WARNINGS_DISABLE := -Wno-nullability-completeness
# endif

CXXFLAGS := -MMD -MP -std=c++20 $(INCLUDE) $(WARNINGS_DISABLE) -g -ggdb
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

./%.o: %.cpp
	@echo "Compiling $(notdir $<)"
	@$(CXX) $(CXXFLAGS) -o$@ -c $<
	
./%.o: $(VB_SRC)/%.cpp
	@echo "Compiling $(notdir $<)"
	@$(CXX) $(CXXFLAGS) -o$@ -c $<

$(TARGET): $(EXAMPLE_OBJS) $(VB_OBJS)
	@$(CXX) -o$@ $^ $(LDFLAGS)

rm:
	@$(RM) \
	$(wildcard *.o) \
	$(wildcard *.d) \
	$(wildcard *.a) \
	$(TARGET)

-include $(DEPFILES)
