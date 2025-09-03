############################################################################
# FindWhisper.cmake
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################
#
# Find the whisper library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  whisper - If the whisper library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Whisper_FOUND - The whisper library has been found
#  Whisper_TARGET - The name of the CMake target for the whisper library
#
# This module may set the following variable:
#
#  Whisper_USE_BUILD_INTERFACE - If the whisper library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_Whisper_REQUIRED_VARS Whisper_TARGET)
set(_Whisper_CACHE_VARS ${_Whisper_REQUIRED_VARS})

if(TARGET whisper)

	set(Whisper_TARGET whisper)
	set(Whisper_USE_BUILD_INTERFACE TRUE)

else()

	find_path(_Whisper_INCLUDE_DIRS
		NAMES whisper.h
		PATH_SUFFIXES include
	)

	find_library(_Whisper_LIBRARY NAMES whisper)

	if(_Whisper_INCLUDE_DIRS AND _Whisper_LIBRARY)
		add_library(whisper UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(whisper PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Whisper_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_Whisper_LIBRARY}"
			)
		else()
			set_target_properties(whisper PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Whisper_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_Whisper_LIBRARY}"
			)
		endif()

		set(Whisper_TARGET whisper)
	endif()

endif()

find_package_handle_standard_args(Whisper
	REQUIRED_VARS ${_Whisper_REQUIRED_VARS}
)
mark_as_advanced(${_Whisper_CACHE_VARS})
