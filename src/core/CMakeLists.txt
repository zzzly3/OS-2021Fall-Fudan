# file(GLOB core_sources CONFIGURE_DEPENDS "*.c")

file (GLOB core_sources CONFIGURE_DEPENDS "physical_memory.c virtual_memory.c")
add_library(core STATIC ${core_sources})
