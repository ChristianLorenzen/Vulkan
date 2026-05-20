include_guard(GLOBAL)

function(faye_find_glslc out_var)
    if(DEFINED ENV{VULKAN_SDK})
        set(_vulkan_sdk_glslc
            "$ENV{VULKAN_SDK}/Bin/glslc"
            "$ENV{VULKAN_SDK}/bin/glslc")
    endif()

    find_program(_faye_glslc
        NAMES glslc
        HINTS ${_vulkan_sdk_glslc})

    if(NOT _faye_glslc)
        message(FATAL_ERROR "glslc was not found. Install shaderc/glslc or set VULKAN_SDK.")
    endif()

    set(${out_var} "${_faye_glslc}" PARENT_SCOPE)
endfunction()

function(faye_add_shader_target target_name)
    set(options)
    set(oneValueArgs OUTPUT_DIRECTORY)
    set(multiValueArgs SHADERS)
    cmake_parse_arguments(FAYE_SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT FAYE_SHADER_SHADERS)
        message(FATAL_ERROR "faye_add_shader_target requires at least one shader file.")
    endif()

    faye_find_glslc(_faye_glslc)

    set(_outputs)
    set(_shader_source_root "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

    foreach(_shader IN LISTS FAYE_SHADER_SHADERS)
        get_filename_component(_shader_name "${_shader}" NAME)

        if(NOT _shader_name MATCHES "\\.[^.]+$")
            message(FATAL_ERROR "Shader '${_shader}' must have a stage extension such as .vert or .frag.")
        endif()

        if(FAYE_SHADER_OUTPUT_DIRECTORY)
            file(RELATIVE_PATH _shader_relative "${_shader_source_root}" "${_shader}")

            if(_shader_relative MATCHES "^\\.\\.")
                get_filename_component(_shader_relative "${_shader}" NAME)
            endif()

            set(_output "${FAYE_SHADER_OUTPUT_DIRECTORY}/${_shader_relative}.spv")
            get_filename_component(_output_dir "${_output}" DIRECTORY)
        else()
            set(_output "${_shader}.spv")
            get_filename_component(_output_dir "${_output}" DIRECTORY)
        endif()

        list(APPEND _outputs "${_output}")

        add_custom_command(
            OUTPUT "${_output}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_output_dir}"
            COMMAND "${_faye_glslc}" "${_shader}" -I "${_shader_source_root}" -o "${_output}"
            DEPENDS "${_shader}"
            COMMENT "Compiling shader ${_shader_name}"
            VERBATIM)
    endforeach()

    add_custom_target(${target_name} DEPENDS ${_outputs})
endfunction()