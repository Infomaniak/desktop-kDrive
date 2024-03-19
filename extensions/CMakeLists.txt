add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

if (APPLE)
  add_subdirectory(MacOSX)
endif()
if(BUILD_EXTENSIONS_ICONS)
  add_subdirectory(icons)
endif()

if(MSVC)
  add_subdirectory(windows/standard)
endif()
