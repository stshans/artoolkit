set(lib_name ARMulti)

if(MSVC)
	add_definitions (/W2 /wd4996 /D "_CRT_SECURE_NO_WARNINGS")
endif()

set(SRCS
	arMultiActivate.c
	arMultiGetTransMat.c
	arMultiReadConfigFile.c
	)
	
include_directories(${CMAKE_SOURCE_DIR}/include)

set(HDRS 
	${CMAKE_SOURCE_DIR}/include/AR/arMulti.h
	)



add_library(${lib_name} STATIC ${HDRS} ${SRCS})

#set_target_properties(${lib_name}
#	PROPERTIES
#	POSITION_INDEPENDENT_CODE TRUE
#	)

artoolkit_install(${lib_name})	
		
install(FILES ${HDRS}
	DESTINATION include/AR
	)
