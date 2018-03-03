CXX=clang++

#GLFWDIR=/usr/local/Cellar/glfw/3.2.1
# can't use glfw 3.2.1, moltenvk supported in HEAD: brew install glfw --HEAD
GLFWDIR=/usr/local/Cellar/glfw/HEAD-7787973

#MOLTENDIR=/Users/guillaume/dev/Molten-0.19.0
MOLTENDIR=/Users/guillaume/dev/MoltenVK
MOLTENPKG=$(MOLTENDIR)/Package/Release
MOLTENVK=$(MOLTENPKG)/MoltenVK
SHADERCOMPILER=$(MOLTENPKG)/MoltenVKShaderConverter/Tools/MoltenVKShaderConverter

INCS=-I$(GLFWDIR)/include/GLFW -I$(MOLTENVK)/include
CXXFLAGS=-Wall -W $(INCS) -std=c++14 -g -O2
LDFLAGS=-L$(GLFWDIR)/lib -L$(MOLTENVK)/macOS -framework Cocoa -framework Metal -framework IOSurface -rpath $(MOLTENVK)/macOS -lMoltenVK -lglfw

all: vulkantest fragment.spv vertex.spv
main: vulkantest.cpp

fragment.spv: fragment.glsl
	$(SHADERCOMPILER) -t f -gi $< -so $@

vertex.spv: vertex.glsl
	$(SHADERCOMPILER) -t v -gi $< -so $@


clean:
	rm -rf *.o vulkantest vulkantest.dSYM *.spv
