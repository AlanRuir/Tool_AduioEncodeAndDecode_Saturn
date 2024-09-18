#include <audio_encoder_aac.h>

AudioEncoderAAC::AudioEncoderAAC(int64_t bitrate, int sample_rate, int channels)
    : bytes_per_sample_(0)
    , sample_rate_(sample_rate)
    , channels_(channels)
    , counter_(0U)
    , codec_(nullptr)
    , codec_context_(nullptr)
    , frame_(nullptr)
    , pkt_(nullptr)
    , swr_ctx_(nullptr)
    , adts_header_(new uint8_t[7U](), std::default_delete<uint8_t[]>())
    , callback_(nullptr)
{
#if LIBAVCODEC_VERSION_MAJOR < 58 // 若 libavcodec < 58则注册所有编码器，否则只注册aac
    avcodec_register_all();
#endif

    codec_ = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec_)
    {
        std::cerr << "Codec not found" << std::endl;
        return;
    }

    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_)
    {
        std::cerr << "Could not allocate audio codec context" << std::endl;
        return;
    }

    const enum AVSampleFormat* p = codec_->sample_fmts;
    std::cout << "AAC Supported sample formats: ";
    while (*p != AV_SAMPLE_FMT_NONE)
    {
        std::cout << av_get_sample_fmt_name(*p) << " ";
        p++;
    }
    std::cout << std::endl;

    codec_context_->bit_rate       = bitrate;
    codec_context_->sample_fmt     = AV_SAMPLE_FMT_FLTP; // AAC编码器支持的格式, 平面浮点格式
    codec_context_->sample_rate    = sample_rate_;
    codec_context_->channel_layout = (2 == channels_) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
    codec_context_->channels       = channels_;

    if (avcodec_open2(codec_context_, codec_, nullptr) < 0)
    {
        throw std::runtime_error("Could not open codec");
    }

    frame_                 = av_frame_alloc();
    frame_->nb_samples     = codec_context_->frame_size;
    frame_->format         = AV_SAMPLE_FMT_FLTP;
    frame_->channel_layout = codec_context_->channel_layout;
    frame_->sample_rate    = codec_context_->sample_rate;

    if (av_frame_get_buffer(frame_, 0) < 0)
    {
        throw std::runtime_error("Could not allocate audio frame buffer");
    }

    AVPacket* pkt = av_packet_alloc();

    if (av_frame_make_writable(frame_) < 0)
    {
        throw std::runtime_error("Could not make audio frame writable");
    }

    swr_ctx_ = swr_alloc_set_opts(nullptr, codec_context_->channel_layout, AV_SAMPLE_FMT_FLTP, sample_rate_,
                                  codec_context_->channel_layout, AV_SAMPLE_FMT_FLT, sample_rate_, 0, nullptr);

    if (!swr_ctx_ || swr_init(swr_ctx_) < 0)
    {
        throw std::runtime_error("Could not initialize audio resampler");
    }

    bytes_per_sample_ = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT);

    pkt_ = av_packet_alloc();
}

AudioEncoderAAC::~AudioEncoderAAC()
{
    av_packet_free(&pkt_);
    av_frame_free(&frame_);
    avcodec_free_context(&codec_context_);
    swr_free(&swr_ctx_); // 释放SwrContext
}

bool AudioEncoderAAC::Encode(uint8_t* data, size_t size)
{
    int read_samples = size / (bytes_per_sample_ * channels_);
    if (read_samples < frame_->nb_samples)
    {
        std::cerr << "Warning: Incomplete frame read, adjusting nb_samples" << std::endl;
        frame_->nb_samples = read_samples;
    }

    // 将交错浮点PCM转换为平面浮点PCM
    const uint8_t* input_data[1]  = {data};
    uint8_t*       output_data[2] = {frame_->data[0], frame_->data[1]};
    swr_convert(swr_ctx_, output_data, frame_->nb_samples, input_data, frame_->nb_samples);

    frame_->pts = counter_ * frame_->nb_samples;
    ++counter_;

    if (avcodec_send_frame(codec_context_, frame_) < 0)
    {
        std::cerr << "Error sending the frame to the encoder" << std::endl;
        return false;
    }

    while (avcodec_receive_packet(codec_context_, pkt_) == 0)
    {
        UpdateAdtsHeader(codec_context_, pkt_->size + 7);

        if (callback_)
        {
            callback_(adts_header_.get(), 7U, pkt_->data, pkt_->size);
        }
        av_packet_unref(pkt_);
    }

    return true;
}

bool AudioEncoderAAC::InstallCallback(AACAudioEncoderCallbackType callback)
{
    if (!callback)
    {
        return false;
    }

    callback_ = callback;

    return true;
}

void AudioEncoderAAC::UpdateAdtsHeader(AVCodecContext* codec_context, int aac_length)
{
    int sampling_frequency_index = 4; // 默认44.1kHz
    int channel_config           = codec_context->channels;

    adts_header_.get()[0] = 0xFF;
    adts_header_.get()[1] = 0xF9;
    adts_header_.get()[2] =
        ((codec_context->profile - 1) << 6) + (sampling_frequency_index << 2) + (channel_config >> 2);
    adts_header_.get()[3] = ((channel_config & 3) << 6) + (aac_length >> 11);
    adts_header_.get()[4] = (aac_length & 0x7FF) >> 3;
    adts_header_.get()[5] = ((aac_length & 7) << 5) + 0x1F;
    adts_header_.get()[6] = 0xFC;
}