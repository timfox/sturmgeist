/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef VK_RHI_H
#define VK_RHI_H

#include "../../qcommon/q_shared.h"

struct SDL_Window;

/**
 * @brief Initialize Vulkan for the given SDL window (must be SDL_WINDOW_VULKAN).
 * @return qtrue on success.
 */
qboolean VkRHI_Init(struct SDL_Window *window);

/**
 * @brief Tear down all Vulkan objects. Safe to call when not initialized.
 */
void VkRHI_Shutdown(void);

/**
 * @brief Present one frame (acquire, clear swapchain image, present).
 */
void VkRHI_SwapFrame(void);

/**
 * @brief Block until the device is idle (no queued work). Safe when not initialized.
 */
void VkRHI_WaitIdle(void);

/**
 * @brief Whether VkRHI_Init completed successfully.
 */
qboolean VkRHI_IsActive(void);

#endif // VK_RHI_H
