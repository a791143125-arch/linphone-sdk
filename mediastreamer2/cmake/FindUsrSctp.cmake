############################################################################
# FindUsrSctp.cmake
# Copyright (C) 2025  Belledonne Communications, Grenoble France
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
# Find the usrsctp library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  usrsctp - If the usrsctp library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  USRSCTP_FOUND - The usrsctp library has been found
#  USRSCTP_TARGET - The name of the CMake target for the usrsctp library
#


include(FindPackageHandleStandardArgs)

set(_USRSCTP_REQUIRED_VARS USRSCTP_TARGET)

if(TARGET usrsctp)
	set (USRSCTP_FOUND TRUE)
	set (USRSCTP_TARGET usrsctp)
else()
	set (USRSCTP_FOUND FALSE)
endif()

find_package_handle_standard_args(UsrSctp
	REQUIRED_VARS ${_USRSCTP_REQUIRED_VARS}
)
