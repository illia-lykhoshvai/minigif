set(MINIGIF_SOURCES 
    ${CMAKE_CURRENT_LIST_DIR}/src/minigif.c 
    ${CMAKE_CURRENT_LIST_DIR}/src/minigif_lzw.c
)
set(MINIGIF_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/include)

if (ESP_PLATFORM)
    idf_component_register(
        SRCS ${MINIGIF_SOURCES}
        INCLUDE_DIRS ${MINIGIF_INCLUDE}
    )
else()
    add_library(minigif_lib STATIC ${MINIGIF_SOURCES})
    target_include_directories(minigif_lib PUBLIC ${MINIGIF_INCLUDE})
endif()
