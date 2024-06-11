project(E2Etestcommon)

set(E2Etestcommon_NAME ${APPLICATION_NAME}_e2e_test_common)

set(E2Etestcommon_SRCS
    ../test.cpp

    test.cpp
    # SelfRestarter
    selfrestarter/testselfrestarter.h selfrestarter/testselfrestarter.cpp
)

if (WIN32)
    include_directories("C:/Projects/cppunit/include")
else()
    include_directories("/usr/local/include")
endif()

add_executable(${E2Etestcommon_NAME} ${E2Etestcommon_SRCS})

set_target_properties(${E2Etestcommon_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY} )

if (WIN32)
    target_link_libraries(${E2Etestcommon_NAME}
        debug
        "C:/Projects/cppunit/lib/cppunitd.lib"
        optimized
        "C:/Projects/cppunit/lib/cppunit.lib")
elseif(APPLE)
    target_link_libraries(${E2Etestcommon_NAME}
        "/usr/local/lib/libcppunit.dylib")
else()
    target_link_libraries(${E2Etestcommon_NAME}
        "/usr/local/lib/libcppunit.so")
endif()

if(APPLE)
    install(CODE "
        message(STATUS \"Fixing library paths for ${E2Etestcommon_NAME}...\")
        execute_process(COMMAND \"install_name_tool\" -change libxxhash.0.dylib @rpath/libxxhash.0.8.2.dylib ${BIN_OUTPUT_DIRECTORY}/${E2Etestcommon_NAME})
        " COMPONENT RUNTIME)
endif()
