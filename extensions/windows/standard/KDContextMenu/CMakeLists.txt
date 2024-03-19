add_library(KDContextMenu MODULE
    dllmain.cpp
    KDClientInterface.cpp
    KDContextMenu.cpp
    KDContextMenuFactory.cpp
    KDContextMenuRegHandler.cpp
    stdafx.cpp
    KDContextMenu.rc
    KDContextMenu.def
)

target_link_libraries(KDContextMenu
    KDUtil)

install(TARGETS KDContextMenu
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
