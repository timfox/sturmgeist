/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "vk_rhi_internal.h"
#include "vk_rhi.h"
#include "vk_rhi_formats.h"

#include "../../qcommon/qcommon.h"

#include <SDL2/SDL.h>

#include <stdlib.h>
#include <string.h>

qboolean VkRhi_CreateSwapchainResources(struct SDL_Window *window, const VkSurfaceCapabilitiesKHR *caps)
{
	VkSwapchainCreateInfoKHR swInfo;
	uint32_t                   fmtCount = 0, modeCount = 0, imgCount = 0;
	VkSurfaceFormatKHR        *formats;
	VkPresentModeKHR          *modes;
	VkSurfaceFormatKHR         surfaceFormat;
	VkPresentModeKHR           presentMode;
	VkAttachmentDescription    colorAtt;
	VkAttachmentReference      colorRef;
	VkSubpassDescription       subpass;
	VkRenderPassCreateInfo      rpInfo;
	VkCommandPoolCreateInfo     poolInfo;
	VkSemaphoreCreateInfo       semInfo;
	VkFenceCreateInfo           fenceInfo;
	uint32_t                    i;
	VkResult                    res;

	pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &fmtCount, NULL);
	if (!fmtCount)
	{
		Com_Printf(S_COLOR_RED "Vulkan: no surface formats\n");
		VkRHI_Shutdown();
		return qfalse;
	}
	formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * fmtCount);
	pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &fmtCount, formats);
	surfaceFormat = VkRhi_ChooseSurfaceFormat(formats, fmtCount);
	free(formats);

	pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &modeCount, NULL);
	modes = (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * modeCount);
	pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &modeCount, modes);
	presentMode = VkRhi_ChoosePresentMode(modes, modeCount);
	free(modes);

	if (caps->currentExtent.width != UINT32_MAX)
	{
		vk.swapchainExtent = caps->currentExtent;
	}
	else
	{
		int w = 0, h = 0;
		SDL_GetWindowSize(window, &w, &h);
		vk.swapchainExtent.width  = (uint32_t)w;
		vk.swapchainExtent.height = (uint32_t)h;
	}

	imgCount = caps->minImageCount + 1;
	if (caps->maxImageCount > 0 && imgCount > caps->maxImageCount)
	{
		imgCount = caps->maxImageCount;
	}

	Com_Memset(&swInfo, 0, sizeof(swInfo));
	swInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swInfo.surface          = vk.surface;
	swInfo.minImageCount    = imgCount;
	swInfo.imageFormat      = surfaceFormat.format;
	swInfo.imageColorSpace  = surfaceFormat.colorSpace;
	swInfo.imageExtent      = vk.swapchainExtent;
	swInfo.imageArrayLayers = 1;
	swInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	swInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swInfo.preTransform     = caps->currentTransform;
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
		iv.subresourceRange.levelCount     = 1;
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
		VkCommandBufferBeginInfo bi   = { 0 };
		VkRenderPassBeginInfo   rpBegin = { 0 };
		VkClearValue            clearVal;

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

		rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpBegin.renderPass        = vk.renderPass;
		rpBegin.framebuffer       = vk.framebuffers[i];
		rpBegin.renderArea.offset = (VkOffset2D){ 0, 0 };
		rpBegin.renderArea.extent = vk.swapchainExtent;
		rpBegin.clearValueCount   = 1;
		rpBegin.pClearValues      = &clearVal;

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

	return qtrue;
}
