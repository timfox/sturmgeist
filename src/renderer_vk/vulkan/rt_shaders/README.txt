Regenerate embedded SPIR-V (../vk_rt_spirv_*.inc) after editing the GLSL:

  glslangValidator -V --target-env vulkan1.3 -S rgen rgen.rgen -o /tmp/rgen.spv
  glslangValidator -V --target-env vulkan1.3 -S rmiss rmiss.rmiss -o /tmp/rmiss.spv
  glslangValidator -V --target-env vulkan1.3 -S rchit rchit.rchit -o /tmp/rchit.spv

Then convert each .spv to a C uint8_t array (e.g. small Python script used in-tree) into vk_rt_spirv_rgen.inc, etc.
