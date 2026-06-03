/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "vk_rhi.h"
#include "vk_rhi_internal.h"

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

vkRhi_t vk;

PFN_vkGetInstanceProcAddr pfn_vkGetInstanceProcAddr;
PFN_vkCreateInstance pfn_vkCreateInstance;
PFN_vkDestroyInstance pfn_vkDestroyInstance;
PFN_vkEnumeratePhysicalDevices pfn_vkEnumeratePhysicalDevices;
PFN_vkGetPhysicalDeviceProperties pfn_vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties pfn_vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR pfn_vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pfn_vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pfn_vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkDestroySurfaceKHR pfn_vkDestroySurfaceKHR;
PFN_vkEnumerateDeviceExtensionProperties pfn_vkEnumerateDeviceExtensionProperties;
PFN_vkGetPhysicalDeviceFeatures2 pfn_vkGetPhysicalDeviceFeatures2;
PFN_vkGetPhysicalDeviceProperties2 pfn_vkGetPhysicalDeviceProperties2;
PFN_vkGetPhysicalDeviceMemoryProperties pfn_vkGetPhysicalDeviceMemoryProperties;

PFN_vkCreateDevice pfn_vkCreateDevice;
PFN_vkDestroyDevice pfn_vkDestroyDevice;
PFN_vkGetDeviceQueue pfn_vkGetDeviceQueue;
PFN_vkCreateSwapchainKHR pfn_vkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR pfn_vkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR pfn_vkGetSwapchainImagesKHR;
PFN_vkCreateImageView pfn_vkCreateImageView;
PFN_vkDestroyImageView pfn_vkDestroyImageView;
PFN_vkCreateRenderPass pfn_vkCreateRenderPass;
PFN_vkDestroyRenderPass pfn_vkDestroyRenderPass;
PFN_vkCreateFramebuffer pfn_vkCreateFramebuffer;
PFN_vkDestroyFramebuffer pfn_vkDestroyFramebuffer;
PFN_vkCreateCommandPool pfn_vkCreateCommandPool;
PFN_vkDestroyCommandPool pfn_vkDestroyCommandPool;
PFN_vkAllocateCommandBuffers pfn_vkAllocateCommandBuffers;
PFN_vkFreeCommandBuffers pfn_vkFreeCommandBuffers;
PFN_vkCreateSemaphore pfn_vkCreateSemaphore;
PFN_vkDestroySemaphore pfn_vkDestroySemaphore;
PFN_vkCreateFence pfn_vkCreateFence;
PFN_vkDestroyFence pfn_vkDestroyFence;
PFN_vkAcquireNextImageKHR pfn_vkAcquireNextImageKHR;
PFN_vkQueueSubmit pfn_vkQueueSubmit;
PFN_vkQueuePresentKHR pfn_vkQueuePresentKHR;
PFN_vkBeginCommandBuffer pfn_vkBeginCommandBuffer;
PFN_vkEndCommandBuffer pfn_vkEndCommandBuffer;
PFN_vkCmdBeginRenderPass pfn_vkCmdBeginRenderPass;
PFN_vkCmdEndRenderPass pfn_vkCmdEndRenderPass;
PFN_vkWaitForFences pfn_vkWaitForFences;
PFN_vkResetFences pfn_vkResetFences;
PFN_vkDeviceWaitIdle pfn_vkDeviceWaitIdle;

PFN_vkGetDeviceProcAddr pfn_vkGetDeviceProcAddr;

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
	pfn_vkGetPhysicalDeviceFeatures2 =
	    (PFN_vkGetPhysicalDeviceFeatures2)pfn_vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceFeatures2");
	pfn_vkGetPhysicalDeviceProperties2 =
	    (PFN_vkGetPhysicalDeviceProperties2)pfn_vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceProperties2");
	pfn_vkGetPhysicalDeviceMemoryProperties =
	    (PFN_vkGetPhysicalDeviceMemoryProperties)pfn_vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceMemoryProperties");
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

static qboolean vk_device_raytracing_features_ok(VkPhysicalDevice phys)
{
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeat;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat;
	VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bdaFeat;
	VkPhysicalDeviceFeatures2 feats2;

	if (!pfn_vkGetPhysicalDeviceFeatures2)
	{
		return qfalse;
	}

	Com_Memset(&rtFeat, 0, sizeof(rtFeat));
	rtFeat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	Com_Memset(&asFeat, 0, sizeof(asFeat));
	asFeat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	asFeat.pNext = &rtFeat;
	Com_Memset(&bdaFeat, 0, sizeof(bdaFeat));
	bdaFeat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
	bdaFeat.pNext = &asFeat;
	Com_Memset(&feats2, 0, sizeof(feats2));
	feats2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	feats2.pNext = &bdaFeat;

	pfn_vkGetPhysicalDeviceFeatures2(phys, &feats2);
	return (qboolean)(rtFeat.rayTracingPipeline && asFeat.accelerationStructure && bdaFeat.bufferDeviceAddress);
}

static qboolean vk_phys_has_raytracing_stack(VkPhysicalDevice phys)
{
	uint32_t i, n = 0;
	VkExtensionProperties *props;
	qboolean hasRtPipe = qfalse, hasAccel = qfalse, hasDefer = qfalse, hasBda = qfalse;

	pfn_vkEnumerateDeviceExtensionProperties(phys, NULL, &n, NULL);
	if (!n)
	{
		return qfalse;
	}
	props = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * n);
	pfn_vkEnumerateDeviceExtensionProperties(phys, NULL, &n, props);
	for (i = 0; i < n; i++)
	{
		if (!strcmp(props[i].extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
		{
			hasRtPipe = qtrue;
		}
		if (!strcmp(props[i].extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
		{
			hasAccel = qtrue;
		}
		if (!strcmp(props[i].extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME))
		{
			hasDefer = qtrue;
		}
		if (!strcmp(props[i].extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
		{
			hasBda = qtrue;
		}
	}
	free(props);
	return (qboolean)(hasRtPipe && hasAccel && hasDefer && hasBda);
}

static void vk_log_device_extensions(VkPhysicalDevice phys)
{
	uint32_t i, n = 0;
	VkExtensionProperties *props;
	qboolean hasRtPipe = qfalse, hasAccel = qfalse, hasDefer = qfalse, hasBda = qfalse, hasRayQuery = qfalse;

	vk.meshShaderExt = qfalse;
	vk.rayTracingExt = qfalse;

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
		if (!strcmp(props[i].extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
		{
			hasRtPipe = qtrue;
		}
		if (!strcmp(props[i].extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME))
		{
			hasRayQuery = qtrue;
		}
		if (!strcmp(props[i].extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
		{
			hasAccel = qtrue;
		}
		if (!strcmp(props[i].extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME))
		{
			hasDefer = qtrue;
		}
		if (!strcmp(props[i].extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
		{
			hasBda = qtrue;
		}
	}

	vk.rayTracingExt = hasRtPipe && hasAccel && hasDefer && hasBda;

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
		Com_Printf("Vulkan: KHR ray tracing pipeline stack available (enabled on device if supported)\n");
	}
	else if (hasRtPipe || hasRayQuery)
	{
		Com_Printf("Vulkan: partial ray tracing extensions only (need " VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME
		           ", " VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME ", " VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ")\n");
	}
	else
	{
		Com_Printf("Vulkan: hardware ray tracing extensions not reported\n");
	}

	free(props);
}

static qboolean vk_try_select_device(VkSurfaceKHR surface, qboolean requireRayTracing)
{
	uint32_t i, count = 0;
	VkPhysicalDevice *devices;
	VkPhysicalDeviceProperties props;

	pfn_vkEnumeratePhysicalDevices(vk.instance, &count, NULL);
	if (!count)
	{
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
		if (!vk_device_has_swapchain(devices[i]))
		{
			continue;
		}
		if (requireRayTracing)
		{
			if (!vk_phys_has_raytracing_stack(devices[i]) || !vk_device_raytracing_features_ok(devices[i]))
			{
				continue;
			}
		}
		vk.physicalDevice   = devices[i];
		vk.queueFamilyIndex = qf;
		pfn_vkGetPhysicalDeviceProperties(devices[i], &props);
		Com_Printf("Vulkan: using GPU %s%s\n", props.deviceName, requireRayTracing ? " (ray tracing capable)" : "");
		free(devices);
		return qtrue;
	}

	free(devices);
	return qfalse;
}

static qboolean vk_pick_physical(VkSurfaceKHR surface)
{
	if (pfn_vkGetPhysicalDeviceFeatures2 && vk_try_select_device(surface, qtrue))
	{
		return qtrue;
	}
	if (vk_try_select_device(surface, qfalse))
	{
		return qtrue;
	}
	Com_Printf(S_COLOR_RED "Vulkan: no suitable GPU with graphics+present (and swapchain)\n");
	return qfalse;
}

qboolean VkRHI_Init(struct SDL_Window *window)
{
	unsigned int extCount = 0;
	const char **sdlExts;
	const char *layers[] = { NULL };
	const char *reqExtsSwap[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	const char *reqExtsRt[]   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		                          VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		                          VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		                          VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		                          VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME };
	VkApplicationInfo appInfo;
	VkInstanceCreateInfo instInfo;
	VkDeviceQueueCreateInfo qInfo;
	float qprio = 1.f;
	VkDeviceCreateInfo devInfo;
	VkPhysicalDeviceVulkan12Features vk12Feat;
	VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeat;
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeat;
	VkPhysicalDeviceFeatures2 feats2;
	VkSurfaceCapabilitiesKHR caps;
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
	devInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devInfo.queueCreateInfoCount = 1;
	devInfo.pQueueCreateInfos    = &qInfo;
	devInfo.enabledLayerCount    = 0;

	if (vk.rayTracingExt && pfn_vkGetPhysicalDeviceFeatures2)
	{
		Com_Memset(&rtFeat, 0, sizeof(rtFeat));
		rtFeat.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		rtFeat.rayTracingPipeline = VK_TRUE;
		Com_Memset(&asFeat, 0, sizeof(asFeat));
		asFeat.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		asFeat.accelerationStructure = VK_TRUE;
		asFeat.pNext                 = &rtFeat;
		Com_Memset(&vk12Feat, 0, sizeof(vk12Feat));
		vk12Feat.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vk12Feat.bufferDeviceAddress = VK_TRUE;
		vk12Feat.pNext               = &asFeat;
		Com_Memset(&feats2, 0, sizeof(feats2));
		feats2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		feats2.pNext = &vk12Feat;
		devInfo.pNext                   = &feats2;
		devInfo.enabledExtensionCount   = ARRAY_LEN(reqExtsRt);
		devInfo.ppEnabledExtensionNames = reqExtsRt;
		res = pfn_vkCreateDevice(vk.physicalDevice, &devInfo, NULL, &vk.device);
		if (res != VK_SUCCESS)
		{
			Com_Printf(S_COLOR_YELLOW "Vulkan: vkCreateDevice with ray tracing failed (%d), retrying without RT\n", (int)res);
			vk.rayTracingExt = qfalse;
			Com_Memset(&devInfo, 0, sizeof(devInfo));
			devInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			devInfo.queueCreateInfoCount = 1;
			devInfo.pQueueCreateInfos    = &qInfo;
			devInfo.enabledLayerCount    = 0;
			devInfo.enabledExtensionCount   = ARRAY_LEN(reqExtsSwap);
			devInfo.ppEnabledExtensionNames = reqExtsSwap;
			res = pfn_vkCreateDevice(vk.physicalDevice, &devInfo, NULL, &vk.device);
		}
	}
	else
	{
		devInfo.enabledExtensionCount   = ARRAY_LEN(reqExtsSwap);
		devInfo.ppEnabledExtensionNames = reqExtsSwap;
		res = pfn_vkCreateDevice(vk.physicalDevice, &devInfo, NULL, &vk.device);
	}

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

	if (!VkRhi_CreateSwapchainResources(window, &caps))
	{
		return qfalse;
	}

	VkRhi_InitRayTracing();

	vk.active = qtrue;
	Com_Printf("Vulkan: RHI initialized (swapchain %ux%u, %u images)\n",
	           (unsigned)vk.swapchainExtent.width, (unsigned)vk.swapchainExtent.height,
	           (unsigned)vk.swapchainImageCount);
	return qtrue;
}

void VkRHI_WaitIdle(void)
{
	if (vk.device && pfn_vkDeviceWaitIdle)
	{
		pfn_vkDeviceWaitIdle(vk.device);
	}
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

		VkRhi_ShutdownRayTracing();

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
	VkPipelineStageFlags waitStage = vk.rayTracingActive ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR
	                                                      : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
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
