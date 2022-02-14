file(GLOB core_sources CONFIGURE_DEPENDS "[tpvse]*.c")

add_library(core STATIC ${core_sources})
