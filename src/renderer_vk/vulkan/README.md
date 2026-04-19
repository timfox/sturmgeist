# Vulkan RHI (`renderer_vk/vulkan`)

Small, dependency-light layer between **SDL2 Vulkan** and the **ET:L Vulkan renderer** target. The rest of `renderer_vk/` is the refactored renderer; this folder is the place to grow **instance / device / swapchain** logic without bloating `vk_rhi.c`.

## Files

| File | Role |
|------|------|
| **`vk_rhi.h` / `vk_rhi.c`** | Public API: init, shutdown, frame present, `VkRHI_IsActive`. Owns global Vulkan state and loader proc table. |
| **`vk_rhi_formats.h` / `vk_rhi_formats.c`** | Pure helpers: surface format and present-mode selection. |
| **`vk_rhi_swapchain.c`** | Swapchain, views, render pass, framebuffers, per-image command buffers (clear or trace). |
| **`vk_rhi_raytrace.c`** | Optional **KHR ray tracing pipeline** path: BLAS/TLAS, SBT, `vkCmdTraceRaysKHR` into the swapchain as a storage image. SPIR-V is embedded via `vk_rt_spirv_*.inc`. |
| **`rt_shaders/*.rgen` / `*.rmiss` / `*.rchit`** | GLSL source for the demo trace; regenerate includes with `glslangValidator` (see comments in `rt_shaders/`). |

## Build

- **Static renderer:** `vk_rhi.c` is linked into `renderer_vulkan`; `FEATURE_RENDERER_VULKAN` is defined on that target.
- **Dynamic renderer:** `vk_rhi.c` is also compiled into **`etl`** so `sdl_glimp.c` can call `VkRHI_*` before the renderer module loads (`cmake/ETLBuildClient.cmake`).
- **RHI TU list:** `cmake/ETLSources.cmake` variable `VULKAN_RHI_SRC` lists every file linked into both the client (when `RENDERER_DYNAMIC`) and `renderer_vulkan`.

## Hardware ray tracing (demo)

When the GPU exposes the full KHR stack and features, `VkRHI_Init` enables **buffer device address**, **acceleration structures**, and **ray tracing pipeline** on the logical device, builds a one-triangle scene, and replaces the per-frame clear with `vkCmdTraceRaysKHR` into the swapchain (`STORAGE` + `COLOR_ATTACHMENT` usage). If creation fails or the stack is incomplete, the RHI falls back to the previous clear-only path. **World rendering is still the classic renderer**; this is the foundation for future RT effects (reflections, GI, etc.), not a full game path tracer.

## Next modularization targets (suggested)

1. **`vk_rhi_loader.c`** — Move `vk_load_global` / `vk_load_instance` / `vk_load_device` and PFN storage behind a thin struct.
2. **Resize / `VK_ERROR_OUT_OF_DATE_KHR`** — Recreate swapchain in `VkRHI_SwapFrame` (or dedicated `VkRHI_HandleSurfaceChange`).

## Public API additions

Use **`VkRHI_WaitIdle`** before tearing down resources that may still be referenced by the GPU (e.g. future texture uploads from the renderer).
