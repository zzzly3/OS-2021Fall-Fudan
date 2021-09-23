file(GLOB obj_sources CONFIGURE_DEPENDS "*.c")

add_library(obj STATIC ${obj_sources})
