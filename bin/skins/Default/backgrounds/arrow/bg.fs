#version 330
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
uniform sampler2D mainTexClear;
uniform vec2 tilt;
uniform float clearTransition;

#define pi 3.1415926535897932384626433832795

vec2 rotate_point(vec2 cen,float angle,vec2 p)
{
  float s = sin(angle);
  float c = cos(angle);

  // translate point back to origin:
  p.x -= cen.x;
  p.y -= cen.y;

  // rotate point
  float xnew = p.x * c - p.y * s;
  float ynew = p.x * s + p.y * c;

  // translate point back:
  p.x = xnew + cen.x;
  p.y = ynew + cen.y;
  return p;
}

vec3 hsv2rgb(vec3 c) {
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}


void main()
{
    float ar = float(viewport.x) / viewport.y;
    vec2 center = vec2(screenCenter);
	vec2 uv = texVp.xy;

	float rot = dot(tilt, vec2(0.5, 1.0));
    uv = rotate_point(center, rot * 2.0 * pi, uv);
    float thing = 1.8 / abs(center.x - uv.x);
    float thing2 = abs(center.x - uv.x) * 2.0;
    uv.y -= center.y;
    uv.y *=  thing;
    uv.y = (uv.y + 1.0) / 2.0;
    uv.x *= thing / 3.0;
    uv.x += timing.y * 1.0;

    vec4 col = texture(mainTex, uv) * 0.75;
    vec4 clear_col = texture(mainTexClear, uv);

    col *= (1.0 - clearTransition);
    col += clear_col * clearTransition * 1.3;

    if (abs(uv.y) > 1.0 || uv.y < 0.0)
        col = vec4(0);
    col.a *= 1.0 - (thing * 70.0);

	target = col;

}
