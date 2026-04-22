#include "wav_file.hpp"
#include <cstring>
#include <algorithm>

// Write a little-endian value
template<typename T>
static void writeLE(std::ofstream& f, T v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof(T));
}

bool WavFile::openWrite(const std::string& path, int sampleRate) {
    sampleRate_  = sampleRate;
    sampleCount_ = 0;
    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_) return false;
    writeHeader();
    return true;
}

void WavFile::writeHeader() {
    // RIFF header placeholder (will be patched on close)
    file_.write("RIFF", 4);
    writeLE<uint32_t>(file_, 0);          // chunk size (patched later)
    file_.write("WAVE", 4);
    file_.write("fmt ", 4);
    writeLE<uint32_t>(file_, 16);         // PCM fmt chunk size
    writeLE<uint16_t>(file_, 1);          // PCM format
    writeLE<uint16_t>(file_, 1);          // mono
    writeLE<uint32_t>(file_, static_cast<uint32_t>(sampleRate_));
    writeLE<uint32_t>(file_, static_cast<uint32_t>(sampleRate_ * 2)); // byte rate
    writeLE<uint16_t>(file_, 2);          // block align
    writeLE<uint16_t>(file_, 16);         // bits per sample
    file_.write("data", 4);
    writeLE<uint32_t>(file_, 0);          // data size (patched later)
}

void WavFile::writeFrom(const float* data, int count) {
    if (!file_) return;
    for (int i = 0; i < count; ++i) {
        int16_t s = static_cast<int16_t>(
            std::clamp(static_cast<int>(data[i]), -32767, 32767));
        writeLE<int16_t>(file_, s);
    }
    sampleCount_ += static_cast<uint32_t>(count);
}

void WavFile::patchHeader() {
    uint32_t dataBytes = sampleCount_ * 2;
    // patch data chunk size at offset 40
    file_.seekp(40);
    writeLE<uint32_t>(file_, dataBytes);
    // patch RIFF chunk size at offset 4
    file_.seekp(4);
    writeLE<uint32_t>(file_, 36 + dataBytes);
}

void WavFile::close() {
    if (!file_) return;
    patchHeader();
    file_.close();
    sampleCount_ = 0;
}
