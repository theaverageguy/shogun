IF (CMAKE_USE_PTHREADS_INIT)
	SET(_bindir "${CMAKE_MODULE_PATH}/")
	IF (DARWIN)
		TRY_COMPILE(HAVE_SPINLOCK "${_bindir}" "${CMAKE_MODULE_PATH}/spinlock-test-darwin.cpp")
	ELSE ()
		TRY_COMPILE(HAVE_SPINLOCK "${_bindir}" "${CMAKE_MODULE_PATH}/spinlock-test.cpp")
	ENDIF ()

	IF (HAVE_SPINLOCK)
		MESSAGE(STATUS "Spinlock support found")
		SET(SPINLOCK_FOUND TRUE)
	ELSE (HAVE_SPINLOCK)
		MESSAGE(STATUS "Spinlock support not found")
		SET(SPINLOCK_FOUND FALSE)
	ENDIF (HAVE_SPINLOCK)
ELSE ()
		MESSAGE(STATUS "Spinlock support not found due to no pthreads available")
		SET(SPINLOCK_FOUND FALSE)
ENDIF ()

