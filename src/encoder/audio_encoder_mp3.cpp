#include "audio_encoder_mp3.h"

AudioEncoderMP3::AudioEncoderMP3(int64_t bitrate, int sample_rate, int channels)
    : bytes_per_sample_(AV_SAMPLE_FMT_S16P) // MP3 编码器通常使用 16 位 PCM
    , sample_rate_(sample_rate)
    , channels_(channels)
    , counter_(0)
    , codec_(nullptr)
    , codec_context_(nullptr)
    , frame_(nullptr)
    , pkt_(nullptr)
    , swr_ctx_(nullptr)
{
#if LIBAVCODEC_VERSION_MAJOR < 58           // 若 libavcodec < 58则注册所有编码器，否则只注册aac
    avcodec_register_all();
#endif

    codec_ = avcodec_find_encoder(AV_CODEC_ID_MP3);
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

    const enum AVSampleFormat *p = codec_->sample_fmts;
    std::cout << "MP3 Supported sample formats: ";
    while (*p != AV_SAMPLE_FMT_NONE) 
    {
        std::cout << av_get_sample_fmt_name(*p) << " ";
        p++;
    }
    std::cout << std::endl;

    codec_context_->bit_rate = bitrate;
    codec_context_->sample_fmt = AV_SAMPLE_FMT_S16P; // 设置为 16 位 PCM
    codec_context_->sample_rate = sample_rate;
    codec_context_->channel_layout = (2 == channels_) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
    codec_context_->channels = channels;

    // 打开编码器
    if (avcodec_open2(codec_context_, codec_, nullptr) < 0) 
    {
        throw std::runtime_error("Could not open codec");
    }

    // 分配帧
    frame_ = av_frame_alloc();
    frame_->format = codec_context_->sample_fmt;
    frame_->channel_layout = codec_context_->channel_layout;
    frame_->sample_rate = codec_context_->sample_rate;
    frame_->nb_samples = 1152; // MP3 编码器通常使用 1152 样本的帧

    if (av_frame_get_buffer(frame_, 0) < 0) 
    {
        throw std::runtime_error("Could not allocate audio frame buffer");
    }

    pkt_ = av_packet_alloc();

    swr_ctx_ = swr_alloc_set_opts(
        swr_ctx_,
        codec_context_->channel_layout, AV_SAMPLE_FMT_S16P, sample_rate,
        codec_context_->channel_layout, AV_SAMPLE_FMT_FLT, sample_rate,
        0, nullptr);

    if (!swr_ctx_ || swr_init(swr_ctx_) < 0) 
    {
        throw std::runtime_error("Could not initialize audio resampler");
    }
}

AudioEncoderMP3::~AudioEncoderMP3()
{
    av_packet_free(&pkt_);
    av_frame_free(&frame_);
    avcodec_free_context(&codec_context_);
    if (swr_ctx_) 
    {
        swr_free(&swr_ctx_);
    }
}

bool AudioEncoderMP3::Encode(uint8_t* data, size_t size)
{
    // 检查数据大小是否足够填充一个帧
    size_t samples = size / (av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT) * channels_);
    if (samples < frame_->nb_samples) 
    {
        std::cerr << "Warning: Incomplete frame read, adjusting nb_samples" << std::endl;
        frame_->nb_samples = samples;
    }

    // 将输入数据转换为 MP3 编码器所需的格式
    uint8_t** input_data = reinterpret_cast<uint8_t**>(malloc(sizeof(uint8_t*) * channels_));
    for (int i = 0; i < channels_; ++i) 
    {
        input_data[i] = data + i * size / channels_;
    }

    uint8_t* output_data[channels_];
    for (int i = 0; i < channels_; ++i) 
    {
        output_data[i] = frame_->data[i];
    }

    // 重采样和格式转换
    int resampled_count = swr_convert(swr_ctx_, output_data, frame_->nb_samples, const_cast<const uint8_t**>(input_data), frame_->nb_samples);
    if (resampled_count < 0) 
    {
        std::cerr << "Error in resampling" << std::endl;
        free(input_data);
        return false;
    }

    frame_->pts = counter_ * 1152; // 1152 samples per frame
    counter_++;

    // 将帧发送到编码器
    if (avcodec_send_frame(codec_context_, frame_) < 0) 
    {
        std::cerr << "Error sending the frame to the encoder" << std::endl;
        free(input_data);
        return false;
    }

    // 接收编码后的数据包
    while (avcodec_receive_packet(codec_context_, pkt_) == 0) 
    {
        if (callback_)
        {
            callback_(pkt_->data, pkt_->size);
        }
        av_packet_unref(pkt_);
    }

    free(input_data);
    return true;
}

void AudioEncoderMP3::WriteMp3Header(FILE* output_file, AVCodecContext *codec_context, int mp3_length)
{
    // MP3 文件通常不需要像 AAC 的 ADTS 头那样的元数据头
    // 但是可以添加 ID3 标签或其他元数据
}

bool AudioEncoderMP3::InstallCallback(MP3AudioEncoderCallbackType callback)
{
    if (!callback)
    {
        return false;
    }

    callback_ = callback;

    return true;
}