#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux-x86
CND_DLIB_EXT=so
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/cache-test.o \
	${OBJECTDIR}/code-template.o \
	${OBJECTDIR}/fuse-drive-options.o \
	${OBJECTDIR}/fuse-drive.o \
	${OBJECTDIR}/gdrive/gdrive-cache-node.o \
	${OBJECTDIR}/gdrive/gdrive-cache.o \
	${OBJECTDIR}/gdrive/gdrive-download-buffer.o \
	${OBJECTDIR}/gdrive/gdrive-file-contents.o \
	${OBJECTDIR}/gdrive/gdrive-fileid-cache-node.o \
	${OBJECTDIR}/gdrive/gdrive-fileinfo-array.o \
	${OBJECTDIR}/gdrive/gdrive-fileinfo.o \
	${OBJECTDIR}/gdrive/gdrive-info.o \
	${OBJECTDIR}/gdrive/gdrive-json.o \
	${OBJECTDIR}/gdrive/gdrive-query.o \
	${OBJECTDIR}/gdrive/gdrive-sysinfo.o \
	${OBJECTDIR}/gdrive/gdrive-transfer.o \
	${OBJECTDIR}/gdrive/gdrive-util.o


# C Compiler Flags
CFLAGS=-Wextra

# CC Compiler Flags
CCFLAGS=
CXXFLAGS=

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=`pkg-config --libs fuse` `pkg-config --libs libcurl` `pkg-config --libs json-c` -lm   

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/fusedrive

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/fusedrive: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	${LINK.c} -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/fusedrive ${OBJECTFILES} ${LDLIBSOPTIONS}

${OBJECTDIR}/cache-test.o: cache-test.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/cache-test.o cache-test.c

${OBJECTDIR}/code-template.o: code-template.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/code-template.o code-template.c

${OBJECTDIR}/fuse-drive-options.o: fuse-drive-options.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/fuse-drive-options.o fuse-drive-options.c

${OBJECTDIR}/fuse-drive.o: fuse-drive.c 
	${MKDIR} -p ${OBJECTDIR}
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/fuse-drive.o fuse-drive.c

${OBJECTDIR}/gdrive/gdrive-cache-node.o: gdrive/gdrive-cache-node.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-cache-node.o gdrive/gdrive-cache-node.c

${OBJECTDIR}/gdrive/gdrive-cache.o: gdrive/gdrive-cache.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-cache.o gdrive/gdrive-cache.c

${OBJECTDIR}/gdrive/gdrive-download-buffer.o: gdrive/gdrive-download-buffer.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-download-buffer.o gdrive/gdrive-download-buffer.c

${OBJECTDIR}/gdrive/gdrive-file-contents.o: gdrive/gdrive-file-contents.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-file-contents.o gdrive/gdrive-file-contents.c

${OBJECTDIR}/gdrive/gdrive-fileid-cache-node.o: gdrive/gdrive-fileid-cache-node.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-fileid-cache-node.o gdrive/gdrive-fileid-cache-node.c

${OBJECTDIR}/gdrive/gdrive-fileinfo-array.o: gdrive/gdrive-fileinfo-array.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-fileinfo-array.o gdrive/gdrive-fileinfo-array.c

${OBJECTDIR}/gdrive/gdrive-fileinfo.o: gdrive/gdrive-fileinfo.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-fileinfo.o gdrive/gdrive-fileinfo.c

${OBJECTDIR}/gdrive/gdrive-info.o: gdrive/gdrive-info.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-info.o gdrive/gdrive-info.c

${OBJECTDIR}/gdrive/gdrive-json.o: gdrive/gdrive-json.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-json.o gdrive/gdrive-json.c

${OBJECTDIR}/gdrive/gdrive-query.o: gdrive/gdrive-query.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-query.o gdrive/gdrive-query.c

${OBJECTDIR}/gdrive/gdrive-sysinfo.o: gdrive/gdrive-sysinfo.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-sysinfo.o gdrive/gdrive-sysinfo.c

${OBJECTDIR}/gdrive/gdrive-transfer.o: gdrive/gdrive-transfer.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-transfer.o gdrive/gdrive-transfer.c

${OBJECTDIR}/gdrive/gdrive-util.o: gdrive/gdrive-util.c 
	${MKDIR} -p ${OBJECTDIR}/gdrive
	${RM} "$@.d"
	$(COMPILE.c) -g -Wall -DFUSE_USE_VERSION=26 -D_XOPEN_SOURCE=700 `pkg-config --cflags fuse` `pkg-config --cflags libcurl` `pkg-config --cflags json-c` -std=c99  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/gdrive/gdrive-util.o gdrive/gdrive-util.c

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/fusedrive

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
