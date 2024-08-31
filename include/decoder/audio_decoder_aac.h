#ifndef __AUDIO_DECODER_AAC_H__
#define __AUDIO_DECODER_AAC_H__

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

class AudioDecoderAAC
{
private:
    using AACAudioDecoderCallbackType = std::function<void(uint8_t*, uint32_t)>;
public:
    AudioDecoderAAC();
    ~AudioDecoderAAC();
    bool Decode(uint8_t* data, size_t size);
    bool InstallCallback(std::function<void(uint8_t*, uint32_t)> callback);

private:
    int                         bytes_per_sample_;
    int                         sample_rate_;
    int                         channels_;
    size_t                      counter_;
    AVCodec*                    codec_;
    AVCodecContext*             codec_context_;
    AVFrame*                    frame_;
    AVPacket*                   pkt_;
    SwrContext*                 swr_ctx_;
    std::shared_ptr<uint8_t>    adts_header_;
    AACAudioDecoderCallbackType callback_;
};

#endif  // __AUDIO_DECODER_AAC_H__