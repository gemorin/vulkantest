CXX=clang++
GLFWDIR=/Users/guillaume/dev/glfw

VULKANDIR=/Users/guillaume/dev/vulkansdk-macos-1.0.69.0
SHADERCOMPILER=$(VULKANDIR)/macOS/bin/glslc
VULKANLIBPATH=$(VULKANDIR)/macOS/lib
VULKANINC=$(VULKANDIR)/macOS/include

INCS=-I$(GLFWDIR)/include/GLFW -I$(VULKANINC)
CXXFLAGS=-Wall -W $(INCS) -std=c++14 -g -O2
LDFLAGS=-O2 -L$(GLFWDIR)/src -L $(VULKANLIBPATH) -framework Cocoa -framework Metal -framework IOSurface -rpath $(VULKANLIBPATH) -lglfw -lvulkan

all: vulkantest fragment.spv vertex.spv
main: vulkantest.cpp

fragment.spv: fragment.glsl
	$(SHADERCOMPILER) -fshader-stage=fragment -o $@ $<

vertex.spv: vertex.glsl
	$(SHADERCOMPILER) -fshader-stage=vertex -o $@ $<


clean:
	rm -rf *.o vulkantest vulkantest.dSYM *.spv
