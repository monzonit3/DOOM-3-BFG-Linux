/*
 * Bink decoder - standalone C port from FFmpeg
 *
 * This file is part of the standalone Bink decoder project (LGPL 2.1+).
 * See bink_common.h for the full license header.
 */

/**
 * @file bink_player.c
 *
 * 独立 Bink 解码器的最小 SDL2 播放器。
 *
 * 用法：
 *     bink_player <file.bik>
 *
 * 实时解码并播放 Bink 文件（视频 + 第一个音轨）。
 * 按 ESC 或关闭窗口退出。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bink_api.h"
#include "SDL.h"

int main(int argc, char **argv)
{
    BinkFile *f = NULL;
    BinkFrameResult fr;
    const BinkStreamInfo *vi, *ai;
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Texture *tex = NULL;
    SDL_AudioDeviceID adev = 0;
    uint8_t *rgb = NULL;
    int width = 0, height = 0;
    int audio_rate = 0, audio_ch = 0;
    int fps_num = 60, fps_den = 1;
    int frame_index = 0;
    int ret = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.bik>\n", argv[0]);
        return 1;
    }

    ret = bink_open(&f, argv[1]);
    if (ret < 0) {
        fprintf(stderr, "bink_open('%s') failed (error %d)\n", argv[1], ret);
        return 1;
    }

    vi = bink_stream(f, 0);
    if (vi) {
        width   = vi->width;
        height  = vi->height;
        fps_num = vi->fps_num;
        fps_den = vi->fps_den;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        goto out;
    }

    win = SDL_CreateWindow("Bink player", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, width, height,
                           SDL_WINDOW_RESIZABLE);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto out;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren)
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24,
                            SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!tex) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        goto out;
    }
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

    /* bink_frame_to_rgb 会写入整个帧（height 行），因此缓冲区必须
       容纳完整的 RGB24 图像，而不仅仅是单行 */
    rgb = (uint8_t *)malloc((size_t)height * (size_t)width * 3);
    if (!rgb) {
        fprintf(stderr, "out of memory\n");
        goto out;
    }

    /* 音频：打开第一个音轨，排队缓冲 float32 PCM */
    if (bink_stream_count(f) > 1) {
        SDL_AudioSpec want, have;
        ai = bink_stream(f, 1);
        audio_rate = ai->sample_rate;
        audio_ch   = ai->channels;

        SDL_zero(want);
        want.freq     = audio_rate;
        want.format   = AUDIO_F32SYS; /* 解码器输出 float32 */
        want.channels = (Uint8)audio_ch;
        want.samples  = 4096;
        adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (adev)
            SDL_PauseAudioDevice(adev, 0);
        else
            fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    }

    while ((ret = bink_decode_frame(f, &fr)) > 0) {
        SDL_Event e;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                goto done;
            if (e.type == SDL_KEYDOWN &&
                e.key.keysym.sym == SDLK_ESCAPE)
                goto done;
        }

        /* 将本帧的音频入队 */
        if (adev && fr.nb_audio > 0 && fr.audio_nb_samples[0] > 0) {
            SDL_QueueAudio(adev, fr.audio[0],
                           (Uint32)(fr.audio_nb_samples[0] *
                                    audio_ch * (int)sizeof(float)));
        }

        /* 显示视频帧 */
        if (fr.planes[0]) {
            bink_frame_to_rgb(&fr, rgb, width * 3);
            SDL_UpdateTexture(tex, NULL, rgb, width * 3);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        }

        frame_index++;

        /* 节奏控制：按帧率（实时）控制播放节奏，因此即使没有音频设备也能正常播放。
           音频队列只是限制已排队 PCM 可以超前运行的距离
           （不强制等待，避免死锁）。 */
        {
            static Uint32 next_tick;
            Uint32 frame_ms = (Uint32)(1000.0 * fps_den / fps_num);
            Uint32 now = SDL_GetTicks();
            if (next_tick == 0)
                next_tick = now;
            next_tick += frame_ms;
            if (next_tick > now)
                SDL_Delay(next_tick - now);
            if (adev) {
                Uint32 maxq = (Uint32)((Uint64)audio_rate * audio_ch *
                                       (Uint32)sizeof(float) / 2);
                if (SDL_GetQueuedAudioSize(adev) > maxq)
                    SDL_Delay(2);
            }
        }
    }

done:
    if (adev)
        SDL_PauseAudioDevice(adev, 1);
    if (ret < 0)
        fprintf(stderr, "decode error %d after %d frames\n", ret, frame_index);
    fprintf(stderr, "done: %d frames decoded\n", frame_index);

out:
    free(rgb);
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    if (adev) SDL_CloseAudioDevice(adev);
    SDL_Quit();
    bink_close(&f);
    return ret < 0 ? 1 : 0;
}
