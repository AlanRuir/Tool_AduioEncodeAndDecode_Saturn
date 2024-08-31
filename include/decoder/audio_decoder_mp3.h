#ifndef __AUDIO_DECODER_MP3_H__
#define __AUDIO_DECODER_MP3_H__

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

class AudioDecoderMP3
{
private:
    using MP3AudioDecoderCallbackType = std::function<void(uint8_t*, uint32_t)>;
public:
    AudioDecoderMP3();
    ~AudioDecoderMP3();
    bool Decode(uint8_t* data, size_t size);
    bool InstallCallback(MP3AudioDecoderCallbackType callback);

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
    MP3AudioDecoderCallbackType callback_;
};

#endif  // __AUDIO_DECODER_MP3_H__