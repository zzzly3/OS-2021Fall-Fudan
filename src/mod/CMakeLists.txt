file(GLOB mod_sources CONFIGURE_DEPENDS "*.c")

add_library(mod STATIC ${mod_sources})
