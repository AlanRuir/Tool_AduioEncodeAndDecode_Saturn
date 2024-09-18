#include "audio_decoder_mp3.h"

AudioDecoderMP3::AudioDecoderMP3()
    : codec_(nullptr)
    , codec_context_(nullptr)
    , frame_(nullptr)
    , pkt_(nullptr)
    , callback_(nullptr)
{
#if LIBAVCODEC_VERSION_MAJOR < 58
    avcodec_register_all();
#endif

    // 查找MP3解码器
    codec_ = avcodec_find_decoder(AV_CODEC_ID_MP3);
    if (!codec_)
    {
        std::cerr << "Codec not found" << std::endl;
        return;
    }

    // 分配解码器上下文
    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_)
    {
        std::cerr << "Could not allocate audio codec context" << std::endl;
        return;
    }

    if (avcodec_open2(codec_context_, codec_, nullptr) < 0)
    {
        throw std::runtime_error("Could not open codec");
    }

    frame_ = av_frame_alloc();
    if (!frame_)
    {
        throw std::runtime_error("Could not allocate audio frame");
    }

    pkt_ = av_packet_alloc();
    if (!pkt_)
    {
        throw std::runtime_error("Could not allocate packet");
    }
}

AudioDecoderMP3::~AudioDecoderMP3()
{
    av_packet_free(&pkt_);
    av_frame_free(&frame_);
    avcodec_free_context(&codec_context_);
}

bool AudioDecoderMP3::Decode(uint8_t* data, size_t size)
{
    pkt_->data = data;
    pkt_->size = size;

    if (avcodec_send_packet(codec_context_, pkt_) < 0)
    {
        std::cerr << "Error sending the packet to the decoder" << std::endl;
        return false;
    }

    while (avcodec_receive_frame(codec_context_, frame_) == 0)
    {
        if (callback_)
        {
            int buffer_size = av_samples_get_buffer_size(nullptr, codec_context_->channels, frame_->nb_samples,
                                                         codec_context_->sample_fmt, 1);

            // 如果是平面格式（如fltp），需要分别处理每个声道的数据
            std::vector<uint8_t> interleaved_buffer(buffer_size);

            // 将各声道的数据交织在一起
            for (int sample = 0; sample < frame_->nb_samples; ++sample)
            {
                for (int ch = 0; ch < codec_context_->channels; ++ch)
                {
                    float* src = reinterpret_cast<float*>(frame_->data[ch]);
                    float* dst = reinterpret_cast<float*>(interleaved_buffer.data());

                    dst[sample * codec_context_->channels + ch] = src[sample];
                }
            }

            // 将交织后的数据传递给回调函数
            callback_(interleaved_buffer.data(), buffer_size);
        }
    }

    av_packet_unref(pkt_);
    return true;
}

bool AudioDecoderMP3::InstallCallback(std::function<void(uint8_t*, uint32_t)> callback)
{
    if (!callback)
    {
        return false;
    }

    callback_ = callback;

    return true;
}
