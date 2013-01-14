SET(MAJOR_VERSION 0)
SET(MINOR_VERSION 9)
SET(PATCH_VERSION 8)
SET(PACKAGE_VERSION 1)


cmake_minimum_required(VERSION 2.6)
project(meow)

find_package(Automoc4 REQUIRED)

add_definitions(
		"-DMEOW_VERSION=\"${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}\""
	)


include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
if(${CMAKE_SYSTEM_NAME} STREQUAL Windows)
	include_directories(${QT_DIR}/include/Qt ${QT_DIR}/include ${QT_INCLUDES})
	if (NOT IS_DIRECTORY ${EXTRALIBS})
		MESSAGE( FATAL_ERROR "Specify -DEXTRALIBS with the path to the win32 library toolchain.")
	endif()
	set(CMAKE_EXE_LINKER_FLAGS -mwindows )
	set(platform_sources akode/plugins/dsound_sink.cpp mainwindow-qt.cpp md5.c)
	
	include_directories(${EXTRALIBS}/include/taglib ${EXTRALIBS}/include)
	set(EXTRALIBS
		${TAGLIB_LIBRARIES} ${SQLITE_LIBRARIES}
		${QT_LIBRARY_DIR}/libQtGui.a ${QT_LIBRARY_DIR}/libQtNetwork.a
		${QT_LIBRARY_DIR}/libQtCore.a ${QT_LIBRARY_DIR}/libQtXml.a
		${QT_LIBRARY_DIR}/libqtmain.a
		${EXTRALIBS}/lib/libmad.a
		${EXTRALIBS}/lib/libvorbisfile.a ${EXTRALIBS}/lib/libvorbis.a
		${EXTRALIBS}/lib/libogg.a
		${EXTRALIBS}/lib/libFLAC.a
		${EXTRALIBS}/lib/libmpcdec_static.a

		${EXTRALIBS}/lib/libpthreadGC1.a -lws2_32 -ldsound -limm32 -lwinmm
	)
	set(FLAC113_INCLUDE_DIR "${EXTRALIBS}/include")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNOMINMAX -DPTW32_STATIC_LIB")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -DPTW32_STATIC_LIB")
else()
	set(platform_sources akode/plugins/alsa_sink.cpp akode/localfile.cpp)


	if(${MEOW_QT})
		find_package(Qt4 REQUIRED QtCore QtGui QtNetwork QtXml)
		include(${QT_USE_FILE})
		find_path(SQLITE3_INCLUDE_DIR NAMES sqlite3.h
			PATH_SUFFIXES sqlite3
			DOC "sqlite3 include directory"
		)
		find_library(SQLITE3_LIBRARY NAMES sqlite3
			DOC "sqlite3 library"
		)
		FIND_PACKAGE_HANDLE_STANDARD_ARGS(SQLITE3 DEFAULT_MSG SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)

		find_path(TAGLIB_INCLUDE_DIR NAMES taglib.h
			PATH_SUFFIXES taglib
			DOC "taglib include directory"
		)
		find_library(TAGLIB_LIBRARY NAMES tag
			DOC "taglib library"
		)
		FIND_PACKAGE_HANDLE_STANDARD_ARGS(TAGLIB DEFAULT_MSG TAGLIB_LIBRARY TAGLIB_INCLUDE_DIR)
	else()
		find_package(KDE4 REQUIRED)
		include (KDE4Defaults)
		find_package(Sqlite REQUIRED)
		macro_log_feature(SQLITE_FOUND "sqlite" "SQLite" "http://www.sqlite.org" TRUE "3" "Storing the playlist")
		find_package(Taglib REQUIRED)
		macro_log_feature(TAGLIB_FOUND "taglib" "Taglib" "http://developer.kde.org/~wheeler/taglib/" TRUE "" "Reading id3/ogg/etc tags")
	endif()

	find_package(Threads REQUIRED)

	find_package(ALSA REQUIRED)
	# macro_log_feature(ALSA_FOUND "alsa" "ALSA" "OS" TRUE "" "Only supported output on Linux")

	find_path(MAD_INCLUDE_DIR NAMES mad.h
		PATH_SUFFIXES mad
		DOC "MAD mp3 decoder include directory"
	)
	find_library(MAD_LIBRARY NAMES mad
		DOC "MAD mp3 decoder library"
	)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(MAD DEFAULT_MSG MAD_LIBRARY MAD_INCLUDE_DIR)

	find_path(VORBIS_INCLUDE_DIR NAMES vorbis/vorbisfile.h
		PATH_SUFFIXES vorbis
		DOC "vorbis include directory"
	)
	find_library(VORBIS_LIBRARY NAMES vorbisfile vorbis ogg
		DOC "Vorbis decoder library"
	)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(VORBIS DEFAULT_MSG VORBIS_LIBRARY VORBIS_INCLUDE_DIR)

	find_path(FLAC113_INCLUDE_DIR NAMES FLAC/stream_decoder.h
		PATH_SUFFIXES flac113
		DOC "FLAC 1.1.3 decoder include directory"
	)
	find_library(FLAC113_LIBRARY NAMES FLAC
		DOC "FLAC 1.1.3 decoder library"
	)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(FLAC113 DEFAULT_MSG FLAC113_LIBRARY FLAC113_INCLUDE_DIR)

	find_path(MPC_INCLUDE_DIR NAMES mpc/mpcdec.h
		PATH_SUFFIXES mpc
		DOC "MPC decoder include directory"
	)
	find_library(MPC_LIBRARY NAMES mpcdec
		DOC "MPC decoder library"
	)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(MPC DEFAULT_MSG MPC_LIBRARY MPC_INCLUDE_DIR)

	if(${MEOW_QT})
		set(platform_sources ${platform_sources} mainwindow-qt.cpp md5.c)
	else()
		add_definitions(
				"-DMEOW_WITH_KDE"
			)
		add_definitions(
				${KDE4_DEFINITIONS}
			)
		include_directories(
				${KDE4_INCLUDES}
			)
		set(platform_sources ${platform_sources} mainwindow.cpp
			fileproperties.cpp)
	endif()
	include_directories(
			${QT_INCLUDES}
			${ALSA_INCLUDE_DIR}
			${MAD_INCLUDE_DIR}
			${VORBIS_INCLUDE_DIR}
			${FLAC113_INCLUDE_DIR}
			${MPC_INCLUDE_DIR}
		)
	add_definitions(
			${QT_DEFINITIONS}
			${SQLITE_DEFINITIONS}
			${TAGLIB_CFLAGS}
		)
	# macro_display_feature_log()
endif()

if(FLAC113_INCLUDE_DIR)
	add_definitions("-DHAVE_LIBFLAC113")
endif()


set(
	meow_SRCS
	main.cpp
	player.cpp
	scrobble.cpp
	configdialog.cpp
	directoryadder.cpp
	filter.cpp
	
	db/base.cpp db/file.cpp treeview.cpp db/collection.cpp

	akode/audiobuffer.cpp akode/buffered_decoder.cpp
	akode/bytebuffer.cpp akode/converter.cpp akode/crossfader.cpp
	akode/fast_resampler.cpp
	akode/mmapfile.cpp akode/player.cpp akode/plugin.cpp
	akode/void_sink.cpp
	akode/volumefilter.cpp akode/wav_decoder.cpp
	akode/plugins/mpeg_decoder.cpp
	akode/plugins/vorbis_decoder.cpp
	akode/plugins/flac113_decoder.cpp
	akode/plugins/mpc_decoder.cpp
	${platform_sources}
)

if(${CMAKE_SYSTEM_NAME} STREQUAL Windows)
	ADD_CUSTOM_COMMAND(
		OUTPUT meowres.o
		COMMAND i586-mingw32msvc-windres -o ${CMAKE_CURRENT_BINARY_DIR}/meowres.o ${CMAKE_CURRENT_SOURCE_DIR}/meowres.rc
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/meowres.rc
	)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.cpp
		COMMAND rcc -name icons -o ${CMAKE_CURRENT_BINARY_DIR}/icons.cpp ${CMAKE_CURRENT_SOURCE_DIR}/icons.qrc
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/icons.qrc
	)

	AUTOMOC4_ADD_EXECUTABLE(meow ${meow_SRCS} meowres.o icons.cpp)
	TARGET_LINK_LIBRARIES(
		meow
		${EXTRALIBS}
	)
elseif(${MEOW_QT})
	install(FILES meow-qt.desktop DESTINATION share/applications)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/icons.cpp
		COMMAND rcc -name icons -o ${CMAKE_CURRENT_BINARY_DIR}/icons.cpp ${CMAKE_CURRENT_SOURCE_DIR}/icons.qrc
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/icons.qrc
	)
	AUTOMOC4_ADD_EXECUTABLE(meow-qt ${meow_SRCS} icons.cpp)
	target_link_libraries(meow-qt
		${ALSA_LIBRARY} ${SQLITE3_LIBRARY}
		${TAGLIB_LIBRARY} ${MAD_LIBRARY} ${VORBIS_LIBRARY}
		${FLAC113_LIBRARY}
		${QT_LIBRARIES}
		${MPC_LIBRARY}
	)
	install(TARGETS meow-qt DESTINATION bin)
else()
	kde4_add_executable(meow ${meow_SRCS})
	install(FILES meow.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
	install(FILES meowui.rc DESTINATION ${DATA_INSTALL_DIR}/meow)
	install(TARGETS meow ${INSTALL_TARGETS_DEFAULT_ARGS})
	target_link_libraries(meow ${CMAKE_THREAD_LIBS_INIT}
		${ALSA_LIBRARY} ${SQLITE_LIBRARIES} ${KDE4_KDEUI_LIBS} ${KDE4_KFILE_LIBS}
		${KDE4_KIO_LIBS} ${TAGLIB_LIBRARIES} ${MAD_LIBRARY} ${VORBIS_LIBRARY}
		${FLAC113_LIBRARY}
		${MPC_LIBRARY}
	)
	kde4_install_icons(${ICON_INSTALL_DIR})

endif()



IF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
INCLUDE(InstallRequiredSystemLibraries)

SET(CPACK_SET_DESTDIR "on")
SET(CPACK_PACKAGING_INSTALL_PREFIX "/tmp")
SET(CPACK_GENERATOR "DEB")


if(${MEOW_QT})
	SET(CPACK_PACKAGE_DESCRIPTION "A cute music player")
	SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Meow is a minimalistic music player, this is the Qt version")
	SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6 (>= 4.1.1), libqtcore4 (>= 4.5.0), libc6 (>= 2.2), libqtcore4 (>= 4:4.6.0), libasound2 (>= 1.0.23), libvorbis0a (>= 1.3.0), libogg0 (>= 1.1.0), libmad0 (>= 0.15.0), libflac8 (>= 1.2.0), libtag1-vanilla (>= 1.6.0), libsqlite3-0 (>= 3.7.0), libvorbisfile3 (>= 1.3.0), libmpcdec6 (>= 0.1)")
	SET(PROJECT_NAME_SUFFIX "-qt")
	SET(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
	SET(CPACK_DEBIAN_PACKAGE_SECTION "sound")
	SET(CPACK_STRIP_FILES "bin/meow-qt")
	SET(CPACK_PACKAGE_NAME "meow-qt")
else()
	SET(CPACK_PACKAGE_DESCRIPTION "A cute cutest music player")
	SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Meow is a minimalistic music player for KDE")
	SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libkdeui5 (>= 4:4.4.0), libstdc++6 (>= 4.1.1), libkdecore5 (>= 4:4.4.0), libqtcore4 (>= 4.5.0), libc6 (>= 2.2), libqtcore4 (>= 4:4.6.0), libasound2 (>= 1.0.23), libvorbis0a (>= 1.3.0), libogg0 (>= 1.1.0), libmad0 (>= 0.15.0), libflac8 (>= 1.2.0), libtag1-vanilla (>= 1.6.0), libsqlite3-0 (>= 3.7.0), libvorbisfile3 (>= 1.3.0), libmpcdec6 (>= 0.1)")
	SET(CPACK_INSTALL_DIRECTORIES "/usr/share/kde4/apps/meow")
	SET(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
	SET(CPACK_DEBIAN_PACKAGE_SECTION "kde")
	SET(CPACK_STRIP_FILES "bin/meow")
endif()
SET(CPACK_PACKAGE_CONTACT "Charles Samuels <charles@meowplayer.org>")
SET(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR_VERSION}")
SET(CPACK_PACKAGE_VERSION_MINOR "${MINOR_VERSION}")
SET(CPACK_PACKAGE_VERSION_PATCH "${PATCH_VERSION}")
SET(CPACK_DEBIAN_PACKAGE_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}-${PACKAGE_VERSION}")
SET(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}${PROJECT_NAME_SUFFIX}_${CPACK_DEBIAN_PACKAGE_VERSION}_${CMAKE_SYSTEM_PROCESSOR}")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}${PROJECT_NAME_SUFFIX}_${MAJOR_VERSION}.${MINOR_VERSION}.${CPACK_PACKAGE_VERSION_PATCH}")

SET(CPACK_DEBIAN_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})

SET(CPACK_COMPONENTS_ALL Libraries ApplicationData)
INCLUDE(CPack)


ENDIF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")

# kate: space-indent off; replace-tabs off;
