# Bink 独立解码器 - 构建文件
#
# FFmpeg 的 Bink 分离器（demuxer）、Bink 视频解码器和
# Bink 音频解码器的无依赖 C 移植版本（LGPL 2.1+）。

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
LDLIBS   = -lm

SRCS = bink_bitstream.c bink_video.c bink_audio.c bink_demux.c bink_api.c
OBJS = $(SRCS:.c=.o)

BIN    = bink_test
PLAYER = bink_player

# 用于演示播放器的 SDL2（mingw-w64-x86_64-SDL2）
SDL2_CFLAGS = -I G:/msys64/mingw64/include/SDL2
SDL2_LIBS   = -L G:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2

all: $(BIN) $(PLAYER)

$(BIN): $(OBJS) bink_test.c bink_api.h bink_demux.h bink_video.h bink_audio.h bink_bitstream.h bink_common.h bink_data.h
	$(CC) $(CFLAGS) -o $(BIN) $(OBJS) bink_test.c $(LDLIBS)

$(PLAYER): $(OBJS) bink_player.c bink_api.h bink_demux.h bink_video.h bink_audio.h bink_bitstream.h bink_common.h bink_data.h
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) -o $(PLAYER) $(OBJS) bink_player.c $(SDL2_LIBS) $(LDLIBS)

%.o: %.c bink_api.h bink_demux.h bink_video.h bink_audio.h bink_bitstream.h bink_common.h bink_data.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BIN) $(PLAYER) *.ppm *_audio.wav

.PHONY: all clean
