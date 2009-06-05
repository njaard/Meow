project(meow)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)

find_package(Boost REQUIRED)
macro_log_feature(Boost_FOUND "boost" "Boost C++ Libraries" "http://www.boost.org" TRUE "1_33_0" "Needed by several critical PIM libraries.")

find_package(Sqlite REQUIRED)
macro_log_feature(SQLITE_FOUND "sqlite" "SQLite" "http://www.sqlite.org" TRUE "3" "Storing the playlist")

find_package(Taglib REQUIRED)
macro_log_feature(TAGLIB_FOUND "taglib" "Taglib" "http://developer.kde.org/~wheeler/taglib/" TRUE "" "Reading id3/ogg/etc tags")

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS} ${SQLITE_DEFINITIONS} ${TAGLIB_CFLAGS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

macro_display_feature_log()


set(
	meow_SRCS
	main.cpp
	player.cpp
	mainwindow.cpp
	directoryadder.cpp
	scrobble.cpp
	configdialog.cpp
	fileproperties.cpp
	tooltip.cpp
	
	db/base.cpp db/file.cpp treeview.cpp db/collection.cpp
	)

kde4_add_executable(meow ${meow_SRCS})

target_link_libraries(meow ${SQLITE_LIBRARIES} ${KDE4_KDEUI_LIBS} ${KDE4_KFILE_LIBS} ${KDE4_KIO_LIBS} ${KDE4_PHONON_LIBRARY} ${TAGLIB_LIBRARIES})

install(FILES meow.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES meowui.rc DESTINATION ${DATA_INSTALL_DIR}/meow)
install(TARGETS meow ${INSTALL_TARGETS_DEFAULT_ARGS})

kde4_install_icons(${ICON_INSTALL_DIR})
# kate: space-indent off; replace-tabs off;
