CXX=g++

#GLFWDIR=/usr/local/Cellar/glfw/3.2.1
# can't use glfw 3.2.1, moltenvk supported in HEAD so used
# brew install glfw --HEAD
GLFWDIR=/usr/local/Cellar/glfw/HEAD-7787973

MOLTENVK=/Users/guillaume/dev/Molten-0.19.0/MoltenVK
INCS=-I$(GLFWDIR)/include/GLFW -I$(MOLTENVK)/include
CXXFLAGS=-Wall -W $(INCS) -std=c++11 -g -O2
LDFLAGS=-L$(GLFWDIR)/lib -L$(MOLTENVK)/macOS -framework Cocoa -framework Metal -framework IOSurface -rpath $(MOLTENVK)/macOS -lMoltenVK -lglfw
SHADERCOMPILER=$(MOLTENVK)/../MoltenShaderConverter/Tools/MoltenShaderConverter

all: vulkantest fragment.spv vertex.spv
main: vulkantest.cpp

fragment.spv: fragment.glsl
	$(SHADERCOMPILER) -gi $< -t f -so $@

vertex.spv: vertex.glsl
	$(SHADERCOMPILER) -gi $< -t v -so $@


clean:
	rm -rf *.o vulkantest vulkantest.dSYM *.spv
