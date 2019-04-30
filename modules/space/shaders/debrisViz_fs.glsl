/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2018                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include "fragment.glsl"
#include "floatoperations.glsl"

//layout(location = 0) in vec4 vertex; // 1: x, 2: y, 3: z, 4: time
// This doesn't work, plz help
//layout(location = 1) in vec2 orbit; // 1: epoch, 2: period

uniform vec3 color;
uniform float opacity = 1.0;

uniform float lineFade;
uniform float inGameTime;


in vec4 viewSpacePosition;
in vec4 vs_position;
//in float nrOfPeriods;
//in float offsetPeriods;
in float fade;
//in vec3 vs_color;
//in vec2 vs_texcoord;

Fragment getFragment() {
    
    float nrOfPeriods = (inGameTime - orbit.x) / orbit.y;
    float periodFraction = fract(nrOfPeriods);

    float offsetPeriods = vertex.w / orbit.y;
    float offsetFraction = offsetPeriods;

    // check difference of these two locations
    float vertexDistance = periodFraction - offsetFraction;

    if (vertexDistance < 0.0) {
        vertexDistance += 1.0;
    }

    float invert = 1.0 - vertexDistance;

    float fade = clamp(invert * lineFade, 0.0, 1.0);
    

    Fragment frag;
    frag.color = vec4(color, fade * opacity);
    frag.depth = vs_position.w;
    frag.gPosition = viewSpacePosition;
    frag.gNormal = vec4(1, 1, 1 , 0);
    return frag;
}



