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
    set(oneValueArgs)
    set(multiValueArgs SHADERS)
    cmake_parse_arguments(FAYE_SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT FAYE_SHADER_SHADERS)
        message(FATAL_ERROR "faye_add_shader_target requires at least one shader file.")
    endif()

    faye_find_glslc(_faye_glslc)

    set(_outputs)

    foreach(_shader IN LISTS FAYE_SHADER_SHADERS)
        get_filename_component(_shader_name "${_shader}" NAME)
        get_filename_component(_shader_dir "${_shader}" DIRECTORY)
        get_filename_component(_shader_ext "${_shader}" LAST_EXT)
        string(REGEX REPLACE "^\\." "" _shader_output_stem "${_shader_ext}")

        if(NOT _shader_output_stem)
            message(FATAL_ERROR "Shader '${_shader}' must have a stage extension such as .vert or .frag.")
        endif()

        set(_output "${_shader_dir}/${_shader_output_stem}.spv")
        list(APPEND _outputs "${_output}")

        add_custom_command(
            OUTPUT "${_output}"
            COMMAND "${_faye_glslc}" "${_shader}" -o "${_output}"
            DEPENDS "${_shader}"
            COMMENT "Compiling shader ${_shader_name}"
            VERBATIM)
    endforeach()

    add_custom_target(${target_name} DEPENDS ${_outputs})
endfunction()