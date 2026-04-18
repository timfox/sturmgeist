/*
 * Copyright (C) 2026 ET: Legacy / Sturmgeist contributors
 *
 * ET: Legacy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "tr_common.h"
#include "tr_local_proxy.h"

#include "../renderer_vk/vulkan/vk_rhi.h"

/**
 * @brief Vulkan path: skip GLEW / GL context probing (handled by VkRHI + SDL).
 */
int RE_InitOpenGlSubsystems(void)
{
	if (VkRHI_IsActive())
	{
		return qtrue;
	}
	Com_Printf(S_COLOR_RED "Vulkan: RHI is not active; cannot initialize renderer GL subsystem\n");
	return qfalse;
}

/**
 * @brief Vulkan path: no GL extensions string; optional future hook for device caps.
 */
void RE_InitOpenGl(void)
{
	if (VkRHI_IsActive())
	{
		Com_Printf("Vulkan: renderer GL compatibility layer skipped (native RHI)\n");
	}
}
