/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Shared Vulkan RHI state and loader symbols for vk_rhi.c and vk_rhi_swapchain.c.
 * Not a public API — include only from these translation units.
 */

#ifndef VK_RHI_INTERNAL_H
#define VK_RHI_INTERNAL_H

#include "../../qcommon/q_shared.h"

struct SDL_Window;

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

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
	/** VK_KHR_ray_tracing_pipeline + deps enabled on device (may still be qfalse if init failed). */
	qboolean rayTracingActive;
} vkRhi_t;

extern vkRhi_t vk;

extern PFN_vkGetInstanceProcAddr pfn_vkGetInstanceProcAddr;
extern PFN_vkCreateInstance pfn_vkCreateInstance;
extern PFN_vkDestroyInstance pfn_vkDestroyInstance;
extern PFN_vkEnumeratePhysicalDevices pfn_vkEnumeratePhysicalDevices;
extern PFN_vkGetPhysicalDeviceProperties pfn_vkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties pfn_vkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR pfn_vkGetPhysicalDeviceSurfaceSupportKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pfn_vkGetPhysicalDeviceSurfaceFormatsKHR;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pfn_vkGetPhysicalDeviceSurfacePresentModesKHR;
extern PFN_vkDestroySurfaceKHR pfn_vkDestroySurfaceKHR;
extern PFN_vkEnumerateDeviceExtensionProperties pfn_vkEnumerateDeviceExtensionProperties;
/** Optional (Vulkan 1.1+); used for ray tracing and feature queries. */
extern PFN_vkGetPhysicalDeviceFeatures2 pfn_vkGetPhysicalDeviceFeatures2;
extern PFN_vkGetPhysicalDeviceProperties2 pfn_vkGetPhysicalDeviceProperties2;
extern PFN_vkGetPhysicalDeviceMemoryProperties pfn_vkGetPhysicalDeviceMemoryProperties;

extern PFN_vkCreateDevice pfn_vkCreateDevice;
extern PFN_vkDestroyDevice pfn_vkDestroyDevice;
extern PFN_vkGetDeviceQueue pfn_vkGetDeviceQueue;
extern PFN_vkCreateSwapchainKHR pfn_vkCreateSwapchainKHR;
extern PFN_vkDestroySwapchainKHR pfn_vkDestroySwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR pfn_vkGetSwapchainImagesKHR;
extern PFN_vkCreateImageView pfn_vkCreateImageView;
extern PFN_vkDestroyImageView pfn_vkDestroyImageView;
extern PFN_vkCreateRenderPass pfn_vkCreateRenderPass;
extern PFN_vkDestroyRenderPass pfn_vkDestroyRenderPass;
extern PFN_vkCreateFramebuffer pfn_vkCreateFramebuffer;
extern PFN_vkDestroyFramebuffer pfn_vkDestroyFramebuffer;
extern PFN_vkCreateCommandPool pfn_vkCreateCommandPool;
extern PFN_vkDestroyCommandPool pfn_vkDestroyCommandPool;
extern PFN_vkAllocateCommandBuffers pfn_vkAllocateCommandBuffers;
extern PFN_vkFreeCommandBuffers pfn_vkFreeCommandBuffers;
extern PFN_vkCreateSemaphore pfn_vkCreateSemaphore;
extern PFN_vkDestroySemaphore pfn_vkDestroySemaphore;
extern PFN_vkCreateFence pfn_vkCreateFence;
extern PFN_vkDestroyFence pfn_vkDestroyFence;
extern PFN_vkAcquireNextImageKHR pfn_vkAcquireNextImageKHR;
extern PFN_vkQueueSubmit pfn_vkQueueSubmit;
extern PFN_vkQueuePresentKHR pfn_vkQueuePresentKHR;
extern PFN_vkBeginCommandBuffer pfn_vkBeginCommandBuffer;
extern PFN_vkEndCommandBuffer pfn_vkEndCommandBuffer;
extern PFN_vkCmdBeginRenderPass pfn_vkCmdBeginRenderPass;
extern PFN_vkCmdEndRenderPass pfn_vkCmdEndRenderPass;
extern PFN_vkWaitForFences pfn_vkWaitForFences;
extern PFN_vkResetFences pfn_vkResetFences;
extern PFN_vkDeviceWaitIdle pfn_vkDeviceWaitIdle;

extern PFN_vkGetDeviceProcAddr pfn_vkGetDeviceProcAddr;

/**
 * Create swapchain, views, render pass, framebuffers, command buffers, sync objects.
 * On failure calls VkRHI_Shutdown() and returns qfalse.
 */
qboolean VkRhi_CreateSwapchainResources(struct SDL_Window *window, const VkSurfaceCapabilitiesKHR *caps);

/**
 * When device exposes KHR ray tracing pipeline, build minimal BLAS/TLAS, SBT, and trace into the swapchain.
 * Requires swapchain images created with STORAGE usage. Safe no-op if unsupported or on failure.
 */
void VkRhi_InitRayTracing(void);

void VkRhi_ShutdownRayTracing(void);

/**
 * Re-record per-swapchain-image command buffers (clear pass vs ray trace).
 */
void VkRhi_RefreshSwapchainCommandBuffers(void);

#endif // VK_RHI_INTERNAL_H
