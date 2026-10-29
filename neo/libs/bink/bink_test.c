/*
 * Bink decoder - standalone C port from FFmpeg
 *
 * This file is part of the standalone Bink decoder project (LGPL 2.1+).
 * See bink_common.h for the full license header.
 */

/**
 * @file bink_test.c
 *
 * 独立 Bink 解码器的示例 / 测试程序。
 *
 * 用法：
 *     bink_test <file.bik> [out_prefix]
 *
 * 打印流信息，解码每一帧，将每个解码后的视频帧写入 RGB PPM 图像
 * （prefix_00000.ppm，...），并将第一个音轨的解码音频写入 16 位 WAV 文件
 * （prefix_audio.wav）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bink_api.h"

/* 写入一张 PPM（P6，RGB24）图像 */
static int write_ppm(const char *path, const BinkFrameResult *r)
{
    FILE *f = fopen(path, "wb");
    uint8_t *rgb;
    int y, x;

    if (!f)
        return -1;
    fprintf(f, "P6\n%d %d\n255\n", r->width, r->height);
    fflush(f);

    rgb = (uint8_t *)malloc((size_t)r->width * 3);
    if (!rgb) {
        fclose(f);
        return -1;
    }
    for (y = 0; y < r->height; y++) {
        /* 在 rgb 中构建一行像素，然后写入文件 */
        for (x = 0; x < r->width; x++) {
            int yy = r->planes[0][(size_t)y * r->strides[0] + x] - 16;
            int cb = r->planes[1][(size_t)(y >> 1) * r->strides[1] + (x >> 1)] - 128;
            int cr = r->planes[2][(size_t)(y >> 1) * r->strides[2] + (x >> 1)] - 128;
            int rr = (298 * yy + 409 * cr + 128) >> 8;
            int gg = (298 * yy - 100 * cb - 208 * cr + 128) >> 8;
            int bb = (298 * yy + 516 * cb + 128) >> 8;
            if (rr < 0) rr = 0; else if (rr > 255) rr = 255;
            if (gg < 0) gg = 0; else if (gg > 255) gg = 255;
            if (bb < 0) bb = 0; else if (bb > 255) bb = 255;
            rgb[(size_t)x * 3 + 0] = (uint8_t)rr;
            rgb[(size_t)x * 3 + 1] = (uint8_t)gg;
            rgb[(size_t)x * 3 + 2] = (uint8_t)bb;
        }
        fwrite(rgb, 1, (size_t)r->width * 3, f);
    }
    free(rgb);
    fclose(f);
    return 0;
}

/* 将解码音频（音轨 0）作为 16 位 PCM WAV 写入 */
static int write_wav(const char *path, int sample_rate, int channels,
                     const float *data, int nb_samples)
{
    FILE *f = fopen(path, "wb");
    uint32_t data_bytes = (uint32_t)((size_t)nb_samples * channels * 2);
    uint32_t riff_size = 36 + data_bytes;
    uint16_t u16;
    uint32_t u32;
    int i, c;
    int16_t *pcm;
    size_t n;

    if (!f)
        return -1;

    /* RIFF/WAVE 文件头（全部为小端序） */
    fwrite("RIFF", 1, 4, f);
    fwrite(&riff_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    u32 = 16;                 fwrite(&u32, 4, 1, f); /* fmt 块大小 */
    u16 = 1;                  fwrite(&u16, 2, 1, f); /* PCM 格式 */
    u16 = (uint16_t)channels; fwrite(&u16, 2, 1, f); /* 声道数 */
    u32 = (uint32_t)sample_rate; fwrite(&u32, 4, 1, f);
    u32 = (uint32_t)sample_rate * (uint16_t)channels * 2;
                              fwrite(&u32, 4, 1, f); /* 字节率 */
    u16 = (uint16_t)(channels * 2); fwrite(&u16, 2, 1, f); /* 块对齐 */
    u16 = 16;                 fwrite(&u16, 2, 1, f); /* 每个样本的位数 */
    fwrite("data", 1, 4, f);
    fwrite(&data_bytes, 4, 1, f);

    pcm = (int16_t *)malloc((size_t)nb_samples * channels * 2);
    if (!pcm) {
        fclose(f);
        return -1;
    }
    n = 0;
    for (i = 0; i < nb_samples; i++) {
        for (c = 0; c < channels; c++) {
            float v = data[(size_t)i * channels + c];
            if (v < -1.0f) v = -1.0f; else if (v > 1.0f) v = 1.0f;
            pcm[n++] = (int16_t)(v * 32767.0f);
        }
    }
    fwrite(pcm, 2, (size_t)nb_samples * channels, f);
    free(pcm);
    fclose(f);
    return 0;
}

static const char *codec_str(uint32_t tag)
{
    static char s[5];
    s[0] = (char)(tag & 0xFF);
    s[1] = (char)((tag >> 8) & 0xFF);
    s[2] = (char)((tag >> 16) & 0xFF);
    s[3] = (char)((tag >> 24) & 0xFF);
    s[4] = 0;
    return s;
}

int main(int argc, char **argv)
{
    BinkFile *f = NULL;
    BinkFrameResult fr;
    const BinkStreamInfo *vi, *ai;
    int ret, frames = 0;
    const char *prefix = "out";
    char path[512];
    float *all_audio = NULL;
    int all_audio_len = 0, all_audio_cap = 0;
    int audio_rate = 0, audio_ch = 0;
    int have_audio = 0;

    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s <file.bik> [out_prefix]\n", argv[0]);
        return 1;
    }
    if (argc >= 3)
        prefix = argv[2];

    ret = bink_open(&f, argv[1]);
    if (ret < 0) {
        fprintf(stderr, "bink_open('%s') failed (error %d)\n", argv[1], ret);
        return 1;
    }

    vi = bink_stream(f, 0);
    if (vi)
        printf("Video : codec=%s %dx%d, fps=%d/%d, frames=%d%s\n",
               codec_str(vi->codec_tag), vi->width, vi->height,
               vi->fps_num, vi->fps_den, vi->nb_frames,
               bink_has_video_decoder(f) ? "" : " (no video decoder)");

    for (int i = 1; i < bink_stream_count(f); i++) {
        ai = bink_stream(f, i);
        if (ai)
            printf("Audio[%d]: %d Hz, %d ch, %s\n", i - 1, ai->sample_rate,
                   ai->channels, ai->use_dct ? "DCT" : "RDFT");
    }

    while ((ret = bink_decode_frame(f, &fr)) > 0) {
        if (bink_has_video_decoder(f) && fr.planes[0]) {
            if (frames < 100 || (frames % 100) == 0) {
                snprintf(path, sizeof(path), "%s_%05d.ppm", prefix, frames);
                write_ppm(path, &fr);
            }
            frames++;
        }

        if (fr.nb_audio > 0 && fr.audio_nb_samples[0] > 0) {
            int need = all_audio_len + fr.audio_nb_samples[0];
            if (need > all_audio_cap) {
                int nc = all_audio_cap ? all_audio_cap * 2 : 1 << 16;
                float *nb;
                while (nc < need)
                    nc *= 2;
                nb = (float *)realloc(all_audio, (size_t)nc *
                                       fr.audio_channels[0] * sizeof(float));
                if (!nb)
                    break;
                all_audio = nb;
                all_audio_cap = nc;
            }
            memcpy(all_audio + (size_t)all_audio_len * fr.audio_channels[0],
                   fr.audio[0],
                   (size_t)fr.audio_nb_samples[0] * fr.audio_channels[0] *
                       sizeof(float));
            all_audio_len += fr.audio_nb_samples[0];
            audio_rate = fr.audio_sample_rate[0];
            audio_ch   = fr.audio_channels[0];
            have_audio = 1;
        }
    }

    if (ret < 0) {
        fprintf(stderr, "decode error %d after %d frames\n", ret, frames);
        bink_close(&f);
        free(all_audio);
        return 1;
    }

    printf("Decoded %d video frames%s\n", frames,
           have_audio ? "" : " (no audio)");

    if (have_audio) {
        double secs = (double)all_audio_len / audio_rate;
        snprintf(path, sizeof(path), "%s_audio.wav", prefix);
        write_wav(path, audio_rate, audio_ch, all_audio, all_audio_len);
        printf("Audio : wrote %d samples (~%.2f s) -> %s\n",
               all_audio_len, secs, path);
    }

    free(all_audio);
    bink_close(&f);
    return 0;
}
