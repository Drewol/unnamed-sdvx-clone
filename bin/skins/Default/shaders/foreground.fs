#version 330
#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 texVp;
layout(location=0) out vec4 target;

uniform ivec2 screenCenter;
// x = bar time
// y = object glow
// z = real time since song start
uniform vec3 timing;
uniform ivec2 viewport;
uniform float objectGlow;
// fg_texture.png
uniform sampler2D mainTex;
// current FrameBuffer
uniform sampler2D fb_tex;
uniform float tilt;
uniform float clearTransition;
uniform float framesCount;

#define PI 3.14159265359
#define TWO_PI 6.28318530718

float portrait(float a, float b)
{
	float resu;
	if (viewport.y > viewport.x) {
	    resu = a;
	}
	else {
	    resu = b;
	}
	
	return resu;
}

vec3 blendMultiply(vec3 base, vec3 blend) {
	return base*blend;
}
vec3 blendMultiply(vec3 base, vec3 blend, float opacity) {
	return (blendMultiply(base, blend) * opacity + base * (1.0 - opacity));
}

vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float Speed = 1.0;

void main()
{
    float ar = float(viewport.x) / float(viewport.y);

    vec2 screenUV = vec2(texVp.x / viewport.x, texVp.y / viewport.y);

    vec2 centerUV = screenUV;
	centerUV.y -= portrait(0.15,-0.35);
	centerUV.y /= ar;
	centerUV = clamp(centerUV,0.0,1.0);

    float frameFraction = 1. / framesCount;

    vec2 texCoords = centerUV;
    texCoords *= vec2(frameFraction, .5);

    // float currentFrame = floor(mod(timing.z * 20 * Speed, framesCount));
    float currentFrame = floor(timing.y * framesCount);
    vec2 offset = vec2(currentFrame*frameFraction, 0.);
	
	vec3 bgcol = texture(mainTex, texCoords + offset).rgb;

    target.rgb = bgcol;
    vec3 hsv = rgb2hsv(target.rgb);

    target.a = clamp(hsv.z * 2., 0.,1.);
}
