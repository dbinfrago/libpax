# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources *.*)

idf_component_register(SRCS ${app_sources}
    INCLUDE_DIRS .
    REQUIRES nvs_flash bt spi_flash)
