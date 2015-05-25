/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                shilin lin - Guangzhou GEC Corporation
//                For Open Source Computer Vision Library
//
// Copyright (C) 2015, GEC Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include "head4audio.h"

// 根据本系统的具体字节序处理的存放格式
#if   __BYTE_ORDER == __LITTLE_ENDIAN

	#define RIFF ('F'<<24 | 'F'<<16 | 'I'<<8 | 'R'<<0)
	#define WAVE ('E'<<24 | 'V'<<16 | 'A'<<8 | 'W'<<0)
	#define FMT  (' '<<24 | 't'<<16 | 'm'<<8 | 'f'<<0)
	#define DATA ('a'<<24 | 't'<<16 | 'a'<<8 | 'd'<<0)

	#define LE_SHORT(val) (val) 
	#define LE_INT(val)   (val) 

#elif __BYTE_ORDER == __BIG_ENDIAN

	#define RIFF ('R'<<24 | 'I'<<16 | 'F'<<8 | 'F'<<0)
	#define WAVE ('W'<<24 | 'A'<<16 | 'V'<<8 | 'E'<<0)
	#define FMT  ('f'<<24 | 'm'<<16 | 't'<<8 | ' '<<0)
	#define DATA ('d'<<24 | 'a'<<16 | 't'<<8 | 'a'<<0)

	#define LE_SHORT(val) bswap_16(val) 
	#define LE_INT(val)   bswap_32(val) 

#endif

wav_format *wav = NULL;
int fd;
// #define DURATION_TIME 3

// 准备WAV格式参数
void prepare_wav_params(wav_format *wav)
{
	wav->format.fmt_id = FMT;
	wav->format.fmt_size = LE_INT(16);
	wav->format.fmt = LE_SHORT(WAV_FMT_PCM);
	wav->format.channels = LE_SHORT(2);         // 声道数目
	wav->format.sample_rate = LE_INT(44100);    // 采样频率
	wav->format.bits_per_sample = LE_SHORT(16); // 量化位数
	wav->format.block_align = LE_SHORT(wav->format.channels
				* wav->format.bits_per_sample/8);
	wav->format.byte_rate = LE_INT(wav->format.sample_rate
				* wav->format.block_align);
	wav->data.data_id = DATA;
	// wav->data.data_size = LE_INT(DURATION_TIME
				// * wav->format.byte_rate);
	wav->head.id = RIFF;
	wav->head.format = WAVE;
	// wav->head.size = LE_INT(36 + wav->data.data_size);
}

// 设置WAV格式参数
void set_wav_params(pcm_container *sound, wav_format *wav)
{
	// 1：定义并分配一个硬件参数空间
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_hw_params_alloca(&hwparams);

	// 2：初始化硬件参数空间
	snd_pcm_hw_params_any(sound->handle, hwparams);

	// 3：设置访问模式为交错模式（即帧连续模式）
	snd_pcm_hw_params_set_access(sound->handle, hwparams,
			 SND_PCM_ACCESS_RW_INTERLEAVED);

	// 4：设置量化参数
	snd_pcm_format_t pcm_format = SND_PCM_FORMAT_S16_LE;
	snd_pcm_hw_params_set_format(sound->handle,
					hwparams, pcm_format);
	sound->format = pcm_format;

	// 5：设置声道数目
	snd_pcm_hw_params_set_channels(sound->handle,
		hwparams, LE_SHORT(wav->format.channels));
	sound->channels = LE_SHORT(wav->format.channels);

	// 6：设置采样频率
	// 注意：最终被设置的频率被存放在来exact_rate中
	uint32_t exact_rate = LE_INT(wav->format.sample_rate);
	snd_pcm_hw_params_set_rate_near(sound->handle,
				hwparams, &exact_rate, 0);

	// 7：设置buffer size为声卡支持的最大值
	snd_pcm_uframes_t buffer_size;
	snd_pcm_hw_params_get_buffer_size_max(hwparams,
					&buffer_size);
	snd_pcm_hw_params_set_buffer_size_near(sound->handle,
				hwparams, &buffer_size);

	// 8：根据buffer size设置period size
	snd_pcm_uframes_t period_size = buffer_size / 4;
	snd_pcm_hw_params_set_period_size_near(sound->handle,
				hwparams, &period_size, 0);

	// 9：安装这些PCM设备参数
	snd_pcm_hw_params(sound->handle, hwparams);

	// 10：获取buffer size和period size
	// 注意：他们均以 frame 为单位 （frame = 声道数 * 量化级）
	snd_pcm_hw_params_get_buffer_size(hwparams,
				&sound->frames_per_buffer);
	snd_pcm_hw_params_get_period_size(hwparams,
				&sound->frames_per_period, 0);

	// 11：保存一些参数
	sound->bits_per_sample =
		snd_pcm_format_physical_width(pcm_format);
	sound->bytes_per_frame =
		sound->bits_per_sample/8 * wav->format.channels;

	// 12：分配一个周期数据空间
	sound->period_buf =
		(uint8_t *)calloc(1,
		sound->frames_per_period * sound->bytes_per_frame);
}

snd_pcm_uframes_t read_pcm_data(pcm_container *sound, snd_pcm_uframes_t frames)
{
	snd_pcm_uframes_t exact_frames = 0;
	snd_pcm_uframes_t n = 0;

	uint8_t *p = sound->period_buf;
	while(frames > 0)	
	{
		n = snd_pcm_readi(sound->handle, p, frames);

		frames -= n;
		exact_frames += n;
		p += (n * sound->bytes_per_frame);
	}

	return exact_frames;
}

uint32_t total_bytes = 0;

// 从PCM设备录取音频数据，并写入fd中
void recorder(int fd, pcm_container *sound, wav_format *wav)
{
	// 1：写WAV格式的文件头
	// write(fd, &wav->head, sizeof(wav->head));
	// write(fd, &wav->format, sizeof(wav->format));
	// write(fd, &wav->data, sizeof(wav->data));

	lseek(fd, sizeof(wav), SEEK_SET);

	// 2：写PCM数据
	// uint32_t total_bytes = wav->data.data_size;
	uint32_t nwrite = 0;

	// while(total_bytes > 0)
	while(1)
	{
		uint32_t total_frames = total_bytes / (sound->bytes_per_frame);
		// snd_pcm_uframes_t n = MIN(total_frames, sound->frames_per_period);
		snd_pcm_uframes_t n = sound->frames_per_period;

		uint32_t frames_read = read_pcm_data(sound, n);
		nwrite = write(fd, sound->period_buf, frames_read * sound->bytes_per_frame);
		// total_bytes -= (frames_read * sound->bytes_per_frame);
		total_bytes += nwrite;
		printf("%d\n", total_bytes);
	}
}

void stop(int sig)
{
	lseek(fd, 0, SEEK_SET);

	wav->data.data_size = LE_INT(total_bytes);
	wav->head.size = LE_INT(36 + wav->data.data_size);

	write(fd, &wav->head, sizeof(wav->head));
	write(fd, &wav->format, sizeof(wav->format));
	write(fd, &wav->data, sizeof(wav->data));

	printf("%u bytes recorded.\n", total_bytes);
	exit(1);
}

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		printf("Usage: %s <wav-file>\n", argv[0]);
		exit(1);
	}

	signal(SIGINT, stop);

	// 1：打开WAV格式文件
	// int fd = open(argv[1], O_CREAT|O_WRONLY|O_TRUNC, 0777);
	fd = open(argv[1], O_CREAT|O_WRONLY|O_TRUNC, 0777);
	if(fd == -1)	
	{
		perror("open() error");
		exit(1);
	}

	// 2: 打开PCM设备文件
	pcm_container *sound = calloc(1, sizeof(pcm_container));
	int ret = snd_pcm_open(&sound->handle, "default",
				SND_PCM_STREAM_CAPTURE, 0);
	if(ret != 0)
	{
		printf("[%d]: %d\n", __LINE__, ret);
		perror("snd_pcm_open( ) failed");
		exit(1);
	}

	// 3: 准备并设置WAV格式参数
	wav = calloc(1, sizeof(wav_format));
	// wav_format *wav = calloc(1, sizeof(wav_format));
	prepare_wav_params(wav);
	set_wav_params(sound, wav);

	// 4: 开始从PCM设备录制音频数据
	//    并且以WAV格式写到fd中
	recorder(fd, sound, wav);

	// 5: 释放相关资源
	snd_pcm_drain(sound->handle);
	snd_pcm_close(sound->handle);
	close(fd);

	free(sound->period_buf);
	free(sound);
	free(wav);

	return 0;
}
