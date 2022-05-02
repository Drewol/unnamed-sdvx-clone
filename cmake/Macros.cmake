# Precompiled header macro
#	src 	= Path to source files
#	pchSrc 	= Path to precompiled header source file
macro(enable_precompiled_headers src pchSrc)
    if(MSVC)
        #message("Enabling precompiled header generated from source file ${pchSrc}")
        #message("Files using precompiled headers => ${src}")
        # Set precompiled header usage
        set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "/Yu")
        # Set precompiled header
        set_source_files_properties(${pchSrc} PROPERTIES COMPILE_FLAGS "/Yc")
    endif(MSVC)
endmacro(enable_precompiled_headers)

# Excludes a file from precompiled header usage
macro(precompiled_header_exclude exclude)
    if(MSVC)
        # Excluded files
        set_source_files_properties(${exclude} PROPERTIES COMPILE_FLAGS "")
    endif(MSVC)
endmacro(precompiled_header_exclude)

# Set output binary postfixes so that they will be named <project>_<configuration>.exe/dll
macro(set_output_postfixes projectName)
    set_target_properties(${projectName} PROPERTIES 
        OUTPUT_NAME_DEBUG ${projectName}_Debug
        OUTPUT_NAME_RELEASE ${projectName})
endmacro(set_output_postfixes)

macro(set_werror projectName)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND USC_GNU_WERROR)
        target_compile_options(${projectName} PUBLIC -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function)
    endif()
endmacro(set_werror)