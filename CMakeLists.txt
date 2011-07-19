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
	set(platform_sources akode/plugins/dsound_sink.cpp mainwindow-qt.cpp md5.c)
	
	include_directories(${EXTRALIBS}/include/taglib ${EXTRALIBS}/include)
	set(EXTRALIBS
		${TAGLIB_LIBRARIES} ${SQLITE_LIBRARIES}
		${QT_LIBRARY_DIR}/libQtGui.a ${QT_LIBRARY_DIR}/libQtNetwork.a ${QT_LIBRARY_DIR}/libQtCore.a ${QT_LIBRARY_DIR}/libqtmain.a
		${EXTRALIBS}/lib/libmad.a
		${EXTRALIBS}/lib/libvorbisfile.a ${EXTRALIBS}/lib/libvorbis.a
		${EXTRALIBS}/lib/libogg.a
		${EXTRALIBS}/lib/libFLAC.a

		${EXTRALIBS}/lib/libpthreadGC1.a -lws2_32 -ldsound -limm32 -lwinmm
	)
	set(FLAC113_INCLUDE_DIR "${EXTRALIBS}/include")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNOMINMAX -DPTW32_STATIC_LIB")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -DPTW32_STATIC_LIB")
else()
	set(platform_sources akode/plugins/alsa_sink.cpp mainwindow.cpp
		fileproperties.cpp)

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

	find_path(FLAC113_INCLUDE_DIR NAMES FLAC/stream_decoder.h
		PATH_SUFFIXES flac113
		DOC "FLAC 1.1.3 decoder include directory"
	)
	find_library(FLAC113_LIBRARY NAMES FLAC
		DOC "FLAC 1.1.3 decoder library"
	)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(FLAC113 DEFAULT_MSG FLAC113_LIBRARY FLAC113_INCLUDE_DIR)

#	find_path(MPC_INCLUDE_DIR NAMES mpc/mpcdec.h
#		PATH_SUFFIXES mpc
#		DOC "MPC decoder include directory"
#	)
#	find_library(MPC_LIBRARY NAMES mpcdec
#		DOC "MPC decoder library"
#	)
#	FIND_PACKAGE_HANDLE_STANDARD_ARGS(MPC DEFAULT_MSG MPC_LIBRARY MPC_INCLUDE_DIR)

	add_definitions(
			${QT_DEFINITIONS}
			${KDE4_DEFINITIONS}
			${SQLITE_DEFINITIONS}
			${TAGLIB_CFLAGS}
			"-DMEOW_WITH_KDE"
		)
	macro_display_feature_log()
	include_directories(
			${KDE4_INCLUDES}
			${ALSA_INCLUDE_DIR}
			${MAD_INCLUDE_DIR}
			${VORBIS_INCLUDE_DIR}
			${FLAC113_INCLUDE_DIR}
#			${MPC_INCLUDE_DIR}
		)
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

	akode/plugins/mppdec/bitstream.cpp akode/plugins/mppdec/huffsv46.cpp
	akode/plugins/mppdec/huffsv7.cpp akode/plugins/mppdec/idtag.cpp
	akode/plugins/mppdec/mpc_dec.cpp akode/plugins/mppdec/requant.cpp
	akode/plugins/mppdec/stdafx.cpp akode/plugins/mppdec/streaminfo.cpp
	akode/plugins/mppdec/synth_filter.cpp
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
else()
	kde4_add_executable(meow ${meow_SRCS})
	install(FILES meow.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
	install(FILES meowui.rc DESTINATION ${DATA_INSTALL_DIR}/meow)
	install(TARGETS meow ${INSTALL_TARGETS_DEFAULT_ARGS})
	target_link_libraries(meow ${CMAKE_THREAD_LIBS_INIT}
		${ALSA_LIBRARY} ${SQLITE_LIBRARIES} ${KDE4_KDEUI_LIBS} ${KDE4_KFILE_LIBS}
		${KDE4_KIO_LIBS} ${TAGLIB_LIBRARIES} ${MAD_LIBRARY} ${VORBIS_LIBRARY}
		${FLAC113_LIBRARY}
#		${MPC_LIBRARY}
	)
	kde4_install_icons(${ICON_INSTALL_DIR})

endif()



# kate: space-indent off; replace-tabs off;
