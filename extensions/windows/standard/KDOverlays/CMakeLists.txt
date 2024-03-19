add_library(KDOverlays MODULE
    DllMain.cpp
    KDOverlay.cpp
    KDOverlayFactory.cpp
    KDOverlayRegistrationHandler.cpp
    stdafx.cpp
    KDOverlay.rc
    KDOverlays.def
)

target_link_libraries(KDOverlays
    KDUtil)

install(TARGETS KDOverlays
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
)
