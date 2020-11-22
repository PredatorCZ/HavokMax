cmake_minimum_required(VERSION 3.3)

project(HavokMax VERSION 1.12)

set (HavokLibLibraryPath ../HavokLib)

add_subdirectory(3rd_party/HavokLib ${HavokLibLibraryPath})
include(${PRECORE_SOURCE_DIR}/cmake/3dsmax.cmake)

build_target(
	NAME HavokMax
	TYPE SHARED
	SOURCES
		src/HavokExport.cpp
		src/HavokImport.cpp
		src/HavokMax.cpp
		src/DllEntry.cpp
		src/HavokMax.rc
		${MAX_EX_DIR}/win/About.rc
	LINKS
		gdiplus bmm core HavokLib flt mesh maxutil maxscrpt paramblk2 geom MaxSDKTarget
	AUTHOR "Lukas Cone"
	DESCR "Havok 3DS Max Unofficial Plugin"
	START_YEAR 2016
	PROPERTIES
		SUFFIX .dlu
		${MaxProperties}
)

set_precore_sources(HavokMax directory_scanner)
