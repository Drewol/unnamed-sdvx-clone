#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 texVp;
layout(location=0) out vec4 target;

uniform ivec2 screenCenter;
// x = bar time
// y = off-sync but smooth bpm based timing
// z = real time since song start
uniform vec3 timing;
uniform ivec2 viewport;
uniform float objectGlow;
// bg_texture.png
uniform sampler2D mainTex;
uniform float tilt;
uniform float clearTransition;
uniform bool reverseColor;
uniform float reverseColorAmount;

vec4 applyReverseColor(vec4 col)
{
    float screenY = gl_FragCoord.y / float(viewport.y);
    float mask = reverseColor ? step(1.0 - reverseColorAmount, screenY) : 1.0 - step(reverseColorAmount, screenY);
    col.xyz = mix(col.xyz, vec3(1.0) - col.xyz, mask * col.a);
    return col;
}

void main()
{

    target = vec4(0.0);
    target = applyReverseColor(target);
}
