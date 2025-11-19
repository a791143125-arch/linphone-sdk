############################################################################
# FindVosk.cmake
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
# Find the vosk library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  vosk - If the vosk library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  Vosk_FOUND - The vosk library has been found
#  Vosk_TARGET - The name of the CMake target for the vosk library
#
# This module may set the following variable:
#
#  Vosk_USE_BUILD_INTERFACE - If the vosk library is used from its build directory


include(FindPackageHandleStandardArgs)

set(_Vosk_REQUIRED_VARS Vosk_TARGET)
set(_Vosk_CACHE_VARS ${_Vosk_REQUIRED_VARS})

if(TARGET vosk)

	set(Vosk_TARGET vosk)
	set(Vosk_USE_BUILD_INTERFACE TRUE)

else()

	find_path(_Vosk_INCLUDE_DIRS
		NAMES vosk_api.h
		PATH_SUFFIXES include
	)

	find_library(_Vosk_LIBRARY NAMES vosk)

	if(_Vosk_INCLUDE_DIRS AND _Vosk_LIBRARY)
		add_library(vosk UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(vosk PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Vosk_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_Vosk_LIBRARY}"
			)
		else()
			set_target_properties(vosk PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_Vosk_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_Vosk_LIBRARY}"
			)
		endif()

		set(Vosk_TARGET vosk)
	endif()

endif()

find_package_handle_standard_args(Vosk
	REQUIRED_VARS ${_Vosk_REQUIRED_VARS}
)
mark_as_advanced(${_Vosk_CACHE_VARS})