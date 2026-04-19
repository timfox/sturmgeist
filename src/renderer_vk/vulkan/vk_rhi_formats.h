/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef VK_RHI_FORMATS_H
#define VK_RHI_FORMATS_H

#include <stdint.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

/**
 * Pick a surface format, preferring B8G8R8A8 in sRGB non-linear color space.
 */
VkSurfaceFormatKHR VkRhi_ChooseSurfaceFormat(const VkSurfaceFormatKHR *formats, uint32_t n);

/**
 * Pick a present mode, preferring MAILBOX when available (lower latency than FIFO).
 */
VkPresentModeKHR VkRhi_ChoosePresentMode(const VkPresentModeKHR *modes, uint32_t n);

#endif // VK_RHI_FORMATS_H
