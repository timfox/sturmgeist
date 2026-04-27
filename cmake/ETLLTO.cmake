#-----------------------------------------------------------------
# Optional link-time optimization (IPO / LTO)
#-----------------------------------------------------------------
#
# Off by default: LTO increases link time and needs a capable toolchain.
# When enabled, applies to Release-style configurations only.

if(NOT ENABLE_LTO)
	return()
endif()

if(ENABLE_ASAN)
	message(FATAL_ERROR "ENABLE_LTO cannot be used together with ENABLE_ASAN.")
endif()

if(CMAKE_VERSION VERSION_LESS "3.9")
	message(FATAL_ERROR "ENABLE_LTO requires CMake 3.9 or newer (CheckIPOSupported).")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT _etl_ipo_supported OUTPUT _etl_ipo_error)

if(NOT _etl_ipo_supported)
	message(WARNING "ENABLE_LTO=ON but IPO is not supported by this toolchain: ${_etl_ipo_error}")
	return()
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)

message(STATUS "Link-time optimization (IPO/LTO) enabled for Release, RelWithDebInfo, MinSizeRel")
