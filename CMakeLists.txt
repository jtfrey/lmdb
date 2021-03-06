CMAKE_MINIMUM_REQUIRED (VERSION 2.6)
PROJECT (lmdb C)

#
# GNU-like installation:
#
INCLUDE(GNUInstallDirs)

#
# For finding packages:
#
INCLUDE(FindPackageHandleStandardArgs)

#
# Provide a default value for LMDB_INSTALL_BINDIR:
#
IF(NOT LMDB_INSTALL_BINDIR)
  SET(LMDB_INSTALL_BINDIR "${CMAKE_INSTALL_FULL_BINDIR}" CACHE PATH "Directory in which programs get installed")
ENDIF(NOT LMDB_INSTALL_BINDIR)

#
# Provide a default value for LMDB_INSTALL_SYSCONFDIR:
#
IF(NOT LMDB_INSTALL_SYSCONFDIR)
  SET(LMDB_INSTALL_SYSCONFDIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/lmdb" CACHE PATH "Directory in which configuration files get installed")
ENDIF(NOT LMDB_INSTALL_SYSCONFDIR)

#
# Provide a default value for LMDB_INSTALL_LOCALSTATEDIR:
#
IF(NOT LMDB_INSTALL_LOCALSTATEDIR)
  SET(LMDB_INSTALL_LOCALSTATEDIR "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/lmdb" CACHE PATH "Directory to hold local state data")
ENDIF(NOT LMDB_INSTALL_LOCALSTATEDIR)

#
# Provide a default value for LMDB_DEFAULT_CONF_FILE:
#
IF(NOT LMDB_DEFAULT_CONF_FILE)
  SET(LMDB_DEFAULT_CONF_FILE "${LMDB_INSTALL_SYSCONFDIR}/lmdb.conf" CACHE PATH "Default configuration file for lmdb")
ENDIF(NOT LMDB_DEFAULT_CONF_FILE)

#
# Declare the LMDB_IGNORE_SQLITE_VERSION flag:
#
SET(LMDB_IGNORE_SQLITE_VERSION OFF CACHE BOOL "Do not check SQLite library version at runtime vs. compile")

#
# Declare the LMDB_DISABLE_RRDTOOL flag:
#
SET(LMDB_DISABLE_RRDTOOL OFF CACHE BOOL "Do not include automated rrdtool updates")

#
# Locate SQLite
#
FIND_PATH(SQLITE3_INCLUDE_DIR NAMES sqlite3.h)
FIND_LIBRARY(SQLITE3_LIBRARY NAMES libsqlite3.so libsqlite3.dylib)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SQLITE3 DEFAULT_MSG SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR)
IF(SQLITE3_FOUND)
	SET(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
	SET(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
ELSE(SQLITE3_FOUND)
	MESSAGE(FATAL_ERROR "SQLite3 library could not be found")
ENDIF(SQLITE3_FOUND)
MARK_AS_ADVANCED(SQLITE3_INCLUDE_DIRS SQLITE3_LIBRARIES)

IF(NOT LMDB_DISABLE_RRDTOOL)
	#
	# Locate RRDTool
	#
	FIND_PATH(RRDTOOL_INCLUDE_DIR NAMES rrd.h)
	FIND_LIBRARY(RRDTOOL_LIBRARY NAMES librrd.so)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(RRDTOOL DEFAULT_MSG RRDTOOL_LIBRARY RRDTOOL_INCLUDE_DIR)
	IF(RRDTOOL_FOUND)
		SET(RRDTOOL_LIBRARIES ${RRDTOOL_LIBRARY})
		SET(RRDTOOL_INCLUDE_DIRS ${RRDTOOL_INCLUDE_DIR})
		IF(NOT LMDB_RRD_REPODIR)
			SET(LMDB_RRD_REPODIR "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/lmdb/rrds" CACHE PATH "Where to store feature rrd files")
		ENDIF(NOT LMDB_RRD_REPODIR)
		ADD_DEFINITIONS(-DLMDB_RRD_REPODIR="${LMDB_RRD_REPODIR}")
	ELSE(RRDTOOL_FOUND)
		MESSAGE("RRDTool library not found, disabling its use")
                ADD_DEFINITIONS(-DLMDB_DISABLE_RRDTOOL)
	ENDIF(RRDTOOL_FOUND)
ELSE(NOT LMDB_DISABLE_RRDTOOL)
	ADD_DEFINITIONS(-DLMDB_DISABLE_RRDTOOL)
ENDIF(NOT LMDB_DISABLE_RRDTOOL)

ADD_DEFINITIONS(-DLMDB_DEFAULT_CONF_FILE="${LMDB_DEFAULT_CONF_FILE}")

IF ( LMDB_IGNORE_SQLITE_VERSION )
	ADD_DEFINITIONS(-DLMDB_IGNORE_SQLITE_VERSION)
ENDIF (LMDB_IGNORE_SQLITE_VERSION)

ADD_DEFINITIONS(-DLMDB_PREFIX_DIR="${CMAKE_INSTALL_PREFIX}")
ADD_DEFINITIONS(-DLMDB_SYSCONF_DIR="${LMDB_INSTALL_SYSCONFDIR}")
ADD_DEFINITIONS(-DLMDB_STATE_DIR="${LMDB_INSTALL_LOCALSTATEDIR}")
INCLUDE_DIRECTORIES(BEFORE ${SQLITE3_INCLUDE_DIRS})

#
# CPack config for producing RPMs
#
SET(CPACK_GENERATOR "RPM")
SET(CPACK_PACKAGE_VERSION_MAJOR 1)
SET(CPACK_PACKAGE_VERSION_MINOR 0)
SET(CPACK_PACKAGE_VERSION_PATCH 1)
SET(CPACK_PACKAGE_VERSION "1.0.1")
SET(CPACK_PACKAGE_RELEASE 1)
SET(CPACK_PACKAGE_NAME "lmdb")
SET(CPACK_PACKAGE_CONTACT "Jeff Frey <frey@udel.edu>")
SET(CPACK_PACKAGE_VENDOR "University of Delaware, IT-NSS")
SET(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
SET(CPACK_RPM_PACKAGE_SUMMARY "Tools for logging FLEXlm license usage to SQLite and RRD")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/rpm-description.txt")
SET(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")
#SET(CPACK_RPM_PACKAGE_REQUIRES "sqlite3 >= 3.0, rrdtool")

ADD_DEFINITIONS(-DLMDB_VERSION_STRING="\"\"${CPACK_PACKAGE_VERSION}\"\"")

#
# Our source subdirectories:
#
ADD_SUBDIRECTORY(lib)
ADD_SUBDIRECTORY(lmdb_cli)
ADD_SUBDIRECTORY(lmdb_nagios_check)
ADD_SUBDIRECTORY(lmdb_report)
ADD_SUBDIRECTORY(lmdb_ls)
ADD_SUBDIRECTORY(etc)

#
# Be sure we get our local state directory created:
#
INSTALL(DIRECTORY DESTINATION "${LMDB_INSTALL_LOCALSTATEDIR}")

IF(NOT LMDB_DISABLE_RRDTOOL)
	ADD_SUBDIRECTORY(graph)

	#
	# Add the rrds and graphs directories to local state:
	#
	INSTALL(DIRECTORY DESTINATION "${LMDB_INSTALL_LOCALSTATEDIR}/rrds" COMPONENT config)
	INSTALL(DIRECTORY DESTINATION "${LMDB_INSTALL_LOCALSTATEDIR}/graphs" COMPONENT config)
ENDIF(NOT LMDB_DISABLE_RRDTOOL)

# For generating packages:
INCLUDE(CPack)

