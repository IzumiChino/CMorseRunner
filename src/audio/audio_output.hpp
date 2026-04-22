#pragma once
#include "snd_types.hpp"
#include <functional>
#include <portaudio.h>

// Callback type: called when the output buffer needs more data
using AudioCallback = std::function<TSingleArray()>;

class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();

    bool start(int sampleRate, int bufSize, AudioCallback cb);
    void stop();
    bool isRunning() const { return stream_ != nullptr; }

private:
    PaStream*     stream_{nullptr};
    AudioCallback callback_;
    int           bufSize_{512};

    static int paCallback(const void*, void* out,
                          unsigned long frames,
                          const PaStreamCallbackTimeInfo*,
                          PaStreamCallbackFlags,
                          void* userData);
};
