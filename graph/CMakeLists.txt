CMAKE_MINIMUM_REQUIRED (VERSION 2.6)

#
# Locate the rrdtool utility:
#
FIND_PROGRAM(RRDTOOL_EXE rrdtool DOC "path to the rrdtool program")
IF(RRDTOOL_EXE_NOTFOUND)
	MESSAGE("RRDTool utility not found, using 'rrdtool' literal")
	SET(RRDTOOL_EXE "rrdtool")
ENDIF(RRDTOOL_EXE_NOTFOUND)

#
# Provide a default value for LMDB_GRAPH_REPODIR:
#
IF(NOT LMDB_GRAPH_REPODIR)
	SET(LMDB_GRAPH_REPODIR "${LMDB_INSTALL_LOCALSTATEDIR}/graphs" CACHE PATH "Directory to hold generated license usage graphs")
ENDIF(NOT LMDB_GRAPH_REPODIR)

#
# The configuration file for the script is expected to be
# in the usual location:
#
SET(LMDB_GRAPH_CONF "${LMDB_INSTALL_SYSCONFDIR}/graph.conf")

#
# Replace variable references in graph.in and output as graph
#
CONFIGURE_FILE(graph.in graph @ONLY NEWLINE_STYLE UNIX)

#
# Installation
# ~~~~~~~~~~~~
#
# Copy the generated 'graph' to the binary installation directory with
# appropriate permissions -- including execute
#
INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/graph" DESTINATION ${LMDB_INSTALL_BINDIR} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
#
# Copy the example configuration file
INSTALL(FILES graph.conf.example DESTINATION ${LMDB_INSTALL_SYSCONFDIR} COMPONENT config)

