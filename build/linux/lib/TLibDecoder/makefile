# the SOURCE definiton lets move your makefile to another position
CONFIG 			= LIBRARY

# set pathes to the correct directories
SRC_DIR			= ../../../../source/Lib/TLibDecoder
INC_DIR			= ../../../../source/Lib/TLibDecoder
LIB_DIR			= ../../../../lib
BIN_DIR			= ../../../../bin

SRC_DIR1		=
SRC_DIR2		=
SRC_DIR3		=
SRC_DIR4		=


USER_INC_DIRS	= -I$(SRC_DIR)
USER_LIB_DIRS	=

# intermediate directory for object files
OBJ_DIR			= ./objects

# the library name
PRJ_NAME	 	= TLibDecoder

# version information
MAJOR_VER		= 0
MINOR_VER		= 1
VER				= $(MAJOR_VER).$(MINOR_VER)

# defines to set
DEFS			= -DMSYS_LINUX

# set objects
OBJS          	= \
				$(OBJ_DIR)/TDecBinCoderCABAC.o \
				$(OBJ_DIR)/TDecBinCoderMultiCABAC.o \
				$(OBJ_DIR)/TDecBinCoderPIPE.o \
				$(OBJ_DIR)/TDecBinCoderMultiPIPE.o \
				$(OBJ_DIR)/TDecBinCoderV2VwLB.o \
				$(OBJ_DIR)/TDecPIPETables.o \
				$(OBJ_DIR)/TDecCAVLC.o \
				$(OBJ_DIR)/TDecCu.o \
				$(OBJ_DIR)/TDecEntropy.o \
				$(OBJ_DIR)/TDecGop.o \
				$(OBJ_DIR)/TDecSbac.o \
				$(OBJ_DIR)/TDecSlice.o \
				$(OBJ_DIR)/TDecTop.o \
				$(OBJ_DIR)/TDecV2VTrees.o \

LIBS				= -lpthread

DEBUG_LIBS			=
RELEASE_LIBS		=

STAT_LIBS			=
DYN_LIBS			= -ldl

# the libraries to link with
STAT_DEBUG_LIBS		=
STAT_RELEASE_LIBS	=
DYN_DEBUG_LIBS		=
DYN_RELEASE_LIBS	=

# name of the base makefile
MAKE_FILE_NAME		= ../../common/makefile.base

# include the base makefile
include $(MAKE_FILE_NAME)
