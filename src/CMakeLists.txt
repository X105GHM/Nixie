# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

file(GLOB_RECURSE app_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "."
    REQUIRES
        app_update
        cxx
        esp_adc
        esp_app_format
        esp_bootloader_format
        esp_driver_gpio
        esp_driver_spi
        esp_event
        spi_flash
        esp_http_client
        esp_http_server
        esp_hw_support
        esp_netif
        esp_partition
        esp_system
        esp_timer
        esp_wifi
        freertos
        json
        lwip
        mbedtls
        nvs_flash
        spiffs
)
