#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
using namespace std;

void errorCallback(int /*error*/, const char* description)
{
    puts(description);
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const VkSurfaceFormatKHR* formats,
                                           uint32_t formatCount)
{
    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        // Guaranteed to be avail in this case
        return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }
    for (uint32_t i = 0; i < formatCount; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM
         && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return formats[i];
    }
    return formats[0];
}

VkPresentModeKHR choosePresentMode(const VkPresentModeKHR* modes,
                                   uint32_t modeCount)
{
    // This is guaranteed to be avail per spec but can be buggy
    VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < modeCount; ++i) {
        if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            return VK_PRESENT_MODE_MAILBOX_KHR;
        if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
            bestMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    return bestMode;
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
    struct SwapChainDetails {
        // color depth
        VkSurfaceFormatKHR format;
        // how we display images
        VkPresentModeKHR presentMode;
        // size of the image
        VkExtent2D extent;
        // Images number
        uint32_t imageCount;
        VkSurfaceTransformFlagBitsKHR transform;
        bool set = false;
    } swapChainDetails;
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
                // * swap chain khr extension
                // * valid swap chain format/present mode
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
                if (!deviceQueuesFamilies.hasAll()) {
                    continue;
                }
                VkSurfaceCapabilitiesKHR capabilities;
                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice,
                                                          surface,
                                                          &capabilities);
                if (capabilities.currentExtent.width != UINT_MAX) {
                    swapChainDetails.extent = capabilities.currentExtent;
                }
                else {
                    swapChainDetails.extent.width =
                        max(capabilities.minImageExtent.width,
                            min(capabilities.maxImageExtent.width, 800U));
                    swapChainDetails.extent.height =
                        max(capabilities.minImageExtent.height,
                            min(capabilities.maxImageExtent.height, 600U));
                }
                uint32_t formatCount;
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                                     &formatCount, nullptr);
                if (0 == formatCount) {
                    continue;
                }
                VkSurfaceFormatKHR formats[formatCount];
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                                     &formatCount, formats);
                swapChainDetails.format = chooseSwapSurfaceFormat(formats,
                                                                  formatCount);

                uint32_t presentModeCount;
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
                                                          surface,
                                                          &presentModeCount,
                                                          nullptr);
                if (0 == presentModeCount) {
                    continue;
                }
                VkPresentModeKHR presentModes[presentModeCount];
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                                          &presentModeCount,
                                                          presentModes);
                swapChainDetails.presentMode = choosePresentMode(presentModes,
                                                                 presentModeCount);

                uint32_t imgCount = capabilities.minImageCount + 1;
                if (capabilities.maxImageCount > 0)
                    imgCount = min(capabilities.maxImageCount, imgCount);
                swapChainDetails.imageCount = imgCount;
                swapChainDetails.transform = capabilities.currentTransform;
                swapChainDetails.set = true;
            }
            if (swapChainDetails.set) {
                physicalDevice = devices[i];
                cout << "Using device " << properties.deviceName << endl;
                break;
            }
        }
    }
    if (!swapChainDetails.set) {
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
    VkSwapchainKHR swapChain;
    {
        VkSwapchainCreateInfoKHR createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = swapChainDetails.imageCount;
        createInfo.imageFormat = swapChainDetails.format.format;
        createInfo.imageColorSpace = swapChainDetails.format.colorSpace;
        createInfo.imageExtent = swapChainDetails.extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        const bool uniqueQueue = (deviceQueuesFamilies.numUnique() == 1);
        createInfo.imageSharingMode = uniqueQueue
                                    ? VK_SHARING_MODE_EXCLUSIVE
                                    : VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = uniqueQueue ? 0 : 2;
        createInfo.pQueueFamilyIndices = uniqueQueue
                                       ? nullptr
                                       : (uint32_t *) &deviceQueuesFamilies;
        createInfo.preTransform = swapChainDetails.transform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = swapChainDetails.presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkResult vkRet = vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr,
                                              &swapChain);
        if (vkRet != VK_SUCCESS) {
            printf("vkCreateSwapchainKHR failed with %d\n", vkRet);
            return 1;
        }
    }

    while(1) {
        glfwPollEvents();
        bool running = (glfwGetKey(window, GLFW_KEY_ESCAPE ) == GLFW_RELEASE);
        running &= !glfwWindowShouldClose(window);
        if (!running)
            break;
    }
    vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);

    glfwTerminate();

    return 0;
}
