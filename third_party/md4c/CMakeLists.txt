# md4c - Markdown parser for C
# https://github.com/mity/md4c  v0.5.3  MIT License

enable_language(C)

add_library(md4c STATIC
    md4c.c
    md4c-html.c
    entity.c
)

target_include_directories(md4c PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_definitions(md4c PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(md4c PRIVATE /W3)
endif()
