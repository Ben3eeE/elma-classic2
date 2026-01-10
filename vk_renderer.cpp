#include "vk_renderer.h"
#include "main.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <set>
#include <algorithm>

// Vertex shader SPIR-V (compiled from GLSL)
#include "shaders/vert.spv.h"

// Fragment shader SPIR-V (compiled from GLSL)
#include "shaders/frag.spv.h"

struct Vertex {
    float pos[2];
    float texCoord[2];
};

static const Vertex vertices[] = {
    {{-1.0f,  1.0f}, {0.0f, 0.0f}},
    {{-1.0f, -1.0f}, {0.0f, 1.0f}},
    {{ 1.0f, -1.0f}, {1.0f, 1.0f}},
    {{-1.0f,  1.0f}, {0.0f, 0.0f}},
    {{ 1.0f, -1.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  1.0f}, {1.0f, 0.0f}},
};

// Triple buffering like Metal
static const int MAX_FRAMES_IN_FLIGHT = 3;

static VkInstance instance = VK_NULL_HANDLE;
static VkSurfaceKHR surface = VK_NULL_HANDLE;
static VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
static VkDevice device = VK_NULL_HANDLE;
static VkQueue graphicsQueue = VK_NULL_HANDLE;
static VkQueue presentQueue = VK_NULL_HANDLE;
static VkSwapchainKHR swapchain = VK_NULL_HANDLE;
static std::vector<VkImage> swapchainImages;
static std::vector<VkImageView> swapchainImageViews;
static VkFormat swapchainImageFormat;
static VkExtent2D swapchainExtent;
static VkRenderPass renderPass = VK_NULL_HANDLE;
static VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
static VkPipeline graphicsPipeline = VK_NULL_HANDLE;
static std::vector<VkFramebuffer> swapchainFramebuffers;

// Per-frame resources for triple buffering
struct FrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
};
static FrameData frames[MAX_FRAMES_IN_FLIGHT];
static int currentFrame = 0;

static VkBuffer vertexBuffer = VK_NULL_HANDLE;
static VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

// Persistent mapped pointers to texture memory (like Metal's shared storage)
static void* stagingMapped = nullptr;
static void* paletteMapped = nullptr;

static VkImage indexTexture = VK_NULL_HANDLE;
static VkDeviceMemory indexTextureMemory = VK_NULL_HANDLE;
static VkImageView indexTextureView = VK_NULL_HANDLE;
static VkSampler indexSampler = VK_NULL_HANDLE;

static VkImage paletteTexture = VK_NULL_HANDLE;
static VkDeviceMemory paletteTextureMemory = VK_NULL_HANDLE;
static VkImageView paletteTextureView = VK_NULL_HANDLE;
static VkSampler paletteSampler = VK_NULL_HANDLE;

static VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
static VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
static VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

static int frameWidth = 0;
static int frameHeight = 0;
static uint32_t graphicsFamily = UINT32_MAX;
static uint32_t presentFamily = UINT32_MAX;

static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    internal_error("Failed to find suitable memory type");
    return 0;
}

static VkShaderModule createShaderModule(const uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = code;

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        internal_error("Failed to create shader module");
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                        VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                        VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        internal_error("Failed to create buffer");
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        internal_error("Failed to allocate buffer memory");
        return;
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

static void createImage(uint32_t width, uint32_t height, VkFormat format, 
                       VkImageTiling tiling, VkImageUsageFlags usage, 
                       VkMemoryPropertyFlags properties, VkImage& image, 
                       VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        internal_error("Failed to create image");
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        internal_error("Failed to allocate image memory");
        return;
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

static VkCommandBuffer beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = frames[0].commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

static void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, frames[0].commandPool, 1, &commandBuffer);
}

static void transitionImageLayout(VkImage image, VkFormat format, 
                                 VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && 
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
               newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        internal_error("Unsupported layout transition");
        return;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 
                        0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

static void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, 
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

int vk_init(SDL_Window* window, int width, int height) {
    frameWidth = width;
    frameHeight = height;

    // Create Vulkan instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Elma";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Get required extensions from SDL
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        internal_error("Failed to create Vulkan instance");
        return -1;
    }

    // Create surface
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        internal_error("Failed to create Vulkan surface:", SDL_GetError());
        return -1;
    }

    // Pick physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        internal_error("Failed to find GPUs with Vulkan support");
        return -1;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    physicalDevice = devices[0]; // Pick first device

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            presentFamily = i;
        }
        if (graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX) {
            break;
        }
    }

    if (graphicsFamily == UINT32_MAX || presentFamily == UINT32_MAX) {
        internal_error("Failed to find suitable queue families");
        return -1;
    }

    // Create logical device
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        internal_error("Failed to create logical device");
        return -1;
    }

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);

    // Create swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && 
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

    // Prefer IMMEDIATE for no VSync, fallback to MAILBOX, then FIFO
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // Always available
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            presentMode = mode;
            break;
        }
    }
    if (presentMode == VK_PRESENT_MODE_FIFO_KHR) {
        for (const auto& mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = mode;
                break;
            }
        }
    }

    swapchainExtent = capabilities.currentExtent;
    if (swapchainExtent.width == UINT32_MAX) {
        swapchainExtent.width = width;
        swapchainExtent.height = height;
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = swapchainExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};
    if (graphicsFamily != presentFamily) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain) != VK_SUCCESS) {
        internal_error("Failed to create swapchain");
        return -1;
    }

    swapchainImageFormat = surfaceFormat.format;

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

    // Create image views
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
            internal_error("Failed to create image views");
            return -1;
        }
    }

    // Create render pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        internal_error("Failed to create render pass");
        return -1;
    }

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding indexSamplerLayoutBinding{};
    indexSamplerLayoutBinding.binding = 0;
    indexSamplerLayoutBinding.descriptorCount = 1;
    indexSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    indexSamplerLayoutBinding.pImmutableSamplers = nullptr;
    indexSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding paletteSamplerLayoutBinding{};
    paletteSamplerLayoutBinding.binding = 1;
    paletteSamplerLayoutBinding.descriptorCount = 1;
    paletteSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    paletteSamplerLayoutBinding.pImmutableSamplers = nullptr;
    paletteSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {indexSamplerLayoutBinding, paletteSamplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        internal_error("Failed to create descriptor set layout");
        return -1;
    }

    // Create graphics pipeline
    VkShaderModule vertShaderModule = createShaderModule((uint32_t*)vert_spv, vert_spv_len);
    VkShaderModule fragShaderModule = createShaderModule((uint32_t*)frag_spv, frag_spv_len);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[2];
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, texCoord);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = (float)swapchainExtent.height;
    viewport.width = (float)swapchainExtent.width;
    viewport.height = -(float)swapchainExtent.height; // Negative to flip Y axis
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // CCW because negative viewport height flips winding
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        internal_error("Failed to create pipeline layout");
        return -1;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, 
                                  &graphicsPipeline) != VK_SUCCESS) {
        internal_error("Failed to create graphics pipeline");
        return -1;
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    // Create framebuffers
    swapchainFramebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkImageView attachments[] = {swapchainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, 
                               &swapchainFramebuffers[i]) != VK_SUCCESS) {
            internal_error("Failed to create framebuffer");
            return -1;
        }
    }

    // Create per-frame resources for triple buffering
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsFamily;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &frames[i].commandPool) != VK_SUCCESS) {
            internal_error("Failed to create command pool");
            return -1;
        }

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = frames[i].commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].commandBuffer) != VK_SUCCESS) {
            internal_error("Failed to allocate command buffer");
            return -1;
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &frames[i].inFlightFence) != VK_SUCCESS) {
            internal_error("Failed to create synchronization objects");
            return -1;
        }
    }

    // Create vertex buffer
    VkDeviceSize bufferSize = sizeof(vertices);
    createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexBuffer, vertexBufferMemory);

    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices, (size_t)bufferSize);
    vkUnmapMemory(device, vertexBufferMemory);

    // No staging buffers needed - we write directly to texture memory!

    // Create index texture with LINEAR tiling and host-visible memory (like Metal's shared storage)
    createImage(width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_LINEAR,
               VK_IMAGE_USAGE_SAMPLED_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
               indexTexture, indexTextureMemory);

    transitionImageLayout(indexTexture, VK_FORMAT_R8_UNORM, 
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    // Map the texture memory persistently
    vkMapMemory(device, indexTextureMemory, 0, width * height, 0, &stagingMapped);

    VkImageViewCreateInfo indexViewInfo{};
    indexViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    indexViewInfo.image = indexTexture;
    indexViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    indexViewInfo.format = VK_FORMAT_R8_UNORM;
    indexViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    indexViewInfo.subresourceRange.baseMipLevel = 0;
    indexViewInfo.subresourceRange.levelCount = 1;
    indexViewInfo.subresourceRange.baseArrayLayer = 0;
    indexViewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &indexViewInfo, nullptr, &indexTextureView) != VK_SUCCESS) {
        internal_error("Failed to create index texture view");
        return -1;
    }

    VkSamplerCreateInfo indexSamplerInfo{};
    indexSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    indexSamplerInfo.magFilter = VK_FILTER_NEAREST;
    indexSamplerInfo.minFilter = VK_FILTER_NEAREST;
    indexSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    indexSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    indexSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    indexSamplerInfo.anisotropyEnable = VK_FALSE;
    indexSamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    indexSamplerInfo.unnormalizedCoordinates = VK_FALSE;
    indexSamplerInfo.compareEnable = VK_FALSE;
    indexSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(device, &indexSamplerInfo, nullptr, &indexSampler) != VK_SUCCESS) {
        internal_error("Failed to create index sampler");
        return -1;
    }

    // Create palette texture with LINEAR tiling and host-visible memory (like Metal's shared storage)
    createImage(256, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR,
               VK_IMAGE_USAGE_SAMPLED_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               paletteTexture, paletteTextureMemory);

    transitionImageLayout(paletteTexture, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    // Map the palette texture memory persistently
    vkMapMemory(device, paletteTextureMemory, 0, 256 * 4, 0, &paletteMapped);

    VkImageViewCreateInfo paletteViewInfo{};
    paletteViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    paletteViewInfo.image = paletteTexture;
    paletteViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    paletteViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    paletteViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    paletteViewInfo.subresourceRange.baseMipLevel = 0;
    paletteViewInfo.subresourceRange.levelCount = 1;
    paletteViewInfo.subresourceRange.baseArrayLayer = 0;
    paletteViewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &paletteViewInfo, nullptr, &paletteTextureView) != VK_SUCCESS) {
        internal_error("Failed to create palette texture view");
        return -1;
    }

    VkSamplerCreateInfo paletteSamplerInfo{};
    paletteSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    paletteSamplerInfo.magFilter = VK_FILTER_NEAREST;
    paletteSamplerInfo.minFilter = VK_FILTER_NEAREST;
    paletteSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    paletteSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    paletteSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    paletteSamplerInfo.anisotropyEnable = VK_FALSE;
    paletteSamplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    paletteSamplerInfo.unnormalizedCoordinates = VK_FALSE;
    paletteSamplerInfo.compareEnable = VK_FALSE;
    paletteSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    if (vkCreateSampler(device, &paletteSamplerInfo, nullptr, &paletteSampler) != VK_SUCCESS) {
        internal_error("Failed to create palette sampler");
        return -1;
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSizes[1];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 2;

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = poolSizes;
    descriptorPoolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        internal_error("Failed to create descriptor pool");
        return -1;
    }

    // Create descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        internal_error("Failed to allocate descriptor sets");
        return -1;
    }

    VkDescriptorImageInfo indexImageInfo{};
    indexImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    indexImageInfo.imageView = indexTextureView;
    indexImageInfo.sampler = indexSampler;

    VkDescriptorImageInfo paletteImageInfo{};
    paletteImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    paletteImageInfo.imageView = paletteTextureView;
    paletteImageInfo.sampler = paletteSampler;

    VkWriteDescriptorSet descriptorWrites[2];
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &indexImageInfo;
    descriptorWrites[0].pNext = nullptr;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &paletteImageInfo;
    descriptorWrites[1].pNext = nullptr;

    vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);

    return 0;
}

void vk_upload_frame(const unsigned char* indices, int pitch) {
    // Direct memcpy to texture memory (like Metal's MTLStorageModeShared)
    // Need to handle potential row pitch from Vulkan
    VkImageSubresource subres{};
    subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(device, indexTexture, &subres, &layout);
    
    unsigned char* dst = (unsigned char*)stagingMapped;
    if (layout.rowPitch == frameWidth) {
        // Contiguous memory - single memcpy
        memcpy(dst, indices, frameWidth * frameHeight);
    } else {
        // Need to copy row by row
        for (int y = 0; y < frameHeight; y++) {
            memcpy(dst + y * layout.rowPitch, indices + y * frameWidth, frameWidth);
        }
    }
}

void vk_update_palette(const unsigned int* palette) {
    // Direct write to texture memory (like Metal's replaceRegion)
    VkImageSubresource subres{};
    subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(device, paletteTexture, &subres, &layout);
    
    unsigned char* rgba_palette = (unsigned char*)paletteMapped;
    for (int i = 0; i < 256; i++) {
        unsigned int color = palette[i];
        rgba_palette[i * 4 + 0] = (color >> 16) & 0xFF; // R
        rgba_palette[i * 4 + 1] = (color >> 8) & 0xFF;  // G
        rgba_palette[i * 4 + 2] = color & 0xFF;         // B
        rgba_palette[i * 4 + 3] = (color >> 24) & 0xFF; // A
    }
}

void vk_present() {
    // No fence wait - let frames queue up like Metal's async presentation
    FrameData& frame = frames[currentFrame];

    // Acquire next image (small timeout to avoid excessive skips)
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, 1000000, // 1ms timeout
                                            frame.imageAvailableSemaphore, 
                                            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_TIMEOUT || result == VK_NOT_READY) {
        // Skip frame if can't acquire
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        return;
    }
    
    // Reset fence only when we successfully acquire an image
    vkResetFences(device, 1, &frame.inFlightFence);

    // Record command buffer
    VkCommandBuffer commandBuffer = frame.commandBuffer;
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // No copy operations needed - textures are directly CPU-writable like Metal's shared storage!

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                           pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdDraw(commandBuffer, 6, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    vkEndCommandBuffer(commandBuffer);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {frame.imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = {frame.renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, frame.inFlightFence);

    // Present (non-blocking)
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);

    // Move to next frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void vk_cleanup() {
    vkDeviceWaitIdle(device);

    // Unmap texture memory
    if (stagingMapped != nullptr && indexTextureMemory != VK_NULL_HANDLE) {
        vkUnmapMemory(device, indexTextureMemory);
        stagingMapped = nullptr;
    }
    if (paletteMapped != nullptr && paletteTextureMemory != VK_NULL_HANDLE) {
        vkUnmapMemory(device, paletteTextureMemory);
        paletteMapped = nullptr;
    }

    // Clean up per-frame resources
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (frames[i].inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, frames[i].inFlightFence, nullptr);
        }
        if (frames[i].renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frames[i].renderFinishedSemaphore, nullptr);
        }
        if (frames[i].imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frames[i].imageAvailableSemaphore, nullptr);
        }
        if (frames[i].commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, frames[i].commandPool, nullptr);
        }
    }

    for (auto framebuffer : swapchainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
    }
    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    }
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    }
    if (paletteSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, paletteSampler, nullptr);
    }
    if (paletteTextureView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, paletteTextureView, nullptr);
    }
    if (paletteTexture != VK_NULL_HANDLE) {
        vkDestroyImage(device, paletteTexture, nullptr);
    }
    if (paletteTextureMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, paletteTextureMemory, nullptr);
    }
    if (indexSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, indexSampler, nullptr);
    }
    if (indexTextureView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, indexTextureView, nullptr);
    }
    if (indexTexture != VK_NULL_HANDLE) {
        vkDestroyImage(device, indexTexture, nullptr);
    }
    if (indexTextureMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexTextureMemory, nullptr);
    }
    // Staging buffers removed - writing directly to textures now
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
    }
    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
    }
}
