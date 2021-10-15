file(GLOB core_sources CONFIGURE_DEPENDS "[tpcs]*.c")

add_library(core STATIC ${core_sources})
