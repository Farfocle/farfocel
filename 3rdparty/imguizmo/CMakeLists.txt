set(IMGUIZMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(imguizmo STATIC
    "${IMGUIZMO_DIR}/ImGuizmo.cpp"
    "${IMGUIZMO_DIR}/GraphEditor.cpp"
    "${IMGUIZMO_DIR}/ImCurveEdit.cpp"
    "${IMGUIZMO_DIR}/ImGradient.cpp"
    "${IMGUIZMO_DIR}/ImSequencer.cpp"
    "${IMGUIZMO_DIR}/ImVectorEditor.cpp"
)

target_include_directories(imguizmo
    SYSTEM
    PUBLIC
        "${IMGUIZMO_DIR}"
)

target_link_libraries(imguizmo
    PUBLIC
        imgui
)
