# Home Assistant controller (AJ's branch). Wi-Fi + HA REST client live in HANet.
file(GLOB_RECURSE HOMEASSISTANT_SOURCES "*.cpp" "*.c")
file(GLOB_RECURSE HOMEASSISTANT_HEADERS "*.h")

add_library(HomeAssistantAPP ${HOMEASSISTANT_SOURCES} ${HOMEASSISTANT_HEADERS})

target_include_directories(HomeAssistantAPP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Decided here rather than with __has_include: a compile-definition change forces a rebuild when
# HAConfig.local.h first appears, which header dependency tracking can't see
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/HAConfig.local.h)
    target_compile_definitions(HomeAssistantAPP PRIVATE HA_LOCAL_CONFIG=1)
endif()

target_link_libraries(HomeAssistantAPP PUBLIC
    MatrixOSInterface
    MatrixOSAPPInterface
    idf::esp_wifi
    idf::esp_netif
    idf::esp_event
    idf::esp_http_client
)

# Register the application class
RegisterApplicationClass(HomeAssistant)
