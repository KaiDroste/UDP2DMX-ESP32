if("${CMAKE_PROJECT_NAME}" STREQUAL "bootloader")
    return()
endif()

idf_component_register(
    SRCS "my_config.c"
    INCLUDE_DIRS "include"
    REQUIRES driver json spiffs my_wifi
)
message(STATUS "my_config component being included")
