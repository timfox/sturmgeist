/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Minimal hardware ray tracing: one-triangle BLAS, one-instance TLAS,
 * ray-gen / miss / closest-hit pipeline, output written to the swapchain
 * image (storage) then transitioned to PRESENT. Skips cleanly if extensions
 * or features are unavailable.
 */

#include "vk_rhi_internal.h"
#include "vk_rhi.h"

#include "../../qcommon/qcommon.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "vk_rt_spirv_rgen.inc"
#include "vk_rt_spirv_rmiss.inc"
#include "vk_rt_spirv_rchit.inc"

typedef struct
{
	VkBuffer           buf;
	VkDeviceMemory     mem;
	VkDeviceAddress    addr;
} vkRtBuf_t;

typedef struct
{
	VkAccelerationStructureKHR as;
	VkBuffer                   backing;
	VkDeviceMemory             backingMem;
	VkDeviceSize               backingSize;
} vkRtAs_t;

static PFN_vkCreateBuffer                         pfn_vkCreateBuffer;
static PFN_vkDestroyBuffer                        pfn_vkDestroyBuffer;
static PFN_vkAllocateMemory                       pfn_vkAllocateMemory;
static PFN_vkFreeMemory                           pfn_vkFreeMemory;
static PFN_vkBindBufferMemory                     pfn_vkBindBufferMemory;
static PFN_vkGetBufferMemoryRequirements          pfn_vkGetBufferMemoryRequirements;
static PFN_vkMapMemory                            pfn_vkMapMemory;
static PFN_vkUnmapMemory                          pfn_vkUnmapMemory;
static PFN_vkGetBufferDeviceAddress               pfn_vkGetBufferDeviceAddress;
static PFN_vkCreateAccelerationStructureKHR       pfn_vkCreateAccelerationStructureKHR;
static PFN_vkDestroyAccelerationStructureKHR      pfn_vkDestroyAccelerationStructureKHR;
static PFN_vkGetAccelerationStructureDeviceAddressKHR pfn_vkGetAccelerationStructureDeviceAddressKHR;
static PFN_vkGetAccelerationStructureBuildSizesKHR pfn_vkGetAccelerationStructureBuildSizesKHR;
static PFN_vkCmdBuildAccelerationStructuresKHR    pfn_vkCmdBuildAccelerationStructuresKHR;
static PFN_vkCreateRayTracingPipelinesKHR         pfn_vkCreateRayTracingPipelinesKHR;
static PFN_vkGetRayTracingShaderGroupHandlesKHR   pfn_vkGetRayTracingShaderGroupHandlesKHR;
static PFN_vkCmdTraceRaysKHR                      pfn_vkCmdTraceRaysKHR;
static PFN_vkCreateShaderModule                   pfn_vkCreateShaderModule;
static PFN_vkDestroyShaderModule                  pfn_vkDestroyShaderModule;
static PFN_vkCreatePipelineLayout                 pfn_vkCreatePipelineLayout;
static PFN_vkDestroyPipelineLayout                pfn_vkDestroyPipelineLayout;
static PFN_vkCreateDescriptorSetLayout            pfn_vkCreateDescriptorSetLayout;
static PFN_vkDestroyDescriptorSetLayout           pfn_vkDestroyDescriptorSetLayout;
static PFN_vkCreateDescriptorPool                 pfn_vkCreateDescriptorPool;
static PFN_vkDestroyDescriptorPool                pfn_vkDestroyDescriptorPool;
static PFN_vkAllocateDescriptorSets               pfn_vkAllocateDescriptorSets;
static PFN_vkUpdateDescriptorSets                 pfn_vkUpdateDescriptorSets;
static PFN_vkDestroyPipeline                      pfn_vkDestroyPipeline;
static PFN_vkCmdPipelineBarrier                   pfn_vkCmdPipelineBarrier;
static PFN_vkCmdBindPipeline                      pfn_vkCmdBindPipeline;
static PFN_vkCmdBindDescriptorSets                pfn_vkCmdBindDescriptorSets;
static PFN_vkAllocateCommandBuffers pfn_vkAllocateCommandBuffers_rt;
static PFN_vkFreeCommandBuffers     pfn_vkFreeCommandBuffers_rt;
static PFN_vkCreateFence           pfn_vkCreateFence_rt;
static PFN_vkDestroyFence          pfn_vkDestroyFence_rt;

static VkPhysicalDeviceRayTracingPipelinePropertiesKHR g_rtProps;
static VkPhysicalDeviceAccelerationStructurePropertiesKHR g_asProps;

static vkRtBuf_t g_vertexBuf;
static vkRtBuf_t g_instancesBuf;
static vkRtBuf_t g_scratchBuf;
static vkRtBuf_t g_sbtBuf;
static vkRtAs_t  g_blas;
static vkRtAs_t  g_tlas;

static VkDescriptorSetLayout g_descLayout;
static VkPipelineLayout      g_pipeLayout;
static VkDescriptorPool     g_descPool;
static VkDescriptorSet     *g_descSets;
static uint32_t             g_descSetCount;

static VkPipeline       g_rtPipeline;
static VkShaderModule   g_modRgen, g_modMiss, g_modHit;

static qboolean vk_rt_load_device(VkDevice dev)
{
	pfn_vkCreateBuffer       = (PFN_vkCreateBuffer)pfn_vkGetDeviceProcAddr(dev, "vkCreateBuffer");
	pfn_vkDestroyBuffer      = (PFN_vkDestroyBuffer)pfn_vkGetDeviceProcAddr(dev, "vkDestroyBuffer");
	pfn_vkAllocateMemory     = (PFN_vkAllocateMemory)pfn_vkGetDeviceProcAddr(dev, "vkAllocateMemory");
	pfn_vkFreeMemory         = (PFN_vkFreeMemory)pfn_vkGetDeviceProcAddr(dev, "vkFreeMemory");
	pfn_vkBindBufferMemory   = (PFN_vkBindBufferMemory)pfn_vkGetDeviceProcAddr(dev, "vkBindBufferMemory");
	pfn_vkGetBufferMemoryRequirements =
	    (PFN_vkGetBufferMemoryRequirements)pfn_vkGetDeviceProcAddr(dev, "vkGetBufferMemoryRequirements");
	pfn_vkMapMemory    = (PFN_vkMapMemory)pfn_vkGetDeviceProcAddr(dev, "vkMapMemory");
	pfn_vkUnmapMemory  = (PFN_vkUnmapMemory)pfn_vkGetDeviceProcAddr(dev, "vkUnmapMemory");
	pfn_vkGetBufferDeviceAddress =
	    (PFN_vkGetBufferDeviceAddress)pfn_vkGetDeviceProcAddr(dev, "vkGetBufferDeviceAddress");
	pfn_vkCreateAccelerationStructureKHR =
	    (PFN_vkCreateAccelerationStructureKHR)pfn_vkGetDeviceProcAddr(dev, "vkCreateAccelerationStructureKHR");
	pfn_vkDestroyAccelerationStructureKHR =
	    (PFN_vkDestroyAccelerationStructureKHR)pfn_vkGetDeviceProcAddr(dev, "vkDestroyAccelerationStructureKHR");
	pfn_vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)pfn_vkGetDeviceProcAddr(
	    dev, "vkGetAccelerationStructureDeviceAddressKHR");
	pfn_vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)pfn_vkGetDeviceProcAddr(
	    dev, "vkGetAccelerationStructureBuildSizesKHR");
	pfn_vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)pfn_vkGetDeviceProcAddr(
	    dev, "vkCmdBuildAccelerationStructuresKHR");
	pfn_vkCreateRayTracingPipelinesKHR =
	    (PFN_vkCreateRayTracingPipelinesKHR)pfn_vkGetDeviceProcAddr(dev, "vkCreateRayTracingPipelinesKHR");
	pfn_vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)pfn_vkGetDeviceProcAddr(
	    dev, "vkGetRayTracingShaderGroupHandlesKHR");
	pfn_vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)pfn_vkGetDeviceProcAddr(dev, "vkCmdTraceRaysKHR");
	pfn_vkCreateShaderModule  = (PFN_vkCreateShaderModule)pfn_vkGetDeviceProcAddr(dev, "vkCreateShaderModule");
	pfn_vkDestroyShaderModule = (PFN_vkDestroyShaderModule)pfn_vkGetDeviceProcAddr(dev, "vkDestroyShaderModule");
	pfn_vkCreatePipelineLayout  = (PFN_vkCreatePipelineLayout)pfn_vkGetDeviceProcAddr(dev, "vkCreatePipelineLayout");
	pfn_vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)pfn_vkGetDeviceProcAddr(dev, "vkDestroyPipelineLayout");
	pfn_vkCreateDescriptorSetLayout =
	    (PFN_vkCreateDescriptorSetLayout)pfn_vkGetDeviceProcAddr(dev, "vkCreateDescriptorSetLayout");
	pfn_vkDestroyDescriptorSetLayout =
	    (PFN_vkDestroyDescriptorSetLayout)pfn_vkGetDeviceProcAddr(dev, "vkDestroyDescriptorSetLayout");
	pfn_vkCreateDescriptorPool  = (PFN_vkCreateDescriptorPool)pfn_vkGetDeviceProcAddr(dev, "vkCreateDescriptorPool");
	pfn_vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)pfn_vkGetDeviceProcAddr(dev, "vkDestroyDescriptorPool");
	pfn_vkAllocateDescriptorSets =
	    (PFN_vkAllocateDescriptorSets)pfn_vkGetDeviceProcAddr(dev, "vkAllocateDescriptorSets");
	pfn_vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)pfn_vkGetDeviceProcAddr(dev, "vkUpdateDescriptorSets");
	pfn_vkDestroyPipeline      = (PFN_vkDestroyPipeline)pfn_vkGetDeviceProcAddr(dev, "vkDestroyPipeline");
	pfn_vkCmdPipelineBarrier   = (PFN_vkCmdPipelineBarrier)pfn_vkGetDeviceProcAddr(dev, "vkCmdPipelineBarrier");
	pfn_vkCmdBindPipeline      = (PFN_vkCmdBindPipeline)pfn_vkGetDeviceProcAddr(dev, "vkCmdBindPipeline");
	pfn_vkCmdBindDescriptorSets =
	    (PFN_vkCmdBindDescriptorSets)pfn_vkGetDeviceProcAddr(dev, "vkCmdBindDescriptorSets");
	pfn_vkAllocateCommandBuffers_rt =
	    (PFN_vkAllocateCommandBuffers)pfn_vkGetDeviceProcAddr(dev, "vkAllocateCommandBuffers");
	pfn_vkFreeCommandBuffers_rt = (PFN_vkFreeCommandBuffers)pfn_vkGetDeviceProcAddr(dev, "vkFreeCommandBuffers");
	pfn_vkCreateFence_rt  = (PFN_vkCreateFence)pfn_vkGetDeviceProcAddr(dev, "vkCreateFence");
	pfn_vkDestroyFence_rt = (PFN_vkDestroyFence)pfn_vkGetDeviceProcAddr(dev, "vkDestroyFence");

	return pfn_vkCreateBuffer && pfn_vkCreateAccelerationStructureKHR && pfn_vkCreateRayTracingPipelinesKHR &&
	       pfn_vkCmdTraceRaysKHR && pfn_vkAllocateCommandBuffers_rt;
}

static uint32_t vk_rt_memory_type(uint32_t typeBits, VkMemoryPropertyFlags want)
{
	VkPhysicalDeviceMemoryProperties mp;
	uint32_t i;
	pfn_vkGetPhysicalDeviceMemoryProperties(vk.physicalDevice, &mp);
	for (i = 0; i < mp.memoryTypeCount; i++)
	{
		if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want)
		{
			return i;
		}
	}
	return UINT32_MAX;
}

static void vk_rt_buf_free(vkRtBuf_t *b)
{
	if (!vk.device)
	{
		return;
	}
	if (b->buf)
	{
		pfn_vkDestroyBuffer(vk.device, b->buf, NULL);
		b->buf = VK_NULL_HANDLE;
	}
	if (b->mem)
	{
		pfn_vkFreeMemory(vk.device, b->mem, NULL);
		b->mem = VK_NULL_HANDLE;
	}
	b->addr = 0;
}

static qboolean vk_rt_buf_create(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memProps,
                                 VkMemoryAllocateFlags allocFlags, vkRtBuf_t *out)
{
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateFlagsInfo fl = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
	VkMemoryAllocateInfo ai;
	VkBufferDeviceAddressInfo bai;
	VkResult res;
	uint32_t idx;

	Com_Memset(&bi, 0, sizeof(bi));
	bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size        = size;
	bi.usage       = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	res = pfn_vkCreateBuffer(vk.device, &bi, NULL, &out->buf);
	if (res != VK_SUCCESS)
	{
		return qfalse;
	}
	pfn_vkGetBufferMemoryRequirements(vk.device, out->buf, &req);
	idx = vk_rt_memory_type(req.memoryTypeBits, memProps);
	if (idx == UINT32_MAX)
	{
		vk_rt_buf_free(out);
		return qfalse;
	}
	Com_Memset(&ai, 0, sizeof(ai));
	ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.pNext           = (allocFlags != 0) ? &fl : NULL;
	fl.flags           = allocFlags;
	ai.allocationSize  = req.size;
	ai.memoryTypeIndex = idx;
	res = pfn_vkAllocateMemory(vk.device, &ai, NULL, &out->mem);
	if (res != VK_SUCCESS)
	{
		vk_rt_buf_free(out);
		return qfalse;
	}
	if (pfn_vkBindBufferMemory(vk.device, out->buf, out->mem, 0) != VK_SUCCESS)
	{
		vk_rt_buf_free(out);
		return qfalse;
	}
	out->addr = 0;
	if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		Com_Memset(&bai, 0, sizeof(bai));
		bai.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bai.buffer = out->buf;
		out->addr  = pfn_vkGetBufferDeviceAddress(vk.device, &bai);
	}
	return qtrue;
}

static void vk_rt_as_free(vkRtAs_t *a)
{
	if (!vk.device)
	{
		return;
	}
	if (a->as)
	{
		pfn_vkDestroyAccelerationStructureKHR(vk.device, a->as, NULL);
		a->as = VK_NULL_HANDLE;
	}
	if (a->backing)
	{
		pfn_vkDestroyBuffer(vk.device, a->backing, NULL);
		a->backing = VK_NULL_HANDLE;
	}
	if (a->backingMem)
	{
		pfn_vkFreeMemory(vk.device, a->backingMem, NULL);
		a->backingMem = VK_NULL_HANDLE;
	}
	a->backingSize = 0;
}

static qboolean vk_rt_as_create(VkAccelerationStructureTypeKHR type, VkDeviceSize asSize, vkRtAs_t *out)
{
	VkAccelerationStructureCreateInfoKHR ci;
	VkBufferCreateInfo bi;
	VkMemoryRequirements req;
	VkMemoryAllocateFlagsInfo fl = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
	fl.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	VkMemoryAllocateInfo ai;
	VkResult res;
	uint32_t idx;

	Com_Memset(&bi, 0, sizeof(bi));
	bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size        = asSize;
	bi.usage       = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	res = pfn_vkCreateBuffer(vk.device, &bi, NULL, &out->backing);
	if (res != VK_SUCCESS)
	{
		return qfalse;
	}
	pfn_vkGetBufferMemoryRequirements(vk.device, out->backing, &req);
	idx = vk_rt_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (idx == UINT32_MAX)
	{
		vk_rt_as_free(out);
		return qfalse;
	}
	Com_Memset(&ai, 0, sizeof(ai));
	ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.pNext           = &fl;
	ai.allocationSize  = req.size;
	ai.memoryTypeIndex = idx;
	res = pfn_vkAllocateMemory(vk.device, &ai, NULL, &out->backingMem);
	if (res != VK_SUCCESS || pfn_vkBindBufferMemory(vk.device, out->backing, out->backingMem, 0) != VK_SUCCESS)
	{
		vk_rt_as_free(out);
		return qfalse;
	}
	out->backingSize = asSize;

	Com_Memset(&ci, 0, sizeof(ci));
	ci.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	ci.buffer = out->backing;
	ci.size   = asSize;
	ci.type   = type;
	res = pfn_vkCreateAccelerationStructureKHR(vk.device, &ci, NULL, &out->as);
	if (res != VK_SUCCESS)
	{
		vk_rt_as_free(out);
		return qfalse;
	}
	return qtrue;
}

static VkDeviceAddress vk_rt_as_device_addr(VkAccelerationStructureKHR as)
{
	VkAccelerationStructureDeviceAddressInfoKHR inf;
	Com_Memset(&inf, 0, sizeof(inf));
	inf.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	inf.accelerationStructure = as;
	return pfn_vkGetAccelerationStructureDeviceAddressKHR(vk.device, &inf);
}

static qboolean vk_rt_extensions_ok(void)
{
	uint32_t n, i;
	VkExtensionProperties *p;
	qboolean accel = qfalse, rt = qfalse, def = qfalse, bda = qfalse;

	pfn_vkEnumerateDeviceExtensionProperties(vk.physicalDevice, NULL, &n, NULL);
	if (!n)
	{
		return qfalse;
	}
	p = (VkExtensionProperties *)malloc(sizeof(*p) * n);
	pfn_vkEnumerateDeviceExtensionProperties(vk.physicalDevice, NULL, &n, p);
	for (i = 0; i < n; i++)
	{
		if (!strcmp(p[i].extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
		{
			accel = qtrue;
		}
		if (!strcmp(p[i].extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
		{
			rt = qtrue;
		}
		if (!strcmp(p[i].extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME))
		{
			def = qtrue;
		}
		if (!strcmp(p[i].extensionName, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
		{
			bda = qtrue;
		}
	}
	free(p);
	return accel && rt && def && bda;
}

static qboolean vk_rt_submit_build(VkAccelerationStructureBuildGeometryInfoKHR *buildInfo,
                                   const VkAccelerationStructureBuildRangeInfoKHR *range)
{
	const VkAccelerationStructureBuildRangeInfoKHR *pRange = range;
	VkCommandBufferAllocateInfo alloc;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFenceCreateInfo fi;
	VkFence fence = VK_NULL_HANDLE;
	VkSubmitInfo si;
	VkResult res;

	Com_Memset(&alloc, 0, sizeof(alloc));
	alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc.commandPool        = vk.commandPool;
	alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc.commandBufferCount = 1;
	res = pfn_vkAllocateCommandBuffers_rt(vk.device, &alloc, &cmd);
	if (res != VK_SUCCESS)
	{
		return qfalse;
	}
	{
		VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		pfn_vkBeginCommandBuffer(cmd, &bi);
		pfn_vkCmdBuildAccelerationStructuresKHR(cmd, 1, buildInfo, &pRange);
		pfn_vkEndCommandBuffer(cmd);
	}
	Com_Memset(&fi, 0, sizeof(fi));
	fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (pfn_vkCreateFence_rt(vk.device, &fi, NULL, &fence) != VK_SUCCESS)
	{
		pfn_vkFreeCommandBuffers_rt(vk.device, vk.commandPool, 1, &cmd);
		return qfalse;
	}
	Com_Memset(&si, 0, sizeof(si));
	si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers    = &cmd;
	res = pfn_vkQueueSubmit(vk.graphicsQueue, 1, &si, fence);
	if (res != VK_SUCCESS)
	{
		pfn_vkDestroyFence_rt(vk.device, fence, NULL);
		pfn_vkFreeCommandBuffers_rt(vk.device, vk.commandPool, 1, &cmd);
		return qfalse;
	}
	pfn_vkWaitForFences(vk.device, 1, &fence, VK_TRUE, UINT64_MAX);
	pfn_vkDestroyFence_rt(vk.device, fence, NULL);
	pfn_vkFreeCommandBuffers_rt(vk.device, vk.commandPool, 1, &cmd);
	return qtrue;
}

void VkRhi_ShutdownRayTracing(void)
{
	vk.rayTracingActive = qfalse;

	if (!vk.device)
	{
		return;
	}

	if (g_rtPipeline)
	{
		pfn_vkDestroyPipeline(vk.device, g_rtPipeline, NULL);
		g_rtPipeline = VK_NULL_HANDLE;
	}
	if (g_modRgen)
	{
		pfn_vkDestroyShaderModule(vk.device, g_modRgen, NULL);
		g_modRgen = VK_NULL_HANDLE;
	}
	if (g_modMiss)
	{
		pfn_vkDestroyShaderModule(vk.device, g_modMiss, NULL);
		g_modMiss = VK_NULL_HANDLE;
	}
	if (g_modHit)
	{
		pfn_vkDestroyShaderModule(vk.device, g_modHit, NULL);
		g_modHit = VK_NULL_HANDLE;
	}
	if (g_pipeLayout)
	{
		pfn_vkDestroyPipelineLayout(vk.device, g_pipeLayout, NULL);
		g_pipeLayout = VK_NULL_HANDLE;
	}
	if (g_descLayout)
	{
		pfn_vkDestroyDescriptorSetLayout(vk.device, g_descLayout, NULL);
		g_descLayout = VK_NULL_HANDLE;
	}
	if (g_descPool)
	{
		pfn_vkDestroyDescriptorPool(vk.device, g_descPool, NULL);
		g_descPool = VK_NULL_HANDLE;
	}
	if (g_descSets)
	{
		free(g_descSets);
		g_descSets = NULL;
	}
	g_descSetCount = 0;

	vk_rt_as_free(&g_blas);
	vk_rt_as_free(&g_tlas);
	vk_rt_buf_free(&g_vertexBuf);
	vk_rt_buf_free(&g_instancesBuf);
	vk_rt_buf_free(&g_scratchBuf);
	vk_rt_buf_free(&g_sbtBuf);
}

void VkRhi_RefreshSwapchainCommandBuffers(void)
{
	uint32_t i;
	VkResult res;

	if (!vk.device || !vk.commandBuffers || !vk.swapchainImageCount)
	{
		return;
	}

	for (i = 0; i < vk.swapchainImageCount; i++)
	{
		VkCommandBufferBeginInfo bi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		bi.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		res = pfn_vkBeginCommandBuffer(vk.commandBuffers[i], &bi);
		if (res != VK_SUCCESS)
		{
			continue;
		}

		if (vk.rayTracingActive && g_rtPipeline && g_descSets && g_rtProps.shaderGroupHandleSize)
		{
			VkImageMemoryBarrier imb;
			VkStridedDeviceAddressRegionKHR raygenRegion, missRegion, hitRegion, callRegion;
			VkDeviceSize handleSize, handleAlign, baseAlign, stride;

			handleSize  = g_rtProps.shaderGroupHandleSize;
			handleAlign = g_rtProps.shaderGroupHandleAlignment;
			baseAlign   = g_rtProps.shaderGroupBaseAlignment;
			stride      = handleSize;
			while (stride % handleAlign)
			{
				stride++;
			}
			while (stride % baseAlign)
			{
				stride++;
			}

			Com_Memset(&imb, 0, sizeof(imb));
			imb.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imb.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
			imb.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
			imb.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
			imb.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
			imb.image                           = vk.swapchainImages[i];
			imb.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
			imb.subresourceRange.baseMipLevel   = 0;
			imb.subresourceRange.levelCount     = 1;
			imb.subresourceRange.baseArrayLayer = 0;
			imb.subresourceRange.layerCount     = 1;
			imb.srcAccessMask                   = 0;
			imb.dstAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;
			pfn_vkCmdPipelineBarrier(vk.commandBuffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, NULL, 0, NULL, 1, &imb);

			pfn_vkCmdBindPipeline(vk.commandBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, g_rtPipeline);
			pfn_vkCmdBindDescriptorSets(vk.commandBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, g_pipeLayout, 0,
			                            1, &g_descSets[i], 0, NULL);

			raygenRegion.deviceAddress = g_sbtBuf.addr;
			raygenRegion.stride        = stride;
			raygenRegion.size          = stride;
			missRegion.deviceAddress   = g_sbtBuf.addr + stride;
			missRegion.stride          = stride;
			missRegion.size            = stride;
			hitRegion.deviceAddress    = g_sbtBuf.addr + 2u * stride;
			hitRegion.stride           = stride;
			hitRegion.size             = stride;
			Com_Memset(&callRegion, 0, sizeof(callRegion));

			pfn_vkCmdTraceRaysKHR(vk.commandBuffers[i], &raygenRegion, &missRegion, &hitRegion, &callRegion,
			                      vk.swapchainExtent.width, vk.swapchainExtent.height, 1u);

			imb.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
			imb.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			imb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imb.dstAccessMask = 0;
			pfn_vkCmdPipelineBarrier(vk.commandBuffers[i], VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &imb);
		}
		else
		{
			VkRenderPassBeginInfo rpBegin = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			VkClearValue clearVal;
			clearVal.color.float32[0] = 0.02f;
			clearVal.color.float32[1] = 0.05f;
			clearVal.color.float32[2] = 0.12f;
			clearVal.color.float32[3] = 1.f;
			rpBegin.renderPass        = vk.renderPass;
			rpBegin.framebuffer       = vk.framebuffers[i];
			rpBegin.renderArea.offset = (VkOffset2D){ 0, 0 };
			rpBegin.renderArea.extent = vk.swapchainExtent;
			rpBegin.clearValueCount   = 1;
			rpBegin.pClearValues      = &clearVal;
			pfn_vkCmdBeginRenderPass(vk.commandBuffers[i], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
			pfn_vkCmdEndRenderPass(vk.commandBuffers[i]);
		}
		pfn_vkEndCommandBuffer(vk.commandBuffers[i]);
	}
}

void VkRhi_InitRayTracing(void)
{
	float verts[9] = { -1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, -1.f, 0.f };
	VkAccelerationStructureGeometryTrianglesDataKHR triData;
	VkAccelerationStructureGeometryKHR geom;
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo;
	VkAccelerationStructureBuildRangeInfoKHR range;
	VkAccelerationStructureBuildSizesInfoKHR sizes;
	VkAccelerationStructureInstanceKHR inst;
	VkDeviceAddress blasAddr;
	VkMemoryAllocateFlags allocDevAddr = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	VkShaderModuleCreateInfo smi[3];
	VkPipelineShaderStageCreateInfo stages[3];
	VkRayTracingShaderGroupCreateInfoKHR groups[3];
	VkRayTracingPipelineCreateInfoKHR pci;
	VkDescriptorSetLayoutBinding binds[2];
	VkDescriptorSetLayoutCreateInfo dsl;
	VkPipelineLayoutCreateInfo plci;
	VkDescriptorPoolSize ps[2];
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorSetAllocateInfo dsai;
	VkWriteDescriptorSet wds[2];
	VkDescriptorImageInfo dii;
	VkWriteDescriptorSetAccelerationStructureKHR wdas;
	VkDeviceSize handleSize, stride, sbtBytes;
	uint8_t *handles = NULL;
	void *mapped;
	uint32_t i;
	VkResult res;

	VkRhi_ShutdownRayTracing();

	if (!vk.instance || !vk.device || !vk.physicalDevice || !vk.swapchainImageCount)
	{
		return;
	}

	if (!vk.rayTracingExt)
	{
		return;
	}

	if (!vk_rt_extensions_ok())
	{
		Com_Printf("Vulkan: hardware ray tracing extensions incomplete; using clear-only path\n");
		return;
	}

	if (!pfn_vkGetPhysicalDeviceProperties2 || !pfn_vkGetPhysicalDeviceMemoryProperties || !pfn_vkGetPhysicalDeviceFeatures2 ||
	    !vk_rt_load_device(vk.device))
	{
		Com_Printf(S_COLOR_YELLOW "Vulkan: ray tracing entry points missing\n");
		return;
	}

	Com_Memset(&g_rtProps, 0, sizeof(g_rtProps));
	g_rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	Com_Memset(&g_asProps, 0, sizeof(g_asProps));
	g_asProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
	g_rtProps.pNext = &g_asProps;
	{
		VkPhysicalDeviceProperties2 p2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		p2.pNext = &g_rtProps;
		pfn_vkGetPhysicalDeviceProperties2(vk.physicalDevice, &p2);
	}

	{
		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bda = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR };
		bda.bufferDeviceAddress = VK_TRUE;
		VkPhysicalDeviceAccelerationStructureFeaturesKHR asf = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		asf.accelerationStructure = VK_TRUE;
		asf.pNext                 = &bda;
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtf = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
		rtf.rayTracingPipeline = VK_TRUE;
		rtf.pNext              = &asf;
		VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		f2.pNext = &rtf;
		pfn_vkGetPhysicalDeviceFeatures2(vk.physicalDevice, &f2);
		if (!rtf.rayTracingPipeline || !asf.accelerationStructure || !bda.bufferDeviceAddress)
		{
			Com_Printf(S_COLOR_YELLOW "Vulkan: ray tracing features not enabled on GPU\n");
			return;
		}
	}

	/* Vertex buffer */
	if (!vk_rt_buf_create(sizeof(verts),
	                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
	                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocDevAddr, &g_vertexBuf))
	{
		goto fail;
	}
	if (pfn_vkMapMemory(vk.device, g_vertexBuf.mem, 0, sizeof(verts), 0, &mapped) != VK_SUCCESS)
	{
		goto fail;
	}
	memcpy(mapped, verts, sizeof(verts));
	pfn_vkUnmapMemory(vk.device, g_vertexBuf.mem);

	/* BLAS geometry query */
	Com_Memset(&triData, 0, sizeof(triData));
	triData.sType                      = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	triData.vertexFormat               = VK_FORMAT_R32G32B32_SFLOAT;
	triData.vertexData.deviceAddress = g_vertexBuf.addr;
	triData.vertexStride               = sizeof(float) * 3u;
	triData.maxVertex                  = 2u;
	triData.indexType                  = VK_INDEX_TYPE_NONE_KHR;

	Com_Memset(&geom, 0, sizeof(geom));
	geom.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geom.geometry.triangles = triData;

	Com_Memset(&range, 0, sizeof(range));
	range.primitiveCount = 1u;

	Com_Memset(&buildInfo, 0, sizeof(buildInfo));
	buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1u;
	buildInfo.pGeometries   = &geom;

	Com_Memset(&sizes, 0, sizeof(sizes));
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	{
		uint32_t maxPrim = range.primitiveCount;
		pfn_vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
		                                            &maxPrim, &sizes);
	}

	if (!vk_rt_buf_create(sizes.buildScratchSize,
	                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocDevAddr,
	                     &g_scratchBuf))
	{
		goto fail;
	}

	if (!vk_rt_as_create(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, sizes.accelerationStructureSize, &g_blas))
	{
		goto fail;
	}

	Com_Memset(&buildInfo, 0, sizeof(buildInfo));
	buildInfo.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags                    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure = g_blas.as;
	buildInfo.geometryCount            = 1u;
	buildInfo.pGeometries              = &geom;
	buildInfo.scratchData.deviceAddress = g_scratchBuf.addr;
	if (!vk_rt_submit_build(&buildInfo, &range))
	{
		goto fail;
	}

	blasAddr = vk_rt_as_device_addr(g_blas.as);

	/* Instance buffer for TLAS */
	Com_Memset(&inst, 0, sizeof(inst));
	{
		float *m = &inst.transform.matrix[0][0];
		m[0] = 1.f;
		m[5] = 1.f;
		m[10] = 1.f;
	}
	inst.mask                                  = 0xFF;
	inst.flags                                 = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	inst.accelerationStructureReference        = blasAddr;

	if (!vk_rt_buf_create(sizeof(inst),
	                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
	                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocDevAddr, &g_instancesBuf))
	{
		goto fail;
	}
	if (pfn_vkMapMemory(vk.device, g_instancesBuf.mem, 0, sizeof(inst), 0, &mapped) != VK_SUCCESS)
	{
		goto fail;
	}
	memcpy(mapped, &inst, sizeof(inst));
	pfn_vkUnmapMemory(vk.device, g_instancesBuf.mem);

	Com_Memset(&geom, 0, sizeof(geom));
	geom.sType                        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geom.geometryType                 = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geom.geometry.instances.sType     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geom.geometry.instances.arrayOfPointers    = VK_FALSE;
	geom.geometry.instances.data.deviceAddress = g_instancesBuf.addr;

	Com_Memset(&buildInfo, 0, sizeof(buildInfo));
	buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1u;
	buildInfo.pGeometries   = &geom;

	Com_Memset(&sizes, 0, sizeof(sizes));
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	{
		uint32_t maxPrim = range.primitiveCount;
		pfn_vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
		                                            &maxPrim, &sizes);
	}

	vk_rt_buf_free(&g_scratchBuf);
	if (!vk_rt_buf_create(sizes.buildScratchSize,
	                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocDevAddr,
	                     &g_scratchBuf))
	{
		goto fail;
	}

	if (!vk_rt_as_create(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, sizes.accelerationStructureSize, &g_tlas))
	{
		goto fail;
	}

	Com_Memset(&buildInfo, 0, sizeof(buildInfo));
	buildInfo.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.flags                     = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure  = g_tlas.as;
	buildInfo.geometryCount             = 1u;
	buildInfo.pGeometries               = &geom;
	buildInfo.scratchData.deviceAddress = g_scratchBuf.addr;
	if (!vk_rt_submit_build(&buildInfo, &range))
	{
		goto fail;
	}

	/* Shaders */
	Com_Memset(smi, 0, sizeof(smi));
	smi[0].sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smi[0].codeSize = vk_rt_rgen_spv_size;
	smi[0].pCode    = (const uint32_t *)vk_rt_rgen_spv;
	smi[1].sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smi[1].codeSize = vk_rt_rmiss_spv_size;
	smi[1].pCode    = (const uint32_t *)vk_rt_rmiss_spv;
	smi[2].sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smi[2].codeSize = vk_rt_rchit_spv_size;
	smi[2].pCode    = (const uint32_t *)vk_rt_rchit_spv;

	if (pfn_vkCreateShaderModule(vk.device, &smi[0], NULL, &g_modRgen) != VK_SUCCESS ||
	    pfn_vkCreateShaderModule(vk.device, &smi[1], NULL, &g_modMiss) != VK_SUCCESS ||
	    pfn_vkCreateShaderModule(vk.device, &smi[2], NULL, &g_modHit) != VK_SUCCESS)
	{
		goto fail;
	}

	Com_Memset(stages, 0, sizeof(stages));
	stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[0].module = g_modRgen;
	stages[0].pName  = "main";
	stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage  = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[1].module = g_modMiss;
	stages[1].pName  = "main";
	stages[2].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[2].stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[2].module = g_modHit;
	stages[2].pName  = "main";

	Com_Memset(groups, 0, sizeof(groups));
	groups[0].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[0].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[0].generalShader      = 0;
	groups[0].closestHitShader   = VK_SHADER_UNUSED_KHR;
	groups[0].anyHitShader       = VK_SHADER_UNUSED_KHR;
	groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

	groups[1].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[1].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[1].generalShader      = 1;
	groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;

	groups[2].sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[2].type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groups[2].generalShader      = VK_SHADER_UNUSED_KHR;
	groups[2].closestHitShader   = 2;
	groups[2].anyHitShader       = VK_SHADER_UNUSED_KHR;
	groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

	Com_Memset(binds, 0, sizeof(binds));
	binds[0].binding         = 0;
	binds[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	binds[0].descriptorCount = 1;
	binds[0].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	binds[1].binding         = 1;
	binds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binds[1].descriptorCount = 1;
	binds[1].stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	Com_Memset(&dsl, 0, sizeof(dsl));
	dsl.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dsl.bindingCount = 2;
	dsl.pBindings    = binds;
	if (pfn_vkCreateDescriptorSetLayout(vk.device, &dsl, NULL, &g_descLayout) != VK_SUCCESS)
	{
		goto fail;
	}

	Com_Memset(&plci, 0, sizeof(plci));
	plci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts    = &g_descLayout;
	if (pfn_vkCreatePipelineLayout(vk.device, &plci, NULL, &g_pipeLayout) != VK_SUCCESS)
	{
		goto fail;
	}

	Com_Memset(&pci, 0, sizeof(pci));
	pci.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	pci.stageCount                   = 3;
	pci.pStages                      = stages;
	pci.groupCount                   = 3;
	pci.pGroups                      = groups;
	pci.maxPipelineRayRecursionDepth = 1;
	pci.layout                       = g_pipeLayout;
	res = pfn_vkCreateRayTracingPipelinesKHR(vk.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pci, NULL, &g_rtPipeline);
	if (res != VK_SUCCESS)
	{
		Com_Printf(S_COLOR_YELLOW "Vulkan: vkCreateRayTracingPipelinesKHR failed (%d)\n", (int)res);
		goto fail;
	}

	handleSize = g_rtProps.shaderGroupHandleSize;
	stride     = handleSize;
	while (stride % g_rtProps.shaderGroupHandleAlignment)
	{
		stride++;
	}
	while (stride % g_rtProps.shaderGroupBaseAlignment)
	{
		stride++;
	}
	sbtBytes = stride * 3u;

	if (!vk_rt_buf_create(sbtBytes,
	                     VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
	                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocDevAddr, &g_sbtBuf))
	{
		goto fail;
	}
	handles = (uint8_t *)malloc(handleSize * 3u);
	if (!handles || pfn_vkGetRayTracingShaderGroupHandlesKHR(vk.device, g_rtPipeline, 0, 3, handleSize * 3u, handles) != VK_SUCCESS)
	{
		free(handles);
		goto fail;
	}
	if (pfn_vkMapMemory(vk.device, g_sbtBuf.mem, 0, sbtBytes, 0, &mapped) != VK_SUCCESS)
	{
		free(handles);
		goto fail;
	}
	for (i = 0; i < 3u; i++)
	{
		memcpy((uint8_t *)mapped + i * stride, handles + i * handleSize, handleSize);
	}
	pfn_vkUnmapMemory(vk.device, g_sbtBuf.mem);
	free(handles);
	handles = NULL;

	/* Descriptor pool & sets */
	g_descSetCount = vk.swapchainImageCount;
	Com_Memset(ps, 0, sizeof(ps));
	ps[0].type            = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	ps[0].descriptorCount = g_descSetCount;
	ps[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	ps[1].descriptorCount = g_descSetCount;
	Com_Memset(&dpci, 0, sizeof(dpci));
	dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets       = g_descSetCount;
	dpci.poolSizeCount = 2;
	dpci.pPoolSizes    = ps;
	if (pfn_vkCreateDescriptorPool(vk.device, &dpci, NULL, &g_descPool) != VK_SUCCESS)
	{
		goto fail;
	}

	g_descSets = (VkDescriptorSet *)calloc(g_descSetCount, sizeof(VkDescriptorSet));
	Com_Memset(&dsai, 0, sizeof(dsai));
	dsai.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsai.descriptorPool = g_descPool;
	dsai.descriptorSetCount = g_descSetCount;
	dsai.pSetLayouts        = (VkDescriptorSetLayout *)malloc(sizeof(VkDescriptorSetLayout) * g_descSetCount);
	for (i = 0; i < g_descSetCount; i++)
	{
		((VkDescriptorSetLayout *)dsai.pSetLayouts)[i] = g_descLayout;
	}
	res = pfn_vkAllocateDescriptorSets(vk.device, &dsai, g_descSets);
	free((void *)dsai.pSetLayouts);
	dsai.pSetLayouts = NULL;
	if (res != VK_SUCCESS)
	{
		goto fail;
	}

	for (i = 0; i < g_descSetCount; i++)
	{
		VkAccelerationStructureKHR tlas = g_tlas.as;
		Com_Memset(&wdas, 0, sizeof(wdas));
		wdas.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		wdas.accelerationStructureCount = 1;
		wdas.pAccelerationStructures    = &tlas;

		Com_Memset(wds, 0, sizeof(wds));
		wds[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds[0].pNext           = &wdas;
		wds[0].dstSet          = g_descSets[i];
		wds[0].dstBinding      = 0;
		wds[0].descriptorCount = 1;
		wds[0].descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

		Com_Memset(&dii, 0, sizeof(dii));
		dii.imageView   = vk.swapchainImageViews[i];
		dii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		wds[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds[1].dstSet          = g_descSets[i];
		wds[1].dstBinding      = 1;
		wds[1].descriptorCount = 1;
		wds[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		wds[1].pImageInfo      = &dii;

		pfn_vkUpdateDescriptorSets(vk.device, 2, wds, 0, NULL);
	}

	vk.rayTracingActive = qtrue;
	VkRhi_RefreshSwapchainCommandBuffers();
	Com_Printf("Vulkan: hardware ray tracing path active (demo triangle + sky gradient)\n");
	return;

fail:
	VkRhi_ShutdownRayTracing();
}
