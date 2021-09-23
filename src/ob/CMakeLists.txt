file(GLOB ob_sources CONFIGURE_DEPENDS "*.c")

add_library(ob STATIC ${ob_sources})
