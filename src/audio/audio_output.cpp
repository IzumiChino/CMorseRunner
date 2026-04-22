#include "audio_output.hpp"
#include <cstring>
#include <algorithm>
#include <stdexcept>

AudioOutput::AudioOutput() {
    Pa_Initialize();
}

AudioOutput::~AudioOutput() {
    stop();
    Pa_Terminate();
}

bool AudioOutput::start(int sampleRate, int bufSize, AudioCallback cb) {
    stop();
    callback_ = std::move(cb);
    bufSize_  = bufSize;

    PaError err = Pa_OpenDefaultStream(
        &stream_,
        0, 1,                    // no input, 1 output channel (mono)
        paFloat32,
        sampleRate,
        static_cast<unsigned long>(bufSize),
        &AudioOutput::paCallback,
        this);

    if (err != paNoError) {
        stream_ = nullptr;
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }
    return true;
}

void AudioOutput::stop() {
    if (!stream_) return;
    Pa_StopStream(stream_);
    Pa_CloseStream(stream_);
    stream_ = nullptr;
}

int AudioOutput::paCallback(const void*, void* out,
                             unsigned long frames,
                             const PaStreamCallbackTimeInfo*,
                             PaStreamCallbackFlags,
                             void* userData)
{
    auto* self = static_cast<AudioOutput*>(userData);
    auto* outBuf = static_cast<float*>(out);

    TSingleArray data = self->callback_();

    int n = static_cast<int>(std::min(
        static_cast<unsigned long>(data.size()), frames));

    for (int i = 0; i < n; ++i)
        outBuf[i] = data[i] / 32767.0f;   // normalize to [-1, 1]
    for (unsigned long i = n; i < frames; ++i)
        outBuf[i] = 0.0f;

    return paContinue;
}
