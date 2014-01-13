# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Windows)
 
# which compilers to use for C and C++
SET(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
SET(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
SET(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> <FLAGS> <DEFINES> --input-format rc --output-format coff -i <SOURCE> -o <OBJECT>")

set(CMAKE_CXX_FLAGS_DEBUG "-g3 -std=c++11")

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# FindQt4.cmake querys qmake to get information, this doesn't work when crosscompiling
set(MEOW_TOOLCHAIN_DIR ${EXTRALIBS})
set(QT_DIR   ${MEOW_TOOLCHAIN_DIR})
set(QT_BINARY_DIR   ${QT_DIR}/bin)
set(QT_LIBRARY_DIR  ${QT_DIR}/lib)
set(QT_HEADERS_DIR  ${QT_DIR}/include)
set(QT_QTCORE_LIBRARY   ${QT_LIBRARY_DIR}/libQtCore4.a)
set(QT_QTCORE_INCLUDE_DIR ${QT_DIR}/include/QtCore)


set(QT_QMAKE_CHANGED false)
set(QT_PLUGINS_DIR ${QT_DIR}/lib/qt4)
set(QT_QTGUI_INCLUDE_DIR ${QT_DIR}/include/QtGui)
set(QT_QT_INCLUDE_DIR ${QT_LIBRARY_DIR}/include/Qt)
set(QT_MKSPECS_DIR  ${QT_DIR}/mkspecs)
set(QT_MOC_EXECUTABLE  ${QT_BINARY_DIR}/moc)
set(QT_QMAKE_EXECUTABLE  ${QT_BINARY_DIR}/qmake)
set(QT_UIC_EXECUTABLE  ${QT_BINARY_DIR}/uic)
set(QT_RCC_EXECUTABLE  ${QT_BINARY_DIR}/rcc)

set(TAGLIB_LIBRARIES ${MEOW_TOOLCHAIN_DIR}/lib/libtag.a)
set(SQLITE_LIBRARIES ${MEOW_TOOLCHAIN_DIR}/lib/libsqlite3.a)

