#ifndef __AUDIO_ENCODER_AAC_H__
#define __AUDIO_ENCODER_AAC_H__

#include <iostream>
#include <fstream>
#include <vector>
#include <functional>
#include <memory>

extern "C" 
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

class AudioEncoderAAC
{
private:
    using AACAudioEncoderCallbackType = std::function<void(uint8_t*, uint32_t, uint8_t*, uint32_t)>;

public:
    AudioEncoderAAC(int64_t bitrate, int sample_rate, int channels);
    ~AudioEncoderAAC();
    bool Encode(uint8_t* data, size_t size);
    bool InstallCallback(AACAudioEncoderCallbackType callback);

private:
    void UpdateAdtsHeader(AVCodecContext *codec_context, int aac_length);

private:
    int                      bytes_per_sample_;
    int                      sample_rate_;
    int                      channels_;
    size_t                   counter_;
    AVCodec*                 codec_;
    AVCodecContext*          codec_context_;
    AVFrame*                 frame_;
    AVPacket*                pkt_;
    SwrContext*              swr_ctx_;
    std::shared_ptr<uint8_t> adts_header_;
    AACAudioEncoderCallbackType callback_;
};

#endif  // __AUDIO_ENCODER_AAC_H__