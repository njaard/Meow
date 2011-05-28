cmake_minimum_required(VERSION 2.6)
project(meow)

find_package(Automoc4 REQUIRED)

set(QT_DIR, "/home/charles/dev/qt-mingw")


include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

if(${CMAKE_SYSTEM_NAME} STREQUAL Windows)
	include_directories(${QT_DIR}/include/Qt ${QT_DIR}/include ${QT_INCLUDES})
	if (NOT IS_DIRECTORY ${EXTRALIBS})
		MESSAGE( FATAL_ERROR "Specify -DEXTRALIBS with the path to the win32 library toolchain.")
	endif()
	set(CMAKE_EXE_LINKER_FLAGS -mwindows )
	set(platform_sources akode/plugins/dsound_sink.cpp mainwindow-qt.cpp)
	
	include_directories(${EXTRALIBS}/include/taglib ${EXTRALIBS}/include)
	set(EXTRALIBS
		${TAGLIB_LIBRARIES} ${SQLITE_LIBRARIES}
		${QT_LIBRARY_DIR}/libQtGui.a ${QT_LIBRARY_DIR}/libQtCore.a ${QT_LIBRARY_DIR}/libqtmain.a
		${QT_LIBRARY_DIR}/../plugins/codecs/libqtwcodecs.a
		${QT_LIBRARY_DIR}/../plugins/codecs/libqkrcodecs.a
		${QT_LIBRARY_DIR}/../plugins/codecs/libqcncodecs.a
		${QT_LIBRARY_DIR}/../plugins/codecs/libqjpcodecs.a
		${EXTRALIBS}/lib/libmad.a
		${EXTRALIBS}/lib/libvorbisfile.a ${EXTRALIBS}/lib/libvorbis.a
		${EXTRALIBS}/lib/libogg.a

		${EXTRALIBS}/lib/libpthreadGC1.a -lws2_32 -ldsound -limm32 -lwinmm
	)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNOMINMAX -DPTW32_STATIC_LIB")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -DPTW32_STATIC_LIB")
else()
	set(platform_sources akode/plugins/alsa_sink.cpp mainwindow.cpp
		scrobble.cpp directoryadder.cpp configdialog.cpp fileproperties.cpp)

	find_package(KDE4 REQUIRED)
	include (KDE4Defaults)
	include (MacroLibrary)
	find_package(Sqlite REQUIRED)
	macro_log_feature(SQLITE_FOUND "sqlite" "SQLite" "http://www.sqlite.org" TRUE "3" "Storing the playlist")

	find_package(Taglib REQUIRED)
	macro_log_feature(TAGLIB_FOUND "taglib" "Taglib" "http://developer.kde.org/~wheeler/taglib/" TRUE "" "Reading id3/ogg/etc tags")

	find_package(Threads REQUIRED)

	find_package(ALSA REQUIRED)
	macro_log_feature(ALSA_FOUND "alsa" "ALSA" "OS" TRUE "" "Only supported output on Linux")

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

	add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS} ${SQLITE_DEFINITIONS} ${TAGLIB_CFLAGS} "-DMEOW_WITH_KDE")
	macro_display_feature_log()
	include_directories(${KDE4_INCLUDES} ${ALSA_INCLUDE_DIR} ${MAD_INCLUDE_DIR} ${VORBIS_INCLUDE_DIR})
endif()



set(
	meow_SRCS
	main.cpp
	player.cpp
	
	db/base.cpp db/file.cpp treeview.cpp db/collection.cpp

	akode/audiobuffer.cpp akode/buffered_decoder.cpp
	akode/bytebuffer.cpp akode/converter.cpp akode/crossfader.cpp
	akode/fast_resampler.cpp akode/localfile.cpp
	akode/mmapfile.cpp akode/player.cpp akode/plugin.cpp
	akode/void_sink.cpp
	akode/volumefilter.cpp akode/wav_decoder.cpp
	akode/plugins/mpeg_decoder.cpp
	akode/plugins/vorbis_decoder.cpp
	${platform_sources}
)
if(${CMAKE_SYSTEM_NAME} STREQUAL Windows)
	AUTOMOC4_ADD_EXECUTABLE(meow ${meow_SRCS})
	TARGET_LINK_LIBRARIES(
		meow
		${EXTRALIBS}
	)
else()
	kde4_add_executable(meow ${meow_SRCS})
	install(FILES meow.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
	install(FILES meowui.rc DESTINATION ${DATA_INSTALL_DIR}/meow)
	install(TARGETS meow ${INSTALL_TARGETS_DEFAULT_ARGS})
	target_link_libraries(meow ${CMAKE_THREAD_LIBS_INIT}
		${ALSA_LIBRARY} ${SQLITE_LIBRARIES} ${KDE4_KDEUI_LIBS} ${KDE4_KFILE_LIBS}
		${KDE4_KIO_LIBS} ${TAGLIB_LIBRARIES} ${MAD_LIBRARY} ${VORBIS_LIBRARY}
	)
	kde4_install_icons(${ICON_INSTALL_DIR})

endif()



# kate: space-indent off; replace-tabs off;
