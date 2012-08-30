#
# OpenBOR - http://www.LavaLit.com
# ---------------------------------------------------------------------------------------------------
# Licensed under the BSD license, see LICENSE in OpenBOR root for details.
#
# Copyright (c) 2004 - 2011 OpenBOR Team
#

ifndef VERSION_NAME
VERSION_NAME = OpenBOR
endif

STRINGSWITCH_GEN=../stringswitch/stringswitch_gen.out

TARGET          = $(VERSION_NAME)
BUILD_SDL       = 1
BUILD_GFX       = 1
BUILD_PTHREAD   = 1
BUILD_SDL_IO    = 1
ifndef NO_GL
BUILD_OPENGL    = 1
BUILD_LOADGL    = 1
CFLAGS += -DOPENGL
endif
BUILD_VORBIS    = 1
BUILDING        = 1
CC              ?= gcc
INCLUDES        = $(SDKPATH)/include \
                  $(SDKPATH)/include/SDL

#----------------------------------------------------------------------------------------------------
# Directories
#----------------------------------------------------------------------------------------------------

ifdef BUILD_SDL
INCS           += sdl
endif

INCS 	       += .                                                                                 \
                  source                                                                            \
                  source/adpcmlib                                                                   \
                  source/gamelib                                                                    \
                  source/preprocessorlib                                                            \
                  source/randlib                                                                    \
                  source/scriptlib                                                                  \
                  source/pnglib                                                                     \
                  source/pcxlib

ifdef BUILD_GFX
INCS 	       += source/gfxlib
endif

INCS += $(INCLUDES)

#----------------------------------------------------------------------------------------------------
# Objects
#----------------------------------------------------------------------------------------------------

ADPCM 	        = source/adpcmlib/adpcm.o

ifdef BUILD_GFX
GFX 	        = source/gfxlib/2xSaI.o                                                             \
                  source/gfxlib/bilinear.o                                                          \
                  source/gfxlib/dotmatrix.o                                                         \
                  source/gfxlib/gfx.o                                                               \
                  source/gfxlib/hq2x.o                                                              \
                  source/gfxlib/motionblur.o                                                        \
                  source/gfxlib/scale2x.o                                                           \
                  source/gfxlib/scanline.o                                                          \
                  source/gfxlib/simple2x.o                                                          \
                  source/gfxlib/tv2x.o
endif
		  
GAME	        = source/gamelib/draw.o                                                             \
                  source/gamelib/draw16.o                                                           \
                  source/gamelib/draw32.o                                                           \
                  source/gamelib/font.o                                                             \
                  source/gamelib/anigif.o                                                           \
                  source/gamelib/bitmap.o 	                                                        \
                  source/gamelib/screen.o                                                           \
                  source/gamelib/screen16.o                                                         \
                  source/gamelib/screen32.o                                                         \
                  source/gamelib/loadimg.o                                                          \
                  source/gamelib/palette.o                                                          \
                  source/gamelib/packfile.o                                                         \
                  source/gamelib/filecache.o                                                        \
                  source/gamelib/pixelformat.o                                                      \
                  source/gamelib/soundmix.o                                                         \
                  source/gamelib/spritef.o                                                          \
                  source/gamelib/spriteq.o                                                          \
                  source/gamelib/spritex8p16.o                                                      \
                  source/gamelib/spritex8p32.o                                                      \
                  source/gamelib/texture.o                                                          \
                  source/gamelib/texture16.o                                                        \
                  source/gamelib/commands.o                                                         \
                  source/gamelib/models.o                                                           \
                  source/gamelib/texture32.o source/gamelib/movie.o source/gamelib/menus.o
SCRIPT          = source/scriptlib/StackedSymbolTable.o                                             \
                  source/scriptlib/ScriptVariant.o                                                  \
                  source/scriptlib/SymbolTable.o                                                    \
                  source/scriptlib/Instruction.o                                                    \
                  source/scriptlib/Interpreter.o                                                    \
                  source/scriptlib/ImportCache.o                                                    \
                  source/scriptlib/ParserSet.o                                                      \
                  source/scriptlib/Parser.o                                                         \
                  source/scriptlib/Lexer.o                                                          \
                  source/scriptlib/Stack.o                                                          \
                  source/scriptlib/List.o                                                           \
                  source/scriptlib/SolidList.o                                                      \
                  source/preprocessorlib/pp_lexer.o                                                 \
                  source/preprocessorlib/pp_parser.o
RAND	        = source/randlib/rand32.o
PNG             = source/pnglib/pngdec.o source/pnglib/savepng.o
SOURCE	        = source/stringptr.o                                                                \
                  source/utils.o                                                                    \
                  source/stristr.o


ifdef BUILD_SDL
GAME	       += source/gamelib/filters.o
endif



ifdef BUILD_SDL_IO
GAME_CONSOLE   += sdl/joysticks.o                                                                   \
                  sdl/control.o                                                                     \
                  sdl/sblaster.o                                                                    \
                  sdl/timer.o                                                                       \
                  sdl/sdlport.o                                                                     \
                  sdl/video.o                                                                       \
                  sdl/menu.o                                                                        
endif


ifdef BUILD_OPENGL
GAME_CONSOLE   += sdl/opengl.o
endif


ifdef BUILD_LOADGL
GAME_CONSOLE   += sdl/loadgl.o
endif

MAIN = openbor.o data.o openborscript.o

OBJS = $(MAIN) $(GAME_CONSOLE) $(ADPCM) $(GFX) $(GAME) $(SOURCE) $(SCRIPT) $(RAND) $(PNG)
                  
		  
#----------------------------------------------------------------------------------------------------
# Compiler Flags
#----------------------------------------------------------------------------------------------------

CFLAGS 	       += $(addprefix -I", $(addsuffix ", $(INCS))) $(ARCHFLAGS)
CFLAGS 	       += -Wall -fsigned-char

ifndef BUILD_DEBUG
  CFLAGS 	       += -fno-ident -freorder-blocks 
else
  CFLAGS 	       += -DDEBUG -O0 -g
  ifdef NO_RAM_DEBUGGER
    CFLAGS         += -DNO_RAM_DEBUGGER 
  endif
endif

ifdef BUILD_SDL
CFLAGS 	       += -DSDL
endif

ifdef BUILD_VORBIS
CFLAGS         += -DOV_EXCLUDE_STATIC_CALLBACKS
endif


ifdef BUILD_TREMOR
CFLAGS         += -DTREMOR
endif


ifdef BUILD_OPENGL
CFLAGS         += -DOPENGL
endif


ifdef BUILD_LOADGL
CFLAGS         += -DLOADGL
endif


ifdef BUILD_GLES
CFLAGS         += -DGLES
endif


ifdef BUILD_VERBOSE
CFLAGS         += -DVERBOSE
endif


CXXFLAGS        = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS         = $(CFLAGS)

#----------------------------------------------------------------------------------------------------
# Library
#----------------------------------------------------------------------------------------------------

LIBS            = $(addprefix -L", $(addsuffix ", $(LIBRARIES)))


ifdef BUILD_SDL
LIBS           += -lSDL -lSDL_gfx
endif


ifdef BUILD_PTHREAD
LIBS           += -lpthread 
endif

ifdef BUILD_STATIC
LIBS           += -static
endif

ifdef BUILD_VORBIS
LIBS           += -lvorbisfile -lvorbis -logg
endif

ifdef BUILD_TREMOR
LIBS           += -lvorbisidec
endif


LIBS           += -lpng -lz -lm


#----------------------------------------------------------------------------------------------------
# Rules to manage Files and Libraries for SDL
#----------------------------------------------------------------------------------------------------

all : $(TARGET) $(TARGET_FINAL)
	@echo
%.o : %.asm
	@echo Compiling $(TARGET_PLATFORM) Port: $<...
	@$(YASM) -D $(TARGET_PLATFORM) -f $(OBJTYPE) -m $(TARGET_ARCH) -o $@ $<
%.o : %.c
	@echo Compiling $(TARGET_PLATFORM) Port: $<...
	@echo $(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -c $< -o $@
$(TARGET) : $(OBJS) $(RES)
	@echo
	@echo Linking $(TARGET_PLATFORM) Port: $(TARGET)...
	@echo $(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(TARGET_RESOURCE) $(LIBS) 
	@$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(TARGET_RESOURCE) $(LIBS) 
	@echo Completed $(TARGET_PLATFORM) Port!
	@echo $(TARGET) is now ready!

#call make switch_gen to re-generate the stringswitch files
switch_gen:
	$(STRINGSWITCH_GEN) openbor.c

clean-all: clean-releases clean

clean-releases:
	@rm -rf releases/* 
	
clean:
	@echo
	@echo "Removing All $(TARGET_PLATFORM) Files..."
	@rm -f $(TARGET) $(TARGET_MAP) PARAM.SFO linkmap $(OBJS)
	@echo Done!
	@echo

version:
	@echo "-------------------------------------------------------"
	@echo "OpenBOR $(VERSION) - http://www.LavaLit.com"
	@echo 
	@echo "Licensed under the BSD license."
	@echo "See LICENSE and README within OpenBOR root for details."
	@echo 
	@echo "Copyright (c) 2004 - 2011 OpenBOR Team"
	@echo "-------------------------------------------------------"

