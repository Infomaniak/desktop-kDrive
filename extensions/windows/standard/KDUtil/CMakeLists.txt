add_library(KDUtil STATIC
    CommunicationSocket.cpp
    RemotePathChecker.cpp
    stdafx.cpp
    StringUtil.cpp
    KDUtil.rc
)

target_include_directories(KDUtil
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)
