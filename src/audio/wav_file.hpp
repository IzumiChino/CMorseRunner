#pragma once
#include "snd_types.hpp"
#include <string>
#include <fstream>
#include <cstdint>

class WavFile {
public:
    WavFile() = default;
    ~WavFile() { if (isOpen()) close(); }

    bool openWrite(const std::string& path, int sampleRate = DEFAULTRATE);
    void writeFrom(const float* data, int count);
    void close();
    bool isOpen() const { return file_.is_open(); }

private:
    std::ofstream file_;
    int           sampleRate_{DEFAULTRATE};
    uint32_t      sampleCount_{0};

    void writeHeader();
    void patchHeader();
};
