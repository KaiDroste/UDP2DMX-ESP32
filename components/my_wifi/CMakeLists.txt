if("${CMAKE_PROJECT_NAME}" STREQUAL "bootloader")
    return()
endif()

idf_component_register(
    SRCS "my_wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_event esp_wifi driver my_led mdns
)
message(STATUS "my_wifi component being included")
