file(GLOB fs_sources CONFIGURE_DEPENDS "*.c")

add_library(fs STATIC ${fs_sources})

target_compile_options(fs PUBLIC "-fno-builtin" "-ffreestanding" "-DUPDATE_API")
