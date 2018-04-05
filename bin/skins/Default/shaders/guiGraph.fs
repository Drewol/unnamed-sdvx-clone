#version 330
#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;

uniform sampler2D graphTex;
uniform ivec2 viewport;
uniform vec4 color;
uniform vec4 upperColor;
uniform vec4 lowerColor;
uniform float colorBorder;


void main()
{
	target = vec4(1.0f);
    float xStep = 1. / viewport.x;
    vec3 col = vec3(0.0f);
    
    vec2 current = vec2(fsTex.x, texture(graphTex, vec2(fsTex.x, 0.5f)).x);
    vec2 next = vec2(fsTex.x + xStep, texture(graphTex, vec2(clamp(fsTex.x + xStep, 0.0f, 1.0f) , 0.5f)).x);
    vec2 avg = (current + next) / 2.0;
    float dist = abs(distance(vec2(fsTex.x,fsTex.y * -1 + 1.),avg));
    
    if (avg.y >= colorBorder)
        col = upperColor.xyz;
    else
        col = lowerColor.xyz;
    

    target.xyz = vec3(col);
    target.a = smoothstep(abs(next.y - current.y) + 0.010, abs(next.y - current.y),dist);
}