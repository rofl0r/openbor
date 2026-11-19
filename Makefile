#
# OpenBOR - http://lavalit.com
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

INCLUDES        = /usr/include/SDL

ifdef BUILD_SDL
INCS           += sdl
endif

INCS         += . source source/adpcmlib source/gamelib source/randlib source/scriptlib source/pnglib source/pcxlib source/lualib

ifdef BUILD_GFX
INCS         += source/gfxlib
endif

INCS += $(INCLUDES)

ADPCM           = source/adpcmlib/adpcm.o

# Lua 5.4 core plus the standard libraries retained for gameplay scripts.
# Coroutine, UTF-8 helper, package/require, io, os, and debug libraries are omitted.
LUALIB          = source/lualib/lapi.o \
				source/lualib/lcode.o \
				source/lualib/lctype.o \
				source/lualib/ldebug.o \
				source/lualib/ldo.o \
				source/lualib/ldump.o \
				source/lualib/lfunc.o \
				source/lualib/lgc.o \
				source/lualib/llex.o \
				source/lualib/lmem.o \
				source/lualib/lobject.o \
				source/lualib/lopcodes.o \
				source/lualib/lparser.o \
				source/lualib/lstate.o \
				source/lualib/lstring.o \
				source/lualib/ltable.o \
				source/lualib/ltm.o \
				source/lualib/lundump.o \
				source/lualib/lvm.o \
				source/lualib/lzio.o \
				source/lualib/lauxlib.o \
				source/lualib/lbaselib.o \
				source/lualib/lmathlib.o \
				source/lualib/lstrlib.o \
				source/lualib/ltablib.o \

ifdef BUILD_GFX
GFX           = source/gfxlib/2xSaI.o source/gfxlib/bilinear.o source/gfxlib/dotmatrix.o source/gfxlib/gfx.o source/gfxlib/hq2x.o source/gfxlib/motionblur.o source/gfxlib/scale2x.o source/gfxlib/scanline.o source/gfxlib/simple2x.o source/gfxlib/tv2x.o
endif

GAME          = source/gamelib/draw.o source/gamelib/draw16.o source/gamelib/draw32.o source/gamelib/font.o source/gamelib/anigif.o source/gamelib/bitmap.o source/gamelib/screen.o source/gamelib/screen16.o source/gamelib/screen32.o source/gamelib/loadimg.o source/gamelib/palette.o source/gamelib/packfile.o source/gamelib/filecache.o source/gamelib/pixelformat.o source/gamelib/soundmix.o source/gamelib/spritef.o source/gamelib/spriteq.o source/gamelib/spritex8p16.o source/gamelib/spritex8p32.o source/gamelib/texture.o source/gamelib/texture16.o source/gamelib/commands.o source/gamelib/models.o source/gamelib/texture32.o source/gamelib/movie.o source/gamelib/menus.o
LISTLIB         = source/scriptlib/List.o
RAND            = source/randlib/rand32.o
PNG             = source/pnglib/pngdec.o source/pnglib/savepng.o
SOURCE          = source/stringptr.o source/utils.o source/stristr.o

ifdef BUILD_SDL
GAME += source/gamelib/filters.o
endif

ifdef BUILD_SDL_IO
GAME_CONSOLE = sdl/joysticks.o sdl/control.o sdl/sblaster.o sdl/timer.o sdl/sdlport.o sdl/video.o sdl/menu.o
endif

ifdef BUILD_OPENGL
GAME_CONSOLE += sdl/opengl.o
endif

ifdef BUILD_LOADGL
GAME_CONSOLE += sdl/loadgl.o
endif

MAIN = openbor.o data.o source/luabindings.o source/lua_stdlibs.o source/crashhandler.o
OBJS = $(MAIN) $(GAME_CONSOLE) $(ADPCM) $(GFX) $(GAME) $(SOURCE) $(LISTLIB) $(RAND) $(PNG) $(LUALIB)

CFLAGS += $(addprefix -I", $(addsuffix ", $(INCS))) $(ARCHFLAGS)
CFLAGS += -Wall -fsigned-char -fcommon -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections

ifndef BUILD_DEBUG
CFLAGS += -fno-ident -freorder-blocks
else
CFLAGS += -DDEBUG -O0 -g
ifdef NO_RAM_DEBUGGER
CFLAGS += -DNO_RAM_DEBUGGER
endif
endif

ifdef BUILD_SDL
CFLAGS += -DSDL
endif

ifdef BUILD_VORBIS
CFLAGS += -DOV_EXCLUDE_STATIC_CALLBACKS
endif

ifdef BUILD_TREMOR
CFLAGS += -DTREMOR
endif

ifdef BUILD_OPENGL
CFLAGS += -DOPENGL
endif

ifdef BUILD_LOADGL
CFLAGS += -DLOADGL
endif

ifdef BUILD_GLES
CFLAGS += -DGLES
endif

ifdef BUILD_VERBOSE
CFLAGS += -DVERBOSE
endif

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = $(addprefix -L", $(addsuffix ", $(LIBRARIES)))

ifdef BUILD_SDL
LIBS += -lSDL -lSDL_gfx
endif

ifdef BUILD_PTHREAD
LIBS += -lpthread
endif

ifdef BUILD_STATIC
LIBS += -static
endif

ifdef BUILD_VORBIS
LIBS += -lvorbisfile -lvorbis -logg
endif

ifdef BUILD_TREMOR
LIBS += -lvorbisidec
endif

LIBS += -lpng -lz -lm

all : $(TARGET) $(TARGET_FINAL)
	@echo

source/luabindings.o: source/luabindings.c
	@echo Compiling Subfolder Script Module: $<...
	@$(CC) $(CFLAGS) -c $< -o $@

%.o : %.asm
	@echo Compiling $(TARGET_PLATFORM) Port: $<...
	@$(YASM) -D $(TARGET_PLATFORM) -f $(OBJTYPE) -m $(TARGET_ARCH) -o $@ $<

%.o : %.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo Compiling $(TARGET_PLATFORM) Port: $<...
	@echo $(CC) $(CFLAGS) -c $< -o $@
	@$(CC) $(CFLAGS) -c $< -o $@

$(TARGET) : $(OBJS) $(RES)
	@echo
	@echo Linking $(TARGET_PLATFORM) Port: $(TARGET)...
	@echo $(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(TARGET_RESOURCE) $(LIBS)
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(OBJS) $(TARGET_RESOURCE) $(LIBS)
	@mkdir -p releases
	@cp $(TARGET) releases/
	@echo Completed $(TARGET_PLATFORM) Port!
	@echo $(TARGET) is now ready inside the releases/ directory!

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
