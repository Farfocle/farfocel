project(farfocel_docs)

find_package(Doxygen)

if(DOXYGEN_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        doxygen_awesome_css
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
        GIT_TAG        v2.4.2
    )
    FetchContent_MakeAvailable(doxygen_awesome_css)
    set(FR_DOXYGEN_INPUT "${CMAKE_SOURCE_DIR}/engine/include/fr")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
    
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_EXTRACT_ALL YES)
    
    set(DOXYGEN_GENERATE_TREEVIEW YES)

    set(DOXYGEN_HTML_EXTRA_STYLESHEET 
        "${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome.css"
        "${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome-sidebar-only.css"
    )

    set(DOXYGEN_PROJECT_NAME "Farfocel Game Engine")
    set(DOXYGEN_OUTPUT_DIR "${DOXYGEN_OUTPUT_DIRECTORY}")
    set(DOXYGEN_INPUT "${FR_DOXYGEN_INPUT}")
    set(DOXYGEN_AWESOME_CSS "${doxygen_awesome_css_SOURCE_DIR}/doxygen-awesome.css")
    
    configure_file(Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

    doxygen_add_docs(docs
        ${FR_DOXYGEN_INPUT}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Generating the Doxygen documentation..."
        CONFIG_FILE "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    )

else()
    message("Doxygen not found installed on the system. Skipping generating documentation...")
endif()
