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

#ifndef laa_hamming_h
#define laa_hamming_h

#include <cmath>
#include <vector>

template <class T>
inline void hamming(std::vector<T>& x)
{
    auto M = static_cast<double>(x.size());
    for (size_t i = 0; i < x.size(); i++) {
        auto di = static_cast<double>(i);
        x[i] *= 0.54 - 0.46 * std::cos(2.0 * M_PI * di / M);
    }
}

#endif //laa_hamming_h
