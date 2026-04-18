/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "vk_rhi.h"

#include "../../qcommon/qcommon.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#ifndef ARRAY_LEN
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#endif

typedef struct
{
	qboolean active;

	struct SDL_Window *window;

	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	uint32_t queueFamilyIndex;

	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;
	uint32_t swapchainImageCount;
	VkImage *swapchainImages;
	VkImageView *swapchainImageViews;

	VkRenderPass renderPass;
	VkCommandPool commandPool;
	VkCommandBuffer *commandBuffers;
	VkFramebuffer *framebuffers;

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkFence inFlightFence;

	qboolean meshShaderExt;
	qboolean rayTracingExt;
} vkRhi_t;

static vkRhi_t vk;

static PFN_vkGetInstanceProcAddr pfn_vkGetInstanceProcAddr;
static PFN_vkCreateInstance pfn_vkCreateInstance;
static PFN_vkDestroyInstance pfn_vkDestroyInstance;
static PFN_vkEnumeratePhysicalDevices pfn_vkEnumeratePhysicalDevices;
static PFN_vkGetPhysicalDeviceProperties pfn_vkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties pfn_vkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR pfn_vkGetPhysicalDeviceSurfaceSupportKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pfn_vkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pfn_vkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkDestroySurfaceKHR pfn_vkDestroySurfaceKHR;
static PFN_vkEnumerateDeviceExtensionProperties pfn_vkEnumerateDeviceExtensionProperties;

static PFN_vkCreateDevice pfn_vkCreateDevice;
static PFN_vkDestroyDevice pfn_vkDestroyDevice;
static PFN_vkGetDeviceQueue pfn_vkGetDeviceQueue;
static PFN_vkCreateSwapchainKHR pfn_vkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR pfn_vkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR pfn_vkGetSwapchainImagesKHR;
static PFN_vkCreateImageView pfn_vkCreateImageView;
static PFN_vkDestroyImageView pfn_vkDestroyImageView;
static PFN_vkCreateRenderPass pfn_vkCreateRenderPass;
static PFN_vkDestroyRenderPass pfn_vkDestroyRenderPass;
static PFN_vkCreateFramebuffer pfn_vkCreateFramebuffer;
static PFN_vkDestroyFramebuffer pfn_vkDestroyFramebuffer;
static PFN_vkCreateCommandPool pfn_vkCreateCommandPool;
static PFN_vkDestroyCommandPool pfn_vkDestroyCommandPool;
static PFN_vkAllocateCommandBuffers pfn_vkAllocateCommandBuffers;
static PFN_vkFreeCommandBuffers pfn_vkFreeCommandBuffers;
static PFN_vkCreateSemaphore pfn_vkCreateSemaphore;
static PFN_vkDestroySemaphore pfn_vkDestroySemaphore;
static PFN_vkCreateFence pfn_vkCreateFence;
static PFN_vkDestroyFence pfn_vkDestroyFence;
static PFN_vkAcquireNextImageKHR pfn_vkAcquireNextImageKHR;
static PFN_vkQueueSubmit pfn_vkQueueSubmit;
static PFN_vkQueuePresentKHR pfn_vkQueuePresentKHR;
static PFN_vkBeginCommandBuffer pfn_vkBeginCommandBuffer;
static PFN_vkEndCommandBuffer pfn_vkEndCommandBuffer;
static PFN_vkCmdBeginRenderPass pfn_vkCmdBeginRenderPass;
static PFN_vkCmdEndRenderPass pfn_vkCmdEndRenderPass;
static PFN_vkWaitForFences pfn_vkWaitForFences;
static PFN_vkResetFences pfn_vkResetFences;
static PFN_vkDeviceWaitIdle pfn_vkDeviceWaitIdle;

static PFN_vkGetDeviceProcAddr pfn_vkGetDeviceProcAddr;

#define VK_LOAD_GLOBAL(name) \
	do { \
		name = (PFN_##name)pfn_vkGetInstanceProcAddr(VK_NULL_HANDLE, #name); \
		if (!name) { \
			Com_Printf(S_COLOR_YELLOW "Vulkan: missing global proc %s\n", #name); \
			return qfalse; \
		} \
	} while (0)

#define VK_LOAD_INSTANCE(inst, name) \
	do { \
		name = (PFN_##name)pfn_vkGetInstanceProcAddr(inst, #name); \
		if (!name) { \
			Com_Printf(S_COLOR_YELLOW "Vulkan: missing instance proc %s\n", #name); \
			return qfalse; \
		} \
	} while (0)

#define VK_LOAD_DEVICE(dev, name) \
	do { \
		name = (PFN_##name)pfn_vkGetDeviceProcAddr(dev, #name); \
		if (!name) { \
			Com_Printf(S_COLOR_YELLOW "Vulkan: missing device proc %s\n", #name); \
			return qfalse; \
		} \
	} while (0)

static qboolean vk_load_global(void)
{
	pfn_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
	if (!pfn_vkGetInstanceProcAddr)
	{
		Com_Printf(S_COLOR_RED "Vulkan: SDL_Vulkan_GetVkGetInstanceProcAddr returned NULL\n");
		return qfalse;
	}

	VK_LOAD_GLOBAL(vkCreateInstance);
	VK_LOAD_GLOBAL(vkDestroyInstance);
	VK_LOAD_GLOBAL(vkEnumeratePhysicalDevices);
	VK_LOAD_GLOBAL(vkGetPhysicalDeviceProperties);
	VK_LOAD_GLOBAL(vkGetPhysicalDeviceQueueFamilyProperties);
	VK_LOAD_GLOBAL(vkEnumerateDeviceExtensionProperties);
	return qtrue;
}

static qboolean vk_load_instance(VkInstance inst)
{
	VK_LOAD_INSTANCE(inst, vkGetPhysicalDeviceSurfaceSupportKHR);
	VK_LOAD_INSTANCE(inst, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
	VK_LOAD_INSTANCE(inst, vkGetPhysicalDeviceSurfaceFormatsKHR);
	VK_LOAD_INSTANCE(inst, vkGetPhysicalDeviceSurfacePresentModesKHR);
	VK_LOAD_INSTANCE(inst, vkDestroySurfaceKHR);
	VK_LOAD_INSTANCE(inst, vkCreateDevice);
	VK_LOAD_INSTANCE(inst, vkDestroyDevice);
	VK_LOAD_INSTANCE(inst, vkGetDeviceProcAddr);
	return qtrue;
}

static qboolean vk_load_device(VkDevice dev)
{
	VK_LOAD_DEVICE(dev, vkGetDeviceQueue);
	VK_LOAD_DEVICE(dev, vkCreateSwapchainKHR);
	VK_LOAD_DEVICE(dev, vkDestroySwapchainKHR);
	VK_LOAD_DEVICE(dev, vkGetSwapchainImagesKHR);
	VK_LOAD_DEVICE(dev, vkCreateImageView);
	VK_LOAD_DEVICE(dev, vkDestroyImageView);
	VK_LOAD_DEVICE(dev, vkCreateRenderPass);
	VK_LOAD_DEVICE(dev, vkDestroyRenderPass);
	VK_LOAD_DEVICE(dev, vkCreateFramebuffer);
	VK_LOAD_DEVICE(dev, vkDestroyFramebuffer);
	VK_LOAD_DEVICE(dev, vkCreateCommandPool);
	VK_LOAD_DEVICE(dev, vkDestroyCommandPool);
	VK_LOAD_DEVICE(dev, vkAllocateCommandBuffers);
	VK_LOAD_DEVICE(dev, vkFreeCommandBuffers);
	VK_LOAD_DEVICE(dev, vkCreateSemaphore);
	VK_LOAD_DEVICE(dev, vkDestroySemaphore);
	VK_LOAD_DEVICE(dev, vkCreateFence);
	VK_LOAD_DEVICE(dev, vkDestroyFence);
	VK_LOAD_DEVICE(dev, vkAcquireNextImageKHR);
	VK_LOAD_DEVICE(dev, vkQueueSubmit);
	VK_LOAD_DEVICE(dev, vkQueuePresentKHR);
	VK_LOAD_DEVICE(dev, vkBeginCommandBuffer);
	VK_LOAD_DEVICE(dev, vkEndCommandBuffer);
	VK_LOAD_DEVICE(dev, vkCmdBeginRenderPass);
	VK_LOAD_DEVICE(dev, vkCmdEndRenderPass);
	VK_LOAD_DEVICE(dev, vkWaitForFences);
	VK_LOAD_DEVICE(dev, vkResetFences);
	VK_LOAD_DEVICE(dev, vkDeviceWaitIdle);
	return qtrue;
}

static uint32_t vk_find_queue(VkPhysicalDevice phys, VkSurfaceKHR surface, uint32_t *outFamily)
{
	uint32_t i;
	uint32_t count = 0;
	VkQueueFamilyProperties *props;
	VkBool32 presentSupport = VK_FALSE;

	pfn_vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, NULL);
	if (!count)
	{
		return UINT32_MAX;
	}

	props = (VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties) * count);
	pfn_vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, props);

	for (i = 0; i < count; i++)
	{
		if ((props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
		{
			continue;
		}
		pfn_vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, surface, &presentSupport);
		if (presentSupport)
		{
			free(props);
			*outFamily = i;
			return i;
		}
	}

	free(props);
	return UINT32_MAX;
}

static VkSurfaceFormatKHR vk_choose_format(const VkSurfaceFormatKHR *formats, uint32_t n)
{
	uint32_t i;
	for (i = 0; i < n; i++)
	{
		if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return formats[i];
		}
	}
	return formats[0];
}

static VkPresentModeKHR vk_choose_present(const VkPresentModeKHR *modes, uint32_t n)
{
	uint32_t i;
	for (i = 0; i < n; i++)
	{
		if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return modes[i];
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

static qboolean vk_pick_physical(VkSurfaceKHR surface)
{
	uint32_t i, count = 0;
	VkPhysicalDevice *devices;
	VkPhysicalDeviceProperties props;

	pfn_vkEnumeratePhysicalDevices(vk.instance, &count, NULL);
	if (!count)
	{
		Com_Printf(S_COLOR_RED "Vulkan: no physical devices\n");
		return qfalse;
	}
	devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * count);
	pfn_vkEnumeratePhysicalDevices(vk.instance, &count, devices);

	for (i = 0; i < count; i++)
	{
		uint32_t qf = UINT32_MAX;
		if (vk_find_queue(devices[i], surface, &qf) == UINT32_MAX)
		{
			continue;
		}
		vk.physicalDevice   = devices[i];
		vk.queueFamilyIndex = qf;
		pfn_vkGetPhysicalDeviceProperties(devices[i], &props);
		Com_Printf("Vulkan: using GPU %s\n", props.deviceName);
		free(devices);
		return qtrue;
	}

	free(devices);
	Com_Printf(S_COLOR_RED "Vulkan: no suitable GPU with graphics+present\n");
	return qfalse;
}

static void vk_log_device_extensions(VkPhysicalDevice phys)
{
	uint32_t i, n = 0;
	VkExtensionProperties *props;

	vk.meshShaderExt   = qfalse;
	vk.rayTracingExt   = qfalse;

	pfn_vkEnumerateDeviceExtensionProperties(phys, NULL, &n, NULL);
	if (!n)
	{
		return;
	}
	props = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * n);
	pfn_vkEnumerateDeviceExtensionProperties(phys, NULL, &n, props);

	for (i = 0; i < n; i++)
	{
		if (!strcmp(props[i].extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME))
		{
			vk.meshShaderExt = qtrue;
		}
		if (!strcmp(props[i].extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) ||
		    !strcmp(props[i].extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME))
		{
			vk.rayTracingExt = qtrue;
		}
	}

	if (vk.meshShaderExt)
	{
		Com_Printf("Vulkan: optional " VK_EXT_MESH_SHADER_EXTENSION_NAME " available (pipeline not wired yet)\n");
	}
	else
	{
		Com_Printf("Vulkan: " VK_EXT_MESH_SHADER_EXTENSION_NAME " not reported by device\n");
	}

	if (vk.rayTracingExt)
	{
		Com_Printf("Vulkan: ray tracing extension(s) available (not enabled yet)\n");
	}
	else
	{
		Com_Printf("Vulkan: ray tracing extensions not reported by device\n");
	}

	free(props);
}

static qboolean vk_device_has_swapchain(VkPhysicalDevice phys)
{
	uint32_t i, n = 0;
	VkExtensionProperties *p;
	qboolean has = qfalse;

	pfn_vkEnumerateDeviceExtensionProperties(phys, NULL, &n, NULL);
	if (!n)
	{
		return qfalse;
	}
	p = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * n);
	pfn_vkEnumerateDeviceExtensionProperties(phys, NULL, &n, p);
	for (i = 0; i < n; i++)
	{
		if (!strcmp(p[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
		{
			has = qtrue;
			break;
		}
	}
	free(p);
	return has;
}

qboolean VkRHI_Init(struct SDL_Window *window)
{
	unsigned int extCount = 0;
	const char **sdlExts;
	const char *layers[] = { NULL };
	const char *reqExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkApplicationInfo appInfo;
	VkInstanceCreateInfo instInfo;
	VkDeviceQueueCreateInfo qInfo;
	float qprio = 1.f;
	VkDeviceCreateInfo devInfo;
	VkSwapchainCreateInfoKHR swInfo;
	VkSurfaceCapabilitiesKHR caps;
	uint32_t fmtCount = 0, modeCount = 0, imgCount = 0;
	VkSurfaceFormatKHR *formats;
	VkPresentModeKHR *modes;
	VkSurfaceFormatKHR surfaceFormat;
	VkPresentModeKHR presentMode;
	VkAttachmentDescription colorAtt;
	VkAttachmentReference colorRef;
	VkSubpassDescription subpass;
	VkRenderPassCreateInfo rpInfo;
	VkCommandPoolCreateInfo poolInfo;
	VkSemaphoreCreateInfo semInfo;
	VkFenceCreateInfo fenceInfo;
	uint32_t i;
	VkResult res;

	if (vk.active)
	{
		return qtrue;
	}

	vk.window = window;

	if (!vk_load_global())
	{
		return qfalse;
	}

	if (!SDL_Vulkan_GetInstanceExtensions(window, &extCount, NULL))
	{
		Com_Printf(S_COLOR_RED "Vulkan: SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
		return qfalse;
	}
	sdlExts = (const char **)malloc(sizeof(char *) * (extCount + 1));
	if (!SDL_Vulkan_GetInstanceExtensions(window, &extCount, sdlExts))
	{
		Com_Printf(S_COLOR_RED "Vulkan: SDL_Vulkan_GetInstanceExtensions(2) failed: %s\n", SDL_GetError());
		free(sdlExts);
		return qfalse;
	}
	sdlExts[extCount] = VK_KHR_SURFACE_EXTENSION_NAME;

	Com_Memset(&appInfo, 0, sizeof(appInfo));
	appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName   = "ET Legacy";
	appInfo.applicationVersion = VK_MAKE_VERSION(2, 60, 0);
	appInfo.pEngineName        = "ETL-Vulkan";
	appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion         = VK_API_VERSION_1_3;

	Com_Memset(&instInfo, 0, sizeof(instInfo));
	instInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pApplicationInfo        = &appInfo;
	instInfo.enabledExtensionCount   = extCount + 1;
	instInfo.ppEnabledExtensionNames = sdlExts;
	instInfo.enabledLayerCount       = 0;
	instInfo.ppEnabledLayerNames     = layers;

	res = pfn_vkCreateInstance(&instInfo, NULL, &vk.instance);
	free(sdlExts);
	if (res != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Vulkan: vkCreateInstance failed (%d)\n", (int)res);
		return qfalse;
	}

	if (!vk_load_instance(vk.instance))
	{
		pfn_vkDestroyInstance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
		return qfalse;
	}

	if (!SDL_Vulkan_CreateSurface(window, vk.instance, &vk.surface))
	{
		Com_Printf(S_COLOR_RED "Vulkan: SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
		pfn_vkDestroyInstance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
		return qfalse;
	}

	if (!vk_pick_physical(vk.surface))
	{
		pfn_vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
		vk.surface = VK_NULL_HANDLE;
		pfn_vkDestroyInstance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
		return qfalse;
	}

	vk_log_device_extensions(vk.physicalDevice);

	if (!vk_device_has_swapchain(vk.physicalDevice))
	{
		Com_Printf(S_COLOR_RED "Vulkan: device missing " VK_KHR_SWAPCHAIN_EXTENSION_NAME "\n");
		pfn_vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
		vk.surface = VK_NULL_HANDLE;
		pfn_vkDestroyInstance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
		return qfalse;
	}

	Com_Memset(&qInfo, 0, sizeof(qInfo));
	qInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qInfo.queueFamilyIndex = vk.queueFamilyIndex;
	qInfo.queueCount       = 1;
	qInfo.pQueuePriorities = &qprio;

	Com_Memset(&devInfo, 0, sizeof(devInfo));
	devInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devInfo.queueCreateInfoCount    = 1;
	devInfo.pQueueCreateInfos       = &qInfo;
	devInfo.enabledExtensionCount   = ARRAY_LEN(reqExts);
	devInfo.ppEnabledExtensionNames = reqExts;
	devInfo.enabledLayerCount       = 0;

	res = pfn_vkCreateDevice(vk.physicalDevice, &devInfo, NULL, &vk.device);
	if (res != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Vulkan: vkCreateDevice failed (%d)\n", (int)res);
		pfn_vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
		vk.surface = VK_NULL_HANDLE;
		pfn_vkDestroyInstance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
		return qfalse;
	}

	pfn_vkGetDeviceProcAddr =
	    (PFN_vkGetDeviceProcAddr)pfn_vkGetInstanceProcAddr(vk.instance, "vkGetDeviceProcAddr");
	if (!pfn_vkGetDeviceProcAddr)
	{
		Com_Printf(S_COLOR_RED "Vulkan: vkGetDeviceProcAddr unavailable\n");
		pfn_vkDestroyDevice(vk.device, NULL);
		vk.device = VK_NULL_HANDLE;
		pfn_vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
		vk.surface = VK_NULL_HANDLE;
		pfn_vkDestroyInstance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
		return qfalse;
	}

	if (!vk_load_device(vk.device))
	{
		pfn_vkDestroyDevice(vk.device, NULL);
		vk.device = VK_NULL_HANDLE;
		pfn_vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
		vk.surface = VK_NULL_HANDLE;
		pfn_vkDestroyInstance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
		return qfalse;
	}

	pfn_vkGetDeviceQueue(vk.device, vk.queueFamilyIndex, 0, &vk.graphicsQueue);

	pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physicalDevice, vk.surface, &caps);

	pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &fmtCount, NULL);
	if (!fmtCount)
	{
		Com_Printf(S_COLOR_RED "Vulkan: no surface formats\n");
		VkRHI_Shutdown();
		return qfalse;
	}
	formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * fmtCount);
	pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &fmtCount, formats);
	surfaceFormat = vk_choose_format(formats, fmtCount);
	free(formats);

	pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &modeCount, NULL);
	modes = (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * modeCount);
	pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &modeCount, modes);
	presentMode = vk_choose_present(modes, modeCount);
	free(modes);

	if (caps.currentExtent.width != UINT32_MAX)
	{
		vk.swapchainExtent = caps.currentExtent;
	}
	else
	{
		int w = 0, h = 0;
		SDL_GetWindowSize(window, &w, &h);
		vk.swapchainExtent.width  = (uint32_t)w;
		vk.swapchainExtent.height = (uint32_t)h;
	}

	imgCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount)
	{
		imgCount = caps.maxImageCount;
	}

	Com_Memset(&swInfo, 0, sizeof(swInfo));
	swInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swInfo.surface          = vk.surface;
	swInfo.minImageCount    = imgCount;
	swInfo.imageFormat      = surfaceFormat.format;
	swInfo.imageColorSpace  = surfaceFormat.colorSpace;
	swInfo.imageExtent      = vk.swapchainExtent;
	swInfo.imageArrayLayers = 1;
	swInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swInfo.preTransform     = caps.currentTransform;
	swInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swInfo.presentMode      = presentMode;
	swInfo.clipped          = VK_TRUE;
	swInfo.oldSwapchain     = VK_NULL_HANDLE;

	res = pfn_vkCreateSwapchainKHR(vk.device, &swInfo, NULL, &vk.swapchain);
	if (res != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Vulkan: vkCreateSwapchainKHR failed (%d)\n", (int)res);
		VkRHI_Shutdown();
		return qfalse;
	}

	vk.swapchainImageFormat = surfaceFormat.format;

	pfn_vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchainImageCount, NULL);
	vk.swapchainImages = (VkImage *)malloc(sizeof(VkImage) * vk.swapchainImageCount);
	pfn_vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchainImageCount, vk.swapchainImages);

	vk.swapchainImageViews = (VkImageView *)calloc(vk.swapchainImageCount, sizeof(VkImageView));
	for (i = 0; i < vk.swapchainImageCount; i++)
	{
		VkImageViewCreateInfo iv = { 0 };
		iv.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		iv.image                           = vk.swapchainImages[i];
		iv.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		iv.format                          = vk.swapchainImageFormat;
		iv.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		iv.subresourceRange.baseMipLevel   = 0;
		iv.subresourceRange.levelCount   = 1;
		iv.subresourceRange.baseArrayLayer = 0;
		iv.subresourceRange.layerCount     = 1;
		res = pfn_vkCreateImageView(vk.device, &iv, NULL, &vk.swapchainImageViews[i]);
		if (res != VK_SUCCESS)
		{
			Com_Printf(S_COLOR_RED "Vulkan: vkCreateImageView failed (%d)\n", (int)res);
			VkRHI_Shutdown();
			return qfalse;
		}
	}

	Com_Memset(&colorAtt, 0, sizeof(colorAtt));
	colorAtt.format         = vk.swapchainImageFormat;
	colorAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
	colorAtt.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAtt.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
	colorAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAtt.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAtt.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	colorRef.attachment = 0;
	colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Com_Memset(&subpass, 0, sizeof(subpass));
	subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments    = &colorRef;

	Com_Memset(&rpInfo, 0, sizeof(rpInfo));
	rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.attachmentCount = 1;
	rpInfo.pAttachments    = &colorAtt;
	rpInfo.subpassCount    = 1;
	rpInfo.pSubpasses      = &subpass;

	res = pfn_vkCreateRenderPass(vk.device, &rpInfo, NULL, &vk.renderPass);
	if (res != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_RED "Vulkan: vkCreateRenderPass failed (%d)\n", (int)res);
		VkRHI_Shutdown();
		return qfalse;
	}

	vk.framebuffers = (VkFramebuffer *)calloc(vk.swapchainImageCount, sizeof(VkFramebuffer));
	for (i = 0; i < vk.swapchainImageCount; i++)
	{
		VkFramebufferCreateInfo fb = { 0 };
		fb.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fb.renderPass      = vk.renderPass;
		fb.attachmentCount = 1;
		fb.pAttachments    = &vk.swapchainImageViews[i];
		fb.width           = vk.swapchainExtent.width;
		fb.height          = vk.swapchainExtent.height;
		fb.layers          = 1;
		res = pfn_vkCreateFramebuffer(vk.device, &fb, NULL, &vk.framebuffers[i]);
		if (res != VK_SUCCESS)
		{
			Com_Printf(S_COLOR_RED "Vulkan: vkCreateFramebuffer failed (%d)\n", (int)res);
			VkRHI_Shutdown();
			return qfalse;
		}
	}

	Com_Memset(&poolInfo, 0, sizeof(poolInfo));
	poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = vk.queueFamilyIndex;
	res = pfn_vkCreateCommandPool(vk.device, &poolInfo, NULL, &vk.commandPool);
	if (res != VK_SUCCESS)
	{
		VkRHI_Shutdown();
		return qfalse;
	}

	vk.commandBuffers = (VkCommandBuffer *)calloc(vk.swapchainImageCount, sizeof(VkCommandBuffer));
	{
		VkCommandBufferAllocateInfo alloc = { 0 };
		alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandPool        = vk.commandPool;
		alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc.commandBufferCount = vk.swapchainImageCount;
		res = pfn_vkAllocateCommandBuffers(vk.device, &alloc, vk.commandBuffers);
		if (res != VK_SUCCESS)
		{
			VkRHI_Shutdown();
			return qfalse;
		}
	}

	for (i = 0; i < vk.swapchainImageCount; i++)
	{
		VkCommandBufferBeginInfo bi     = { 0 };
		VkRenderPassBeginInfo rpBegin   = { 0 };
		VkClearValue clearVal;

		bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		bi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		res = pfn_vkBeginCommandBuffer(vk.commandBuffers[i], &bi);
		if (res != VK_SUCCESS)
		{
			VkRHI_Shutdown();
			return qfalse;
		}

		clearVal.color.float32[0] = 0.02f;
		clearVal.color.float32[1] = 0.05f;
		clearVal.color.float32[2] = 0.12f;
		clearVal.color.float32[3] = 1.f;

		rpBegin.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpBegin.renderPass       = vk.renderPass;
		rpBegin.framebuffer      = vk.framebuffers[i];
		rpBegin.renderArea.offset = (VkOffset2D){ 0, 0 };
		rpBegin.renderArea.extent = vk.swapchainExtent;
		rpBegin.clearValueCount  = 1;
		rpBegin.pClearValues     = &clearVal;

		pfn_vkCmdBeginRenderPass(vk.commandBuffers[i], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
		pfn_vkCmdEndRenderPass(vk.commandBuffers[i]);
		res = pfn_vkEndCommandBuffer(vk.commandBuffers[i]);
		if (res != VK_SUCCESS)
		{
			VkRHI_Shutdown();
			return qfalse;
		}
	}

	Com_Memset(&semInfo, 0, sizeof(semInfo));
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	res = pfn_vkCreateSemaphore(vk.device, &semInfo, NULL, &vk.imageAvailableSemaphore);
	if (res != VK_SUCCESS)
	{
		VkRHI_Shutdown();
		return qfalse;
	}
	res = pfn_vkCreateSemaphore(vk.device, &semInfo, NULL, &vk.renderFinishedSemaphore);
	if (res != VK_SUCCESS)
	{
		VkRHI_Shutdown();
		return qfalse;
	}

	Com_Memset(&fenceInfo, 0, sizeof(fenceInfo));
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	res = pfn_vkCreateFence(vk.device, &fenceInfo, NULL, &vk.inFlightFence);
	if (res != VK_SUCCESS)
	{
		VkRHI_Shutdown();
		return qfalse;
	}

	vk.active = qtrue;
	Com_Printf("Vulkan: RHI initialized (swapchain %ux%u, %u images)\n",
	           (unsigned)vk.swapchainExtent.width, (unsigned)vk.swapchainExtent.height,
	           (unsigned)vk.swapchainImageCount);
	return qtrue;
}

void VkRHI_Shutdown(void)
{
	uint32_t i;

	if (!vk.instance && !vk.device)
	{
		Com_Memset(&vk, 0, sizeof(vk));
		return;
	}

	if (vk.device)
	{
		pfn_vkDeviceWaitIdle(vk.device);

		if (vk.inFlightFence)
		{
			pfn_vkDestroyFence(vk.device, vk.inFlightFence, NULL);
			vk.inFlightFence = VK_NULL_HANDLE;
		}
		if (vk.imageAvailableSemaphore)
		{
			pfn_vkDestroySemaphore(vk.device, vk.imageAvailableSemaphore, NULL);
			vk.imageAvailableSemaphore = VK_NULL_HANDLE;
		}
		if (vk.renderFinishedSemaphore)
		{
			pfn_vkDestroySemaphore(vk.device, vk.renderFinishedSemaphore, NULL);
			vk.renderFinishedSemaphore = VK_NULL_HANDLE;
		}

		if (vk.renderPass)
		{
			pfn_vkDestroyRenderPass(vk.device, vk.renderPass, NULL);
			vk.renderPass = VK_NULL_HANDLE;
		}

		if (vk.framebuffers)
		{
			for (i = 0; i < vk.swapchainImageCount; i++)
			{
				if (vk.framebuffers[i])
				{
					pfn_vkDestroyFramebuffer(vk.device, vk.framebuffers[i], NULL);
				}
			}
			free(vk.framebuffers);
			vk.framebuffers = NULL;
		}

		if (vk.commandPool && vk.commandBuffers)
		{
			pfn_vkFreeCommandBuffers(vk.device, vk.commandPool, vk.swapchainImageCount, vk.commandBuffers);
		}
		if (vk.commandBuffers)
		{
			free(vk.commandBuffers);
			vk.commandBuffers = NULL;
		}
		if (vk.commandPool)
		{
			pfn_vkDestroyCommandPool(vk.device, vk.commandPool, NULL);
			vk.commandPool = VK_NULL_HANDLE;
		}

		if (vk.swapchainImageViews)
		{
			for (i = 0; i < vk.swapchainImageCount; i++)
			{
				if (vk.swapchainImageViews[i])
				{
					pfn_vkDestroyImageView(vk.device, vk.swapchainImageViews[i], NULL);
				}
			}
			free(vk.swapchainImageViews);
			vk.swapchainImageViews = NULL;
		}

		if (vk.swapchain)
		{
			pfn_vkDestroySwapchainKHR(vk.device, vk.swapchain, NULL);
			vk.swapchain = VK_NULL_HANDLE;
		}

		if (vk.swapchainImages)
		{
			free(vk.swapchainImages);
			vk.swapchainImages = NULL;
		}
		vk.swapchainImageCount = 0;

		pfn_vkDestroyDevice(vk.device, NULL);
		vk.device = VK_NULL_HANDLE;
	}

	if (vk.instance)
	{
		if (vk.surface)
		{
			pfn_vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
			vk.surface = VK_NULL_HANDLE;
		}
		pfn_vkDestroyInstance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
	}

	vk.window = NULL;
	vk.active = qfalse;
}

void VkRHI_SwapFrame(void)
{
	uint32_t imageIndex = 0;
	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo;
	VkPresentInfoKHR presentInfo;
	VkResult res;

	if (!vk.active || !vk.device)
	{
		return;
	}

	pfn_vkWaitForFences(vk.device, 1, &vk.inFlightFence, VK_TRUE, UINT64_MAX);
	pfn_vkResetFences(vk.device, 1, &vk.inFlightFence);

	res = pfn_vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.imageAvailableSemaphore,
	                                VK_NULL_HANDLE, &imageIndex);
	if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
	{
		Com_Printf(S_COLOR_YELLOW "Vulkan: vkAcquireNextImageKHR -> %d\n", (int)res);
		return;
	}

	Com_Memset(&submitInfo, 0, sizeof(submitInfo));
	submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount   = 1;
	submitInfo.pWaitSemaphores      = &vk.imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask    = &waitStage;
	submitInfo.commandBufferCount   = 1;
	submitInfo.pCommandBuffers      = &vk.commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores    = &vk.renderFinishedSemaphore;

	res = pfn_vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, vk.inFlightFence);
	if (res != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_YELLOW "Vulkan: vkQueueSubmit -> %d\n", (int)res);
		return;
	}

	Com_Memset(&presentInfo, 0, sizeof(presentInfo));
	presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores    = &vk.renderFinishedSemaphore;
	presentInfo.swapchainCount     = 1;
	presentInfo.pSwapchains        = &vk.swapchain;
	presentInfo.pImageIndices      = &imageIndex;

	res = pfn_vkQueuePresentKHR(vk.graphicsQueue, &presentInfo);
	if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
	{
		Com_Printf(S_COLOR_YELLOW "Vulkan: vkQueuePresentKHR -> %d\n", (int)res);
	}
}

qboolean VkRHI_IsActive(void)
{
	return vk.active;
}
