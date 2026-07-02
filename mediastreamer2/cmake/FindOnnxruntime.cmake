include(FindPackageHandleStandardArgs)

find_path(Onnxruntime_INCLUDE_DIRS
	NAMES onnxruntime_cxx_api.h
	PATH_SUFFIXES include include/onnxruntime onnxruntime/core/session
)
find_library(Onnxruntime_LIBRARIES NAMES onnxruntime)

find_package_handle_standard_args(Onnxruntime
	REQUIRED_VARS Onnxruntime_LIBRARIES Onnxruntime_INCLUDE_DIRS
)

if(Onnxruntime_FOUND)
	if(NOT TARGET onnxruntime)
		add_library(onnxruntime UNKNOWN IMPORTED)
		set_target_properties(onnxruntime PROPERTIES
			IMPORTED_LOCATION "${Onnxruntime_LIBRARIES}"
			INTERFACE_INCLUDE_DIRECTORIES "${Onnxruntime_INCLUDE_DIRS}"
		)
	endif()
	set(Onnxruntime_TARGET onnxruntime)
endif()


mark_as_advanced(Onnxruntime_INCLUDE_DIRS Onnxruntime_LIBRARIES)