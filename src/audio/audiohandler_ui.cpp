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

#include "../midpointslider.h"
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

    case FunctionGeneratorType::Sweep:
        return "Sweep";
    }

    return "";
}

std::string getStr(const StateWindowFilter& filter) noexcept
{
    switch (filter) {
    case StateWindowFilter::None:
        return "None";
    case StateWindowFilter::Hamming:
        return "Hamming";
    case StateWindowFilter::Blackman:
        return "Blackman";
    }

    return "";
}

void AudioHandler::update() noexcept
{
    ImGui::Begin("Audio Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PushItemWidth(-1.0F);
    if (!running) {
        ImGui::Text("Driver");
        std::vector<RtAudio::Api> rtAudioApis;
        RtAudio::getCompiledApi(rtAudioApis);
        if (ImGui::BeginCombo("##apiSelect", RtAudio::getApiDisplayName(rtAudio->getCurrentApi()).c_str())) {
            for (auto& api : rtAudioApis) {
                if (ImGui::Selectable(RtAudio::getApiDisplayName(api).c_str(), api == rtAudio->getCurrentApi())) {
                    rtAudio = std::make_unique<RtAudio>(api);
                    if (rtAudio == nullptr) {
                        rtAudio = std::make_unique<RtAudio>();
                        if (rtAudio == nullptr) {
                            SDL2WRAP_ASSERT(false);
                        }
                    }
                    config.captureDevice = rtAudio->getDeviceInfo(rtAudio->getDefaultInputDevice());
                    config.playbackDevice = rtAudio->getDeviceInfo(rtAudio->getDefaultOutputDevice());
                    config.playbackParams.deviceId = rtAudio->getDefaultOutputDevice();
                    config.captureParams.deviceId = rtAudio->getDefaultInputDevice();
                    break;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Capture Device");
        if (ImGui::BeginCombo("##CapDevice", config.captureDevice.name.c_str())) {
            for (unsigned int i = 0; i < rtAudio->getDeviceCount(); i++) {
                auto device = rtAudio->getDeviceInfo(i);
                if (device.inputChannels < 2) {
                    continue;
                }
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(device.name.c_str(), device.name == config.captureDevice.name)) {
                    config.captureDevice = device;
                    config.sampleRate = config.captureDevice.preferredSampleRate;
                    config.captureParams.deviceId = i;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::Text("Playback Device");
        if (ImGui::BeginCombo("##PbDevice", config.playbackDevice.name.c_str())) {
            for (unsigned int i = 0; i < rtAudio->getDeviceCount(); i++) {
                auto device = rtAudio->getDeviceInfo(i);
                if (device.outputChannels < 2) {
                    continue;
                }
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(device.name.c_str(), device.name == config.playbackDevice.name)) {
                    config.playbackDevice = device;
                    config.playbackParams.deviceId = i;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Sample Rate");
        if (ImGui::BeginCombo("##Sample Rate", std::to_string(config.sampleRate).c_str())) {
            for (auto rate : config.getLegalSampleRates()) {
                ImGui::PushID(static_cast<int>(rate));
                if (ImGui::Selectable(std::to_string(rate).c_str(), rate == config.sampleRate)) {
                    config.sampleRate = rate;
                    // make the generators have the right rate
                    sineGenerator.setSampleRate(config.sampleRate);
                    sweepGenerator.setSampleRate(config.sampleRate);
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }

        int iFirstPlayback = static_cast<int>(config.playbackParams.firstChannel);
        int iFirstCapture = static_cast<int>(config.captureParams.firstChannel);
        ImGui::Text("First Capture");
        ImGui::InputInt("##firstCap", &iFirstCapture, 1, 1);
        ImGui::Checkbox("Swap Reference", &config.inputAndReferenceAreSwapped);
        ImGui::Text("First Playback");
        ImGui::InputInt("##firstPlayback", &iFirstPlayback, 1, 1);
        config.playbackParams.firstChannel = std::clamp(static_cast<unsigned int>(iFirstPlayback), 0u, config.playbackDevice.outputChannels - 2);
        config.captureParams.firstChannel = std::clamp(static_cast<unsigned int>(iFirstCapture), 0u, config.captureDevice.inputChannels - 2);
    } else {
        ImGui::Text("Capture Device: %s", config.captureDevice.name.c_str());
        ImGui::Text("Playback Device: %s", config.playbackDevice.name.c_str());
        ImGui::Text("Sample Rate: %d", static_cast<int>(config.sampleRate));
        ImGui::Text("First Playback: %d", static_cast<int>(config.playbackParams.firstChannel));
        ImGui::Text("First Capture: %d", static_cast<int>(config.playbackParams.firstChannel));
        if (config.inputAndReferenceAreSwapped) {
            ImGui::Text("Input and Ref Swapped!");
        }
    }

    if (!running) {
        if (ImGui::Button("Start Audio")) {
            startAudio();
        }
    } else {
        if (ImGui::Button("Stop Audio")) {
            stopAudio();
            status = "Stopped";
        }
    }

    ImGui::Text("Status: %s", status.c_str());

    ImGui::Separator();

    ImGui::Text("Select Signal");
    if (ImGui::BeginCombo("##Select Signal", getStr(functionGeneratorType).c_str())) {
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
        if (ImGui::Selectable(getStr(FunctionGeneratorType::Sweep).c_str(), functionGeneratorType == FunctionGeneratorType::Sweep)) {
            functionGeneratorType = FunctionGeneratorType::Sweep;
        }
        ImGui::EndCombo();
    }

    if (functionGeneratorType == FunctionGeneratorType::Sine) {
        auto freq = static_cast<float>(sineGenerator.getFrequency());
        ImGui::Text("Frequency");
        if (ImGui::SliderFloat("##Frequency", &freq, 0.0F, 20000.0F, "%.0f", 1.0F)) {
            sineGenerator.setFrequency(static_cast<double>(freq));
        }
    }
    ImGui::Text("Output Volume");
    MidpointSlider("##volime", 0.0, 1.0, 0.5, config.outputVolume);

    ImGui::Separator();
    ImGui::Text("Analysis Length");
    if (ImGui::BeginCombo("##Analysis Length", config.sampleCountToString(config.analysisSamples).c_str())) {
        for (auto&& rate : config.getPossibleAnalysisSampleRates()) {
            ImGui::PushID(static_cast<int>(rate));
            if (ImGui::Selectable(config.sampleCountToString(rate).c_str(), rate == config.analysisSamples)) {
                config.analysisSamples = rate;
                sweepGenerator.setLength(static_cast<double>(config.analysisSamples) / config.sampleRate);
                resetStates();
            }
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }
    ImGui::Text("Window Filter");
    if (ImGui::BeginCombo("##Window Config", getStr(stateFilterConfig.windowFilter).c_str())) {
        if (ImGui::Selectable("None", stateFilterConfig.windowFilter == StateWindowFilter::None)) {
            stateFilterConfig.windowFilter = StateWindowFilter::None;
        }
        if (ImGui::Selectable("Hamming", stateFilterConfig.windowFilter == StateWindowFilter::Hamming)) {
            stateFilterConfig.windowFilter = StateWindowFilter::Hamming;
        }
        if (ImGui::Selectable("Blackman", stateFilterConfig.windowFilter == StateWindowFilter::Blackman)) {
            stateFilterConfig.windowFilter = StateWindowFilter::Blackman;
        }
        ImGui::EndCombo();
    }
    ImGui::Text("FFT Averaging");
    auto iAvgCount = static_cast<int>(stateFilterConfig.avgCount);
    ImGui::InputInt("##avgCount", &iAvgCount, 1, 1);
    stateFilterConfig.avgCount = std::clamp(static_cast<size_t>(iAvgCount), static_cast<size_t>(0), LAA_MAX_FFT_AVG);
    if (ImGui::Button("Reset Avg")) {
        stateFilterConfig.clearAvg();
    }

    ImGui::PopItemWidth();
    ImGui::End();
}