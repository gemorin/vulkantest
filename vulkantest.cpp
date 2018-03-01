#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdio>
#include <iostream>
using namespace std;

void errorCallback(int error, const char* description)
{
    puts(description);
}

int main()
{
    GLFWwindow *window = nullptr;
    {
        // Init glfw
        glfwInit();
        glfwSetErrorCallback(errorCallback);

        // This prevents glfw from creating a gl context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);

        VkExtensionProperties properties[16];
        uint32_t extensionCount = 16;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                               properties);

        std::cout << extensionCount << " extensions supported: ";
        for (unsigned i = 0; i < extensionCount; ++i) {
            if (i)
                cout << ", ";
            cout << properties[i].extensionName;
        }
        cout << endl;
        if (glfwVulkanSupported() != GLFW_TRUE) {
            puts("glfw does not support vulkan! Upgrade to the latest version");
            return 1;
        }
    }

    // Init vulkan
    // Create Instance
    VkInstance instance;
    {
        VkApplicationInfo appInfo;
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "DeadCanard's Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(
                                                          &glfwExtensionCount);
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0;

        VkResult vkRet = vkCreateInstance(&createInfo, nullptr, &instance);
        if (vkRet != VK_SUCCESS) {
            printf("vkCreateInstance failed with %d\n", vkRet);
            return 1;
        }
    }

    // Setup debug cb
    // FIXME

    // Create window surface
    VkSurfaceKHR surface;
    {
        VkResult vkRet = glfwCreateWindowSurface(instance, window, nullptr,
                                                 &surface);
        if (vkRet != VK_SUCCESS) {
            printf("failed to create surface: glfwCreateWindowSurface() failed "
                   "with %d\n", vkRet);
            return 1;
        }
    }

    // Choose physical device
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceFeatures deviceFeatures;
    struct QueueFamilies {
        int graphicsFamily = -1;
        int presentationFamily = -1;

        bool hasAll() const {
            return graphicsFamily != -1 && presentationFamily != -1;
        }
        size_t numUnique() const {
            if (graphicsFamily != presentationFamily)
                return 2;
            return 1;
        }
        int getUnique(unsigned idx) const {
            if (idx)
                return presentationFamily;
            return graphicsFamily;
        }
    } deviceQueuesFamilies;
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            puts("Found no physical device");
            return 1;
        }
        VkPhysicalDevice devices[deviceCount];
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices);
        cout << "Found " << deviceCount << " device(s)" << endl;
        for (unsigned i = 0; i < deviceCount; ++i) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(devices[i], &properties);

            if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                // Only consider discreate cpu type
                continue;
            }
            vkGetPhysicalDeviceFeatures(devices[i], &deviceFeatures);


            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(devices[i],
                                                     &queueFamilyCount,
                                                     nullptr);
            VkQueueFamilyProperties queueProps[queueFamilyCount];
            vkGetPhysicalDeviceQueueFamilyProperties(devices[i],
                                                     &queueFamilyCount,
                                                     queueProps);
            for (unsigned j = 0; j != queueFamilyCount; ++j) {
                if (queueProps[j].queueCount <= 0)
                    continue;
                // We're looking for a device that has 
                // * graphics family queue
                // * presentation family queue
                // * swap chain khr extention
                uint32_t extensionCount;
                vkEnumerateDeviceExtensionProperties(devices[i], nullptr,
                                                     &extensionCount,
                                                     nullptr);
                if (extensionCount == 0)
                    continue;

                VkExtensionProperties extProps[extensionCount];
                vkEnumerateDeviceExtensionProperties(devices[i], nullptr,
                                                     &extensionCount,
                                                     extProps);
                bool hasSwapChain = false;
                for (unsigned k = 0; k < extensionCount; ++k) {
                    if (0 == strcmp(extProps[k].extensionName,
                                    VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                        hasSwapChain = true;
                        break;
                    }
                }
                if (!hasSwapChain)
                    continue;

                if (queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    deviceQueuesFamilies.graphicsFamily = j;
                }
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, surface,
                                                     &presentSupport);
                if (presentSupport) {
                    deviceQueuesFamilies.presentationFamily = j;
                }
                if (deviceQueuesFamilies.hasAll()) {
                    break;
                }
            }
            if (deviceQueuesFamilies.hasAll()) {
                physicalDevice = devices[i];
                cout << "Using device " << properties.deviceName << endl;
                break;
            }
        }
    }
    if (physicalDevice == VK_NULL_HANDLE || !deviceQueuesFamilies.hasAll()) {
        puts("Could not find a suitable device");
        return 1;
    }

    // Create logical device
    VkDevice logicalDevice;
    VkQueue presentationQueue;
    VkQueue graphicsQueue;
    {
        VkDeviceQueueCreateInfo queueCreateInfo[
                                        deviceQueuesFamilies.numUnique()];
        float priority[1] = {1.0f};
        for (unsigned i = 0; i < deviceQueuesFamilies.numUnique(); ++i) {
            queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo[i].queueFamilyIndex = deviceQueuesFamilies.getUnique(i);
            // Only one queue of each type
            queueCreateInfo[i].queueCount = 1;
            queueCreateInfo[i].pQueuePriorities = priority;
        }

        // We already check what we need is there
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                             &extensionCount,
                                             nullptr);
        VkExtensionProperties extProps[extensionCount];
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr,
                                             &extensionCount,
                                             extProps);
        char *extNames[extensionCount];
        for (unsigned i = 0; i < extensionCount; ++i) {
            extNames[i] = extProps[i].extensionName;
        }

        VkDeviceCreateInfo createInfo;
        createInfo.pQueueCreateInfos = queueCreateInfo;
        createInfo.queueCreateInfoCount = deviceQueuesFamilies.numUnique();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = extNames;
        // FIXME validation here at some point
        createInfo.enabledLayerCount = 0;

        VkResult vkRet = vkCreateDevice(physicalDevice, &createInfo, nullptr,
                                        &logicalDevice);
        if (vkRet != VK_SUCCESS) {
            printf("vkCreateDevice failed with %d\n", vkRet);
            return 1;
        }
        vkGetDeviceQueue(logicalDevice,
                         deviceQueuesFamilies.presentationFamily,
                         0, &presentationQueue);
        vkGetDeviceQueue(logicalDevice,
                         deviceQueuesFamilies.graphicsFamily, 0,
                         &graphicsQueue);
    }

    // Swap chain
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                                  &capabilities);
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                              &formatCount, nullptr);
        if (0 == formatCount) {
            puts("No format supported");
            return 1;
        }
        VkSurfaceFormatKHR formats[formatCount];
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                             &formatCount, formats);

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                                  &presentModeCount, nullptr);
        if (0 == presentModeCount) {
            puts("No present modes");
            return 1;
        }
        VkPresentModeKHR presentModes[presentModeCount];
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                                  &presentModeCount,
                                                  presentModes);
    }

    while(1) {
        glfwPollEvents();
        bool running = (glfwGetKey(window, GLFW_KEY_ESCAPE ) == GLFW_RELEASE);
        running &= !glfwWindowShouldClose(window);
        if (!running)
            break;
    }
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}
