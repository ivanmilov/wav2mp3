#include <filesystem>
#include <fstream>
#include <iostream>
#include <lame/lame.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

constexpr std::string_view WAV_EXT{".wav"};
constexpr std::string_view MP3_EXT{".mp3"};
constexpr uint32_t BUF_LEN_4K{4096};
constexpr int FLUSH_BUF_MIN_SIZE{7200};

std::queue<fs::path> files_queue;
std::mutex queue_mutex;

// https://aneescraftsmanship.com/wp-content/ql-cache/quicklatex.com-f7fdf5d082e246f9b6227fae758dc043_l3.png
struct WavHeader
{
    // RIFF Chunk
    uint8_t chunk_id[4];      // RIFF
    uint32_t chunk_data_size; // RIFF Chunk data Size
    uint8_t riff_type_id[4];  // WAVE
    // format sub-chunk
    uint8_t chunk1_id[4];      // fmt
    uint32_t chunk1_data_size; // Size of the format chunk
    uint16_t format_tag;       //  format_Tag 1=PCM
    uint16_t num_channels;     //  1=Mono 2=Sterio
    uint32_t sample_rate;      // Sampling Frequency in (44100)Hz
    uint32_t byte_rate;        // Byte rate
    uint16_t block_align;      // 4
    uint16_t bits_per_sample;  // 16
    /* "data" sub-chunk */
    uint8_t chunk2_id[4];      // data
    uint32_t chunk2_data_size; // Size of the audio data
};

void encode(const fs::path& wavpath)
{
    lame_t lame = lame_init();
    if (lame == NULL)
    {
        std::cerr << "Error in lame_init." << std::endl;
        return;
    }

    std::ifstream wav(wavpath, std::ios_base::binary);

    // read header
    WavHeader header;
    // NOTE: consider big/little endian encoding, current solution won't work on all platforms.
    if (not wav.read(reinterpret_cast<char*>(&header), sizeof(WavHeader)))
    {
        std::cerr << "Error reading header of " << wavpath << std::endl;
        return;
    }

    const MPEG_mode mode = header.num_channels == 1 ? MPEG_mode::MONO : MPEG_mode::STEREO;

    // override settings
    lame_set_num_channels(lame, header.num_channels);
    lame_set_in_samplerate(lame, header.sample_rate);
    lame_set_mode(lame, mode);
    lame_set_quality(lame, 3); // good quality

    if (lame_init_params(lame) < 0)
    {
        std::cerr << "Error init params for file " << wavpath << std::endl;
        return;
    }

    uint32_t byte_per_sample = header.block_align / header.num_channels;

    uint32_t num_samples = BUF_LEN_4K / byte_per_sample;

    // prepare buffers
    std::vector<int16_t> wavbuf;
    wavbuf.resize(num_samples * header.num_channels / 2);

    std::vector<uint8_t> mp3buf;
    // worst case estimate: mp3buf_size in bytes = 1.25*num_samples + 7200
    mp3buf.resize(1.25 * num_samples + 7200);

    auto mp3path = wavpath;
    std::ofstream mp3(mp3path.replace_extension(MP3_EXT), std::ios_base::binary);

    auto write = [&mp3buf, &mp3](int count) { mp3.write(reinterpret_cast<const char*>(mp3buf.data()), count); };

    while (wav.peek(), wav.good())
    {
        wav.read(reinterpret_cast<char*>(wavbuf.data()), wavbuf.size() * 2);
        auto read_count = wav.gcount() / (2 * header.num_channels);

        int write_count{0};
        if (header.num_channels == 1)
        {
            write_count = lame_encode_buffer(lame, wavbuf.data(), nullptr, read_count, mp3buf.data(), mp3buf.size());
        }
        else
        {
            write_count = lame_encode_buffer_interleaved(lame, wavbuf.data(), read_count, mp3buf.data(), mp3buf.size());
        }

        if (write_count > 0)
        {
            write(write_count);
        }
    }

    mp3buf.resize(FLUSH_BUF_MIN_SIZE);
    auto n = lame_encode_flush(lame, mp3buf.data(), mp3buf.size());
    if (n > 0)
    {
        write(n);
    }

    wav.close();
    mp3.close();
    lame_close(lame);
    std::cout << wavpath << " -> " << mp3path << std::endl;
}

void worker()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (files_queue.empty())
        {
            return;
        }

        auto file = files_queue.front();
        files_queue.pop();
        lock.unlock();

        encode(file);
    }
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " path_to_wav_folder\n";
        return 1;
    }

    const std::string_view path{argv[1]};

    std::cout << "To be processed:\n";
    for (const auto& file : fs::directory_iterator(path))
    {
        if (file.path().extension() != WAV_EXT)
            continue;

        std::cout << file.path() << std::endl;
        files_queue.push(file.path());
    }

    std::cout << "Processed:\n";
    std::vector<std::thread> threads;
    for (unsigned int i{0}; i < std::thread::hardware_concurrency(); ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto& t : threads)
    {
        t.join();
    }
    std::cout << "Done\n";

    return 0;
}
