# https://bloerg.net/2012/11/10/cmake-and-distutils.html

find_program(PYTHON "python")

if (PYTHON)
    set(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
    set(SETUP_PY    "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
    set(DEPS        "${CMAKE_CURRENT_SOURCE_DIR}/__init__.py" "${CMAKE_CURRENT_SOURCE_DIR}/nims_py_ext.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/nims_py_ext.h" "${CMAKE_CURRENT_SOURCE_DIR}/nims_py.pxd" "${CMAKE_CURRENT_SOURCE_DIR}/nims_py.pyx" "${CMAKE_CURRENT_SOURCE_DIR}/tracks_server.py")
    set(OUTPUT      "${CMAKE_CURRENT_BINARY_DIR}/build/timestamp")

    configure_file(${SETUP_PY_IN} ${SETUP_PY})

    add_custom_command(OUTPUT ${OUTPUT}
                       COMMAND ${PYTHON} ${SETUP_PY} install --home=$<TARGET_FILE_DIR:nims> --install-scripts=$<TARGET_FILE_DIR:nims>
                       COMMAND ${CMAKE_COMMAND} -E touch ${OUTPUT}
                       DEPENDS ${DEPS})

    add_custom_target(nims_py ALL DEPENDS ${OUTPUT})

    #install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install --prefix=$<TARGET_FILE_DIR:nims>)")
endif()
