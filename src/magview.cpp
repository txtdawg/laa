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

#include "magview.h"
#include "dsp/windows.h"
#include "midpointslider.h"

void MagView::update(StateManager& stateManager, std::string idHint)
{
    ImGui::BeginChild((idHint + "Mag").c_str());

    const auto& liveState = stateManager.getLive();
    auto& data = choose(smoothing, liveState.smoothedAvgMag, liveState.avgMag);

    auto size = ImGui::GetWindowContentRegionMax();
    PlotConfig plotConfig;
    plotConfig.size = ImVec2(size.x * 0.98F, size.y - 75.0F);
    plotConfig.yAxisConfig.min = 0.000001;
    plotConfig.yAxisConfig.max = 1.0;
    plotConfig.yAxisConfig.enableLogScale = true;
    plotConfig.yAxisConfig.gridInterval = 1.0;
    plotConfig.yAxisConfig.gridHint = 1.0;

    plotConfig.label = "Mag";

    plotConfig.xAxisConfig.min = min;
    plotConfig.xAxisConfig.max = max;
    plotConfig.xAxisConfig.enableLogScale = true;
    plotConfig.xAxisConfig.gridInterval = 0.5;
    plotConfig.xAxisConfig.gridHint = 1000.0;

    BeginPlot(plotConfig);

    if (liveState.visible) {
        PlotSourceConfig sourceConfig;
        sourceConfig.count = liveState.fftLen / 2;
        sourceConfig.xMin = 0.0;
        sourceConfig.xMax = liveState.sampleRate / 2.0;
        sourceConfig.color = liveState.uniqueCol;
        sourceConfig.active = liveState.active;
        Plot(
            sourceConfig,
            [&data](size_t idx) {
                if (idx >= data.size()) {
                    return 0.0;
                }

                return data[idx];
            });
    }

    for (auto& state : stateManager.getSaved()) {
        if (!state.visible) {
            continue;
        }
        auto& savedData = choose(smoothing, state.smoothedAvgMag, state.avgMag);
        PlotSourceConfig sourceConfig;
        sourceConfig.count = state.fftLen / 2;
        sourceConfig.xMin = 0.0;
        sourceConfig.xMax = state.sampleRate / 2.0;
        sourceConfig.color = state.uniqueCol;
        sourceConfig.active = state.active;
        Plot(
            sourceConfig, [&savedData](size_t idx) {
                if (idx >= savedData.size()) {
                    return 0.0;
                }
                return savedData[idx];
            });
    }

    EndPlot();
    ImGui::PushItemWidth(plotConfig.size.x / 3.0F);
    MidpointSlider("min##freq", 30.0, 20000.0, 1000.0, min);
    ImGui::SameLine();
    min = std::clamp(min, 30.0, 20000.0);
    MidpointSlider("max##freq", 30.0, 20000.0, 1000.0, max);
    max = std::clamp(max, min, 20000.0);
    ImGui::PopItemWidth();
    ImGui::Checkbox("Enable Smoothing", &smoothing);
    ImGui::EndChild();
}
