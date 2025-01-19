# Set the library name
set(COMPONENT_NAME common_lib)

# Add the source files for the frontend component
add_library(common_lib STATIC config.hpp ip_utils.hpp)
target_link_libraries(common_lib PUBLIC proto_lib)