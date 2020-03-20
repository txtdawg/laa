/*
 * This file is part of LAA
 * Copyright (c) 2020 Malte Kießling
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "audiohandler.h"

std::string getStr(const FunctionGeneratorType& gen) noexcept
{
    switch (gen) {
    case FunctionGeneratorType::Silence:
        return "Silence";

    case FunctionGeneratorType::WhiteNoise:
        return "White Noise";

    case FunctionGeneratorType::PinkNoise:
        return "Pink Noise";

    case FunctionGeneratorType::Sine:
        return "Sine";
    }

    return "";
}

AudioHandler::AudioHandler() noexcept
{
    if (s2::Audio::getNumDevices(false) > 0) {
        config.playbackName = s2::Audio::getDeviceName(0, false);
    }

    if (s2::Audio::getNumDevices(true) > 0) {
        config.captureName = s2::Audio::getDeviceName(0, true);
    }

    if (s2::Audio::getNumDrivers() > 0) {
        config.driver = s2::Audio::getDriver(0);
    }

    auto res = s2::Audio::init(config.driver.c_str());
    if (!res) {
        SDL2WRAP_ASSERT(false);
    }
    driverChosen = true;
}

AudioHandler::~AudioHandler() noexcept
{
    if (running) {
        s2::Audio::closeDevice(captureId);
        s2::Audio::closeDevice(playbackId);
    }
}

void AudioHandler::update() noexcept
{
    ImGui::Begin("Audio Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    if (!running) {
        if (ImGui::BeginCombo("Driver", config.driver.c_str())) {
            for (int i = 0; i < s2::Audio::getNumDrivers(); i++) {
                ImGui::PushID(i);
                if (ImGui::Selectable(s2::Audio::getDriver(i))) {
                    s2::Audio::quit();
                    config.driver = s2::Audio::getDriver(i);
                    auto res = s2::Audio::init(config.driver.c_str());
                    SDL2WRAP_ASSERT(res.hasValue());
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    }

    if (ImGui::BeginCombo("Playback Device", config.playbackName.c_str())) {
        for (int i = 0; i < s2::Audio::getNumDevices(false); i++) {
            std::string playbackName = s2::Audio::getDeviceName(i, false);
            ImGui::PushID(i);
            if (ImGui::Selectable(playbackName.c_str(), playbackName == config.playbackName)) {
                config.playbackName = playbackName;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginCombo("Capture Device", config.captureName.c_str())) {
        for (int i = 0; i < s2::Audio::getNumDevices(true); i++) {
            std::string captureName = s2::Audio::getDeviceName(i, true);
            ImGui::PushID(i);
            if (ImGui::Selectable(captureName.c_str(), captureName == config.captureName)) {
                config.captureName = captureName;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    ImGui::InputInt("Sample Rate", &config.sampleRate);
    if (!running) {
        if (ImGui::Button("Start Audio")) {
            startAudio();
        }
    } else {
        if (ImGui::Button("Stop Audio")) {
            status = "Stopping is not implemented yet";
        }
    }

    ImGui::Text("Status: %s", status.c_str());

    ImGui::Separator();
    if (ImGui::BeginCombo("Select Signal", getStr(functionGeneratorType).c_str())) {
        if (ImGui::Selectable(getStr(FunctionGeneratorType::Silence).c_str(), functionGeneratorType == FunctionGeneratorType::Silence)) {
            functionGeneratorType = FunctionGeneratorType::Silence;
        }
        if (ImGui::Selectable(getStr(FunctionGeneratorType::Sine).c_str(), functionGeneratorType == FunctionGeneratorType::Sine)) {
            functionGeneratorType = FunctionGeneratorType::Sine;
        }
        if (ImGui::Selectable(getStr(FunctionGeneratorType::WhiteNoise).c_str(), functionGeneratorType == FunctionGeneratorType::WhiteNoise)) {
            functionGeneratorType = FunctionGeneratorType::WhiteNoise;
        }
        if (ImGui::Selectable(getStr(FunctionGeneratorType::PinkNoise).c_str(), functionGeneratorType == FunctionGeneratorType::PinkNoise)) {
            functionGeneratorType = FunctionGeneratorType::PinkNoise;
        }

        ImGui::EndCombo();
    }

    if (functionGeneratorType == FunctionGeneratorType::Sine) {
        auto freq = static_cast<float>(sineGenerator.getFrequency());
        if (ImGui::SliderFloat("Frequency", &freq, 0.0F, 20000.0F, "%.0f", 1.0F)) {
            sineGenerator.setFrequency(static_cast<double>(freq));
        }
    }

    ImGui::End();
}

float AudioHandler::genNextPlaybackSample()
{
    switch (functionGeneratorType) {
    case FunctionGeneratorType::Silence:
        break;
    case FunctionGeneratorType::WhiteNoise:
        return static_cast<float>(WhiteNoiseGenerator::nextSample());
    case FunctionGeneratorType::PinkNoise:
        return static_cast<float>(pinkNoise.nextSample());
    case FunctionGeneratorType::Sine:
        return static_cast<float>(sineGenerator.nextSample());
        break;
    }

    return 0.0F;
}

void AudioHandler::startAudio()
{
    if (running) {
        s2::Audio::closeDevice(captureId);
        s2::Audio::closeDevice(playbackId);
    }

    running = false;

    wipReferenceSignal.reserve(config.samples);
    wipInputSignal.reserve(config.samples);

    s2::Audio::Spec want;
    SDL_zero(want);
    want.freq = config.sampleRate;
    want.format = AUDIO_F32;
    want.channels = 2;
    want.samples = config.samples;
    want.userdata = this;

    auto wantPlayback = want;
    wantPlayback.callback = playbackCallbackStatic;
    auto wantCapture = want;
    wantCapture.callback = captureCallbackStatic;

    s2::Audio::Spec gotPlayback;
    auto playbackRes = s2::Audio::openDevice(config.playbackName.c_str(), false, wantPlayback, gotPlayback, s2::AudioAllow::AnyChange);
    if (!playbackRes) {
        status = playbackRes.getError().msg;
        return;
    }
    playbackId = playbackRes.extractValue();
    s2::Audio::pauseDevice(playbackId, false);

    s2::Audio::Spec gotCapture;
    auto captureRes = s2::Audio::openDevice(config.playbackName.c_str(), true, wantCapture, gotCapture, s2::AudioAllow::AnyChange);
    if (!captureRes) {
        status = captureRes.getError().msg;
        s2::Audio::closeDevice(playbackId);
        return;
    }
    captureId = captureRes.extractValue();
    s2::Audio::pauseDevice(captureId, false);

    running = true;
    status = std::string("Running (") + s2::Audio::getCurrentDriver() + ")";
}

void AudioHandler::playbackCallback(Uint8* stream, int len)
{
    auto count = static_cast<size_t>(len) / sizeof(float);
    auto* floatPtr = reinterpret_cast<float*>(stream);
    for (auto i = 0ull; i + 1 < count; i += 2) {
        float f = genNextPlaybackSample();
        floatPtr[i] = f;
        floatPtr[i + 1] = f;
    }
}

void AudioHandler::captureCallback(Uint8* stream, int len)
{
    (void)stream;
    (void)len;
    auto count = static_cast<size_t>(len) / sizeof(float);
    auto* floatPtr = reinterpret_cast<float*>(stream);

    for (auto i = 0ull; i + 1 < count; i += 2) {
        wipReferenceSignal.push_back(floatPtr[i + config.referenceChannel]);
        wipInputSignal.push_back(floatPtr[i + config.inputChannel]);

        if (wipReferenceSignal.size() >= config.samples) {
            currentReferenceSignal = std::move(wipReferenceSignal);
            currentInputSignal = std::move(wipInputSignal);
            wipReferenceSignal.reserve(config.samples);
            wipInputSignal.reserve(config.samples);
            ++frameCount;
        }
    }
}

void AudioHandler::playbackCallbackStatic(void* userdata, Uint8* stream, int len)
{
    reinterpret_cast<AudioHandler*>(userdata)->playbackCallback(stream, len);
}

void AudioHandler::captureCallbackStatic(void* userdata, Uint8* stream, int len)
{
    reinterpret_cast<AudioHandler*>(userdata)->captureCallback(stream, len);
}

size_t AudioHandler::getFrameCount() const noexcept
{
    return frameCount;
}

void AudioHandler::getFrame(std::vector<double>& reference, std::vector<double>& input) const noexcept
{
    s2::Audio::lockDevice(captureId);
    reference = currentReferenceSignal;
    input = currentInputSignal;
    s2::Audio::unlockDevice(captureId);
}
