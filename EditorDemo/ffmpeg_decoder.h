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
	AVCodecID video_codec_id;
	AVStream* video_stream;
	AVCodecContext* video_avctx;
	const AVCodec* video_codec;
	AVPixelFormat chroma_format;
	int width, height, bit_depth, bpp, chroma_height;

	int audio_stream_index;
	AVCodecID audio_codec_id;
	AVStream* audio_stream;
	AVCodecContext* audio_avctx;
	const AVCodec* audio_codec;


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
		video_codec_id = video_stream->codecpar->codec_id;
		width = video_stream->codecpar->width;
		height = video_stream->codecpar->height;
		chroma_format = (AVPixelFormat)video_stream->codecpar->format;
		AVRational r_time_base = video_stream->time_base;
		time_base = av_q2d(r_time_base);

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
		
		if (0 != DecoderOpen(video_stream)) {
			return;
		}

		if (audio_stream_index >= 0) {
			audio_stream = fmtc->streams[audio_stream_index];
			audio_codec_id = audio_stream->codecpar->codec_id;

			if (0 != DecoderOpen(audio_stream)) {
				return;
			}
		}
	}

	int DecoderOpen(AVStream* stream)
	{
		AVCodecContext* temp_avctx;
		const AVCodec* temp_codec;

		temp_avctx = avcodec_alloc_context3(NULL);
		if (!temp_avctx) {
			LOG(ERROR) << "avcodec_alloc_context3 failed" << AVERROR(ENOMEM);
			return AVERROR(ENOMEM);
		}
		int ret = avcodec_parameters_to_context(temp_avctx, stream->codecpar);
		if (ret < 0) {
			LOG(ERROR) << "avcodec_alloc_context3 failed" << AVERROR(ret);
			return ret;
		}
		temp_avctx->pkt_timebase = stream->time_base;

		temp_codec = avcodec_find_decoder(temp_avctx->codec_id);
		if ((ret = avcodec_open2(temp_avctx, temp_codec, nullptr)) < 0) {
			LOG(ERROR) << "avcodec_open2 failed" << AVERROR(ret);
			return ret;
		}

		switch (temp_avctx->codec_type)
		{
		case AVMEDIA_TYPE_VIDEO:
			video_avctx = temp_avctx;
			video_codec = temp_codec;
			break;
		case AVMEDIA_TYPE_AUDIO:
			audio_avctx = temp_avctx;
			audio_codec = temp_codec;
			break;
		case AVMEDIA_TYPE_SUBTITLE:
			break;
		default:
			break;
		}

		return 0;
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

