#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "Utils.h"


class FFmpegDecoder
{
private:
	AVFormatContext* fmtc = nullptr;
	AVPacket* pkt = nullptr;

	//video
	int video_stream_index;
	AVCodecID video_codec_id;
	AVStream* video_stream = nullptr;
	AVCodecContext* video_avctx = nullptr;
	const AVCodec* video_codec = nullptr;
	AVPixelFormat chroma_format;
	int width, height, bit_depth, bpp, chroma_height;

	//audio
	int audio_stream_index;
	AVCodecID audio_codec_id;
	AVStream* audio_stream = nullptr;
	AVCodecContext* audio_avctx = nullptr;
	const AVCodec* audio_codec = nullptr;


	double time_base = 0.0;
	int64_t user_time_scale = 1000;
private:

	AVFormatContext* CreateFormatContext(const char* file_path) {
		AVFormatContext* ctx = nullptr;
		avformat_open_input(&ctx, file_path, nullptr, nullptr);
		return ctx;
	}
	FFmpegDecoder(AVFormatContext* fmtc);

	int DecoderOpen(AVStream* stream);

public:
	FFmpegDecoder(const char* szFilePath) : FFmpegDecoder(CreateFormatContext(szFilePath)) {}

	~FFmpegDecoder() {

		if (!fmtc) {
			return;
		}

		if (pkt) {
			av_packet_free(&pkt);
		}


		if (video_avctx) {
			avcodec_free_context(&video_avctx);
		}
		if (audio_avctx) {
			avcodec_free_context(&audio_avctx);
		}

		avformat_close_input(&fmtc);
	}

	AVCodecID GetVideoCodec() {
		return video_codec_id;
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

