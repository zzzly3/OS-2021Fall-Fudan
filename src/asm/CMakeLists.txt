file(GLOB asm_sources CONFIGURE_DEPENDS "*.[Sc]")

add_library(asm STATIC ${asm_sources})
