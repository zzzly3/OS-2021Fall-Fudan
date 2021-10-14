file(GLOB core_sources CONFIGURE_DEPENDS "[tpv]*.c")

add_library(core STATIC ${core_sources})
