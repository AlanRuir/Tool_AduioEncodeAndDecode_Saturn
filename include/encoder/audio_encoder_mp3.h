#ifndef  __AUDIO_ENCODER_MP3_H__
#define  __AUDIO_ENCODER_MP3_H__

#include <iostream>
#include <fstream>
#include <vector>
#include <functional>

extern "C" 
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

class AudioEncoderMP3
{
private:
    using MP3AudioEncoderCallbackType = std::function<void(uint8_t *, uint32_t)>;

public:
    AudioEncoderMP3(int64_t bitrate, int sample_rate, int channels);
    ~AudioEncoderMP3();
    bool Encode(uint8_t* data, size_t size);
    bool InstallCallback(MP3AudioEncoderCallbackType callback);

private:
    void WriteMp3Header(FILE* output_file, AVCodecContext *codec_context, int mp3_length);

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
    MP3AudioEncoderCallbackType callback_;
};

#endif  // __AUDIO_ENCODER_MP3_H__