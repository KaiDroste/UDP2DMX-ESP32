if("${CMAKE_PROJECT_NAME}" STREQUAL "bootloader")
    return()
endif()

idf_component_register(
    SRCS "my_led.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)
message(STATUS "my_led component being included")
