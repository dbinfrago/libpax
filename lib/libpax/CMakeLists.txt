# Performance-optimized CMakeLists.txt for libpax

FILE(GLOB_RECURSE app_sources *.*)

# Add compiler flags for performance optimization
# -O3: Aggressive optimization
# -finline-functions: Inline simple functions
# -ffast-math: Allow fast math approximations
idf_component_register(SRCS ${app_sources}
    INCLUDE_DIRS .
    REQUIRES nvs_flash bt spi_flash)

# Performance optimization flags
# -funroll-loops intentionally omitted: it inflates code size and overflowed IRAM
target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -finline-functions
    -flto
    -ffunction-sections
    -fdata-sections
)

# Link-time optimization
target_link_options(${COMPONENT_LIB} PRIVATE
    -flto
    -Wl,--gc-sections
)
