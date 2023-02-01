#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "Utils.h"


class FFmpegDecoder
{
private:
	AVFormatContext* fmtc = NULL;
	AVPacket* pkt = NULL;
	int video_stream_index;
	AVCodecID video_codec;
	AVStream* video_stream;
	AVPixelFormat chroma_format;
	int width, height, bit_depth, bpp, chroma_height;

	int audio_stream_index;
	AVCodecID audio_codec;
	AVStream* audio_stream;

	double time_base = 0.0;
	int64_t user_time_scale = 1000;
private:

	AVFormatContext* CreateFormatContext(const char* file_path) {
		AVFormatContext* ctx = NULL;
		avformat_open_input(&ctx, file_path, NULL, NULL);
		return ctx;
	}
	FFmpegDecoder(AVFormatContext* fmtc) : fmtc(fmtc) {
		if (!fmtc) {
			LOG(ERROR) << "No AVFormatContext provided.";
			return;
		}

		pkt = av_packet_alloc();
		if (!pkt) {
			LOG(ERROR) << "AVPacket allocation failed";
			return;
		}

		LOG(INFO) << "Media format: " << fmtc->iformat->long_name << " (" << fmtc->iformat->name << ")";
		avformat_find_stream_info(fmtc, NULL);
		video_stream_index = av_find_best_stream(fmtc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
		audio_stream_index = av_find_best_stream(fmtc, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
		if (video_stream_index < 0) {
			LOG(ERROR) << "FFmpeg error: " << __FILE__ << " " << __LINE__ << " " << "Could not find stream in input file";
			av_packet_free(&pkt);
			return;
		}

		video_stream = fmtc->streams[video_stream_index];
		video_codec = fmtc->streams[video_stream_index]->codecpar->codec_id;
		width = fmtc->streams[video_stream_index]->codecpar->width;
		height = fmtc->streams[video_stream_index]->codecpar->height;
		chroma_format = (AVPixelFormat)fmtc->streams[video_stream_index]->codecpar->format;
		AVRational r_time_base = fmtc->streams[video_stream_index]->time_base;
		time_base = av_q2d(r_time_base);
		if (audio_stream_index >= 0) {
			audio_stream = fmtc->streams[audio_stream_index];
			audio_codec = fmtc->streams[audio_stream_index]->codecpar->codec_id;
		}

		// Set bit depth, chroma height, bits per pixel based on eChromaFormat of input
		switch (chroma_format)
		{
		case AV_PIX_FMT_YUV420P10LE:
		case AV_PIX_FMT_GRAY10LE:   // monochrome is treated as 420 with chroma filled with 0x0
			bit_depth = 10;
			chroma_height = (height + 1) >> 1;
			bpp = 2;
			break;
		case AV_PIX_FMT_YUV420P12LE:
			bit_depth = 12;
			chroma_height = (height + 1) >> 1;
			bpp = 2;
			break;
		case AV_PIX_FMT_YUV444P10LE:
			bit_depth = 10;
			chroma_height = height << 1;
			bpp = 2;
			break;
		case AV_PIX_FMT_YUV444P12LE:
			bit_depth = 12;
			chroma_height = height << 1;
			bpp = 2;
			break;
		case AV_PIX_FMT_YUV444P:
			bit_depth = 8;
			chroma_height = height << 1;
			bpp = 1;
			break;
		case AV_PIX_FMT_YUV420P:
		case AV_PIX_FMT_YUVJ420P:
		case AV_PIX_FMT_YUVJ422P:   // jpeg decoder output is subsampled to NV12 for 422/444 so treat it as 420
		case AV_PIX_FMT_YUVJ444P:   // jpeg decoder output is subsampled to NV12 for 422/444 so treat it as 420
		case AV_PIX_FMT_GRAY8:      // monochrome is treated as 420 with chroma filled with 0x0
			bit_depth = 8;
			chroma_height = (height + 1) >> 1;
			bpp = 1;
			break;
		default:
			LOG(WARNING) << "ChromaFormat not recognized. Assuming 420";
			chroma_format = AV_PIX_FMT_YUV420P;
			bit_depth = 8;
			chroma_height = (height + 1) >> 1;
			bpp = 1;
		}


	}

public:
	FFmpegDecoder(const char* szFilePath) : FFmpegDecoder(CreateFormatContext(szFilePath)) {}

	~FFmpegDecoder() {

		if (!fmtc) {
			return;
		}

		if (pkt) {
			av_packet_free(&pkt);
		}

		avformat_close_input(&fmtc);

	}

	AVCodecID GetVideoCodec() {
		return video_codec;
	}
	AVPixelFormat GetChromaFormat() {
		return chroma_format;
	}
	int GetWidth() {
		return width;
	}
	int GetHeight() {
		return height;
	}
	int GetBitDepth() {
		return bit_depth;
	}
	int GetFrameSize() {
		return width * (height + chroma_height) * bpp;
	}
};

