#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <functional>

#include <audio_encoder_aac.h>
#include <audio_encoder_mp3.h>
#include <audio_decoder_aac.h>
#include <audio_decoder_mp3.h>

void SaveAAC(uint8_t* header, uint32_t header_size, uint8_t* data, uint32_t data_size)
{
    FILE* file = fopen("output.aac", "ab+");
    if (!file) 
    {
        std::cerr << "Failed to open output file" << std::endl;
        return;
    }

    fwrite(header, 1, header_size, file);
    fwrite(data, 1, data_size, file);
    fclose(file);
}

void SaveMP3(uint8_t* data, uint32_t data_size)
{
    FILE* file = fopen("output.mp3", "ab+");
    if (!file) 
    {
        std::cerr << "Failed to open output file" << std::endl;
        return;
    }

    fwrite(data, 1, data_size, file);
    fclose(file);
}

void SavePCM(std::string filename, uint8_t* data, uint32_t data_size)
{
    FILE* file = fopen(filename.c_str(), "ab+");
    if (!file) 
    {
        std::cerr << "Failed to open output file" << std::endl;
        return;
    }

    fwrite(data, 1, data_size, file);
    fclose(file);
}

int main(int argc, char* argv[])
{
    size_t buffer_size = 1024 * 4 * 2;       // 每帧1024个样本，每个样本2个通道，每个通道一个float，一个float4个字节，一共 1024 * 4 * 2 个字节

    std::shared_ptr<AudioEncoderAAC> aac_encoder = std::make_shared<AudioEncoderAAC>(80000, 44100, 2);
    aac_encoder->InstallCallback(SaveAAC);
    std::shared_ptr<AudioEncoderMP3>  mp3_encoder = std::make_shared<AudioEncoderMP3>(320000, 44100, 2);
    mp3_encoder->InstallCallback(SaveMP3);
    std::shared_ptr<AudioDecoderAAC>  aac_decoder = std::make_shared<AudioDecoderAAC>();
    std::function<void(uint8_t*, uint32_t)> aac_decoder_callback = std::bind(SavePCM, std::string("aac_f32le_ar44100_ac2.pcm"), std::placeholders::_1, std::placeholders::_2);
    aac_decoder->InstallCallback(aac_decoder_callback);
    std::shared_ptr<AudioDecoderMP3>  mp3_decoder = std::make_shared<AudioDecoderMP3>();
    std::function<void(uint8_t*, uint32_t)> mp3_decoder_callback = std::bind(SavePCM, std::string("mp3_f32le_ar44100_ac2.pcm"), std::placeholders::_1, std::placeholders::_2);
    mp3_decoder->InstallCallback(mp3_decoder_callback);
    std::shared_ptr<uint8_t> data(new uint8_t[buffer_size](), std::default_delete<uint8_t[]>());


    std::ifstream pcm_file("f32le_ar44100_ac2.pcm", std::ios::binary);
    if (!pcm_file) 
    {
        std::cerr << "Could not open input file" << std::endl;
        return -1;
    }

    while (pcm_file.read(reinterpret_cast<char*>(data.get()), buffer_size)) 
    {
        aac_encoder->Encode(data.get(), buffer_size);
        mp3_encoder->Encode(data.get(), buffer_size);
    }

    std::ifstream aac_file("output.aac", std::ios::binary);
    if (!aac_file) 
    {
        std::cerr << "Failed to open output.aac for reading" << std::endl;
        return -1;
    }

    while (aac_file) 
    {
        // 读取帧头（ADTS header）
        uint8_t adtsHeader[7];
        aac_file.read(reinterpret_cast<char*>(adtsHeader), sizeof(adtsHeader));
        if (aac_file.gcount() < sizeof(adtsHeader)) break;

        // 解析帧长度（ADTS header的第3到第5字节包含帧长度信息）
        int frameLength = ((adtsHeader[3] & 0x03) << 11) |
                          (adtsHeader[4] << 3) |
                          ((adtsHeader[5] & 0xE0) >> 5);

        // 减去ADTS头的长度，得到AAC数据的长度
        frameLength -= sizeof(adtsHeader);

        // 读取AAC帧数据
        std::vector<uint8_t> frameData(frameLength);
        aac_file.read(reinterpret_cast<char*>(frameData.data()), frameLength);
        if (aac_file.gcount() < frameLength) break;

        // 解码当前AAC帧
        if (!aac_decoder->Decode(frameData.data(), frameLength)) 
        {
            std::cerr << "Failed to decode AAC frame" << std::endl;
            return -1;
        }
    }


    std::ifstream mp3_file("output.mp3", std::ios::binary);
    if (!mp3_file) 
    {
        std::cerr << "Failed to open output.mp3 for reading" << std::endl;
        return -1;
    }

    std::vector<uint8_t> buffer(4096);

    while (mp3_file) 
    {
        // 读取MP3帧头部（前4个字节用于检测帧同步字）
        mp3_file.read(reinterpret_cast<char*>(buffer.data()), 4);
        if (mp3_file.gcount() < 4) break;

        // 检查帧同步字 (11个连续的1)
        if ((buffer[0] == 0xFF) && ((buffer[1] & 0xE0) == 0xE0)) 
        {
            // 计算帧比特率
            int bitrate_index = (buffer[2] >> 4) & 0x0F;
            int bitrate = 0;
            static const int bitrates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
            if (bitrate_index > 0 && bitrate_index < 15) 
            {
                bitrate = bitrates[bitrate_index] * 1000; // 比特率的单位是kbps
            } 
            else 
            {
                std::cerr << "Invalid bitrate index: " << bitrate_index << std::endl;
                break;
            }

            // 计算采样率
            int sample_rate_index = (buffer[2] >> 2) & 0x03;
            int sample_rate = 0;
            static const int sample_rates[] = {44100, 48000, 32000, 0};
            if (sample_rate_index < 3) 
            {
                sample_rate = sample_rates[sample_rate_index];
            } 
            else 
            {
                std::cerr << "Invalid sample rate index: " << sample_rate_index << std::endl;
                break;
            }

            // 计算帧长度
            int padding = (buffer[2] >> 1) & 0x01;
            int frameLength = (144 * bitrate / sample_rate) + padding;

            // 检查帧长度是否有效
            if (frameLength < 7 || frameLength > 1441) 
            {
                std::cerr << "Invalid frame length: " << frameLength << std::endl;
                break;
            }

            // 读取完整的MP3帧
            buffer.resize(frameLength);
            mp3_file.read(reinterpret_cast<char*>(buffer.data() + 4), frameLength - 4);
            if (mp3_file.gcount() < frameLength - 4) break;

            // 送入解码器
            std::cout << "Frame length: " << frameLength << std::endl;
            if (!mp3_decoder->Decode(buffer.data(), frameLength)) 
            {
                std::cerr << "Failed to decode MP3 frame" << std::endl;
                return -1;
            }
        } 
        else 
        {
            // 如果未找到帧同步字，则跳过一个字节继续查找
            mp3_file.seekg(-3, std::ios::cur);
        }
    }

    return 0;
}