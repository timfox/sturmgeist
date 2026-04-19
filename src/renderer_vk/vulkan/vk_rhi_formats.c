/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "vk_rhi_formats.h"

VkSurfaceFormatKHR VkRhi_ChooseSurfaceFormat(const VkSurfaceFormatKHR *formats, uint32_t n)
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

VkPresentModeKHR VkRhi_ChoosePresentMode(const VkPresentModeKHR *modes, uint32_t n)
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
