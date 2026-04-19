# Vulkan RHI (`renderer_vk/vulkan`)

Small, dependency-light layer between **SDL2 Vulkan** and the **ET:L Vulkan renderer** target. The rest of `renderer_vk/` is the refactored renderer; this folder is the place to grow **instance / device / swapchain** logic without bloating `vk_rhi.c`.

## Files

| File | Role |
|------|------|
| **`vk_rhi.h` / `vk_rhi.c`** | Public API: init, shutdown, frame present, `VkRHI_IsActive`. Owns global Vulkan state and loader proc table. |
| **`vk_rhi_formats.h` / `vk_rhi_formats.c`** | Pure helpers: surface format and present-mode selection. |

## Build

- **Static renderer:** `vk_rhi.c` is linked into `renderer_vulkan`; `FEATURE_RENDERER_VULKAN` is defined on that target.
- **Dynamic renderer:** `vk_rhi.c` is also compiled into **`etl`** so `sdl_glimp.c` can call `VkRHI_*` before the renderer module loads (`cmake/ETLBuildClient.cmake`).

## Next modularization targets (suggested)

1. **`vk_rhi_loader.c`** — Move `vk_load_global` / `vk_load_instance` / `vk_load_device` and PFN storage behind a thin struct.
2. **`vk_rhi_swapchain.c`** — Swapchain create/destroy + image views + framebuffers + render pass (what dominates `VkRHI_Init` today).
3. **Resize / `VK_ERROR_OUT_OF_DATE_KHR`** — Recreate swapchain in `VkRHI_SwapFrame` (or dedicated `VkRHI_HandleSurfaceChange`).

## Public API additions

Use **`VkRHI_WaitIdle`** before tearing down resources that may still be referenced by the GPU (e.g. future texture uploads from the renderer).
