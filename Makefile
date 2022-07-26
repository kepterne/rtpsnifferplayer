VERSION=1.1
CC=clang
CFLAGS= -Wall \
	-I. \
	-I./lib \
	-I/opt/vc/include \
	-I/opt/vc/include/interface/vcos/pthreads \
	-I/opt/vc/include/interface/vmcs_host/linux \
	-I/opt/vc/src/hello_pi/libs/ilclient \
	-Wno-unused-variable -Wno-unused-result -Wno-pointer-sign -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
	-Wno-unknown-pragmas \
	-Wno-format-overflow \
	-Wno-multichar \
	-Wno-misleading-indentation \
	-Wno-unused-but-set-variable \
	-Wno-unknown-warning-option \
	-Wno-unused-command-line-argument \
	-Wno-enum-conversion \
	-Wno-parentheses-equality \
	-fcommon \
	-L/opt/vc/lib 

CPPFLAGS= -Wall -I./lib -Wno-unused-variable -Wno-unused-result -Wno-int-to-pointer-cast \
	-Wno-unknown-pragmas \
	-Wno-format-overflow 
	
CFLAGS_DEBUG = -g  \


CFLAGS_RELEASE = -ofast \
	-Ofast \
	-funsafe-math-optimizations \
	-mlittle-endian \

BIN= rtpsnifferplayer



SOURCES= $(BIN)_main.c

RELEASE_OBJS = $(patsubst %.c,release/%.o,$(SOURCES))
DEBUG_OBJS = $(patsubst %.c,debug/%.o,$(SOURCES))
OMX_CC_FLAGS = -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT 
OMX_LIB_FLAGS = -L/opt/vc/src/hello_pi/libs/ilclient -L/opt/vc/src/hello_pi/libs/vgfont -lilclient -L/opt/vc/lib -lbrcmGLESv2 -lbrcmEGL -lopenmaxil  

CFLAGS += $(OMX_CC_FLAGS) 

LIBS = -lpthread -ldl -lm -lbcm_host

LIBS += -lvcos
LIBS += -lpcap

BOLD=\033[1m\e[1;32m
BLUE=\033[1m\e[1;34m
NORM=\033[0m\e[0m

debug/lib_%.o: lib/%.c | debug_dir 
	@printf "Compiling\t$(BOLD)%s$(NORM)\r\n\tto\t$(BLUE)%s$(NORM)\r\n" $? $@
	@$(CC) $(CFLAGS_DEBUG) $(CFLAGS) -c $? -o $@
	@printf '%50s\n' | tr ' ' -
debug/%.o: %.c | debug_dir 
	@printf "Compiling\t$(BOLD)%s$(NORM)\r\n\tto\t$(BLUE)%s$(NORM)\r\n" $? $@
	@$(CC) $(CFLAGS_DEBUG) $(CFLAGS) -c $? -o $@
	@printf '%50s\n' | tr ' ' -

release/lib_%.o: lib/%.c | release_dir 
	@printf "Compiling\t$(BOLD)%s$(NORM)\r\n\tto\t$(BLUE)%s$(NORM)\r\n" $? $@
	@$(CC) $(CFLAGS_RELEASE) $(CFLAGS) -c $? -o $@
	@printf '%50s\n' | tr ' ' -

release/%.o: %.c | release_dir 
	@printf "Compiling\t$(BOLD)%s$(NORM)\r\n\tto\t$(BLUE)%s$(NORM)\r\n" $? $@
	@$(CC) $(CFLAGS_RELEASE) $(CFLAGS) -c $? -o $@
	@printf '%50s\n' | tr ' ' -

release/%.opp: %.cpp | release_dir
	@printf "Compiling\t$(BOLD)%s$(NORM)\r\n\tto\t$(BLUE)%s$(NORM)\r\n" $? $@
	@$(CC) $(CFLAGS_RELEASE) $(CPPFLAGS) -c $? -o $@
	@printf '%50s\n' | tr ' ' -

debug/%.opp: %.cpp | debug_dir
	@printf "Compiling\t$(BOLD)%s$(NORM)\r\n\tto\t$(BLUE)%s$(NORM)\r\n" $? $@
	@$(CC) $(CFLAGS_DEBUG) $(CPPFLAGS) -c $? -o $@
	@printf '%50s\n' | tr ' ' -


all: debug release 

debug_dir:
	@mkdir -p debug

release_dir:
	@mkdir -p release
	
bin:
	@mkdir -p ./$@

debug: $(DEBUG_OBJS) bin debug_dir
	@printf "Linking $(BOLD)../bin/$(BIN)_dbg $(NORM) in DEBUG mode\r\n"
	@$(CC) $(CFLAGS) $(CFLAGS_DEBUG) $(DEBUG_OBJS) $(OMX_LIB_FLAGS) -o ./bin/$(BIN)_dbg $(LIBS)
	@printf '%50s\n' | tr ' ' =

release: $(RELEASE_OBJS) bin release_dir 
	@printf "Linking $(BOLD)../bin/$(BIN) $(NORM) in RELEASE mode\r\n"
	@$(CC) $(CFLAGS) $(CFLAGS_DEBUG) $(RELEASE_OBJS) $(OMX_LIB_FLAGS)  -o ./bin/$(BIN) $(LIBS)
	@printf '%50s\n' | tr ' ' =

clean:
	@rm -vfr *~ *.o ./bin/$(BIN)_dbg ./bin/$(BIN) $(RELEASE_OBJS) $(DEBUG_OBJS) release/*.o debug/*.o
	@printf '%50s\n' | tr ' ' =
	@printf "CLEAN COMPLETE\n"
	@printf '%50s\n' | tr ' ' =
