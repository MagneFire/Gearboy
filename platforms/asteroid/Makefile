# gpSP makefile
# Gilead Kutnick - Exophase
# pandora port - notaz
# respberry pi - DPR

GEARBOY_SRC=../../src
GEARBOY_AUDIO_SRC=audio
GEARBOY_RPI_SRC=.

include $(GEARBOY_RPI_SRC)/Makefile.common

INCLUDES+= -I$(GEARBOY_SRC) -I./

# Platform specific definitions 

CFLAGS     += -DQCOM_BSP -DHWC_PLUGIN_HAVE_HWCOMPOSER1_API  -Wall
CFLAGS     += -O3 -marm -mcpu=cortex-a7
CFLAGS     += `pkg-config sdl2 --cflags`
CFLAGS     +=-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed
INCFLAGSCLUDES+=`pkg-config android-headers --cflags`
CFLAGS+=`pkg-config libhardware --cflags`
CFLAGS+=`pkg-config hybris-egl-platform --cflags`
CFLAGS+=`pkg-config hwcomposer-egl --cflags`
CXXFLAGS += $(CFLAGS) -fpermissive

# expecting to have PATH set up to get correct sdl-config first

LIBS       += `pkg-config sdl2 --libs`
LIBS       += -ldl -lz -lrt -lm
LIBS +=-lpthread
LIBS +=`pkg-config libsync --libs`
LIBS +=`pkg-config android-headers --libs`
LIBS +=`pkg-config libhardware --libs`
LIBS +=`pkg-config hybris-egl-platform --libs`
LIBS +=`pkg-config hwcomposer-egl --libs`
LIBS += -lGLESv2 -lEGL
LDFLAGS += $(LIBS)

# Compilation:
.SECONDARY: $(OBJS)

all: $(BIN) $(LIB)

%.o: %.cpp
	@rm -f $@
	$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BIN): $(OBJS)
	$(CC) -o $@ -Wl,--whole-archive $(OBJS) $(LDFLAGS) -lstdc++ -Wl,--no-whole-archive -rdynamic

clean:
	for i in $(OBJS); do (if test -e "$$i"; then ( rm $$i ); fi ); done
	@rm -f $(BIN) $(LIB)
