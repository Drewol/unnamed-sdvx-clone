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
// bg_texture.png
uniform sampler2D mainTex;
uniform float tilt;
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

    uv = rotate_point(center, tilt * 2.0 * pi, uv);
    float thing = 1.8 / abs(center.x - uv.x);
    float thing2 = abs(center.x - uv.x) * 2.0;
    uv.y -= center.y;
    uv.y *=  thing;
    uv.y = (uv.y + 1.0) / 2.0;
    uv.x *= thing / 3.0;
    uv.x += timing.x * 2.0;
	
    vec4 col = texture2D(mainTex, uv);
    float hsvVal = (col.x + col.y + col.z) / 3.0;
    vec3 clear_col = hsv2rgb(vec3(0.5 * uv.x + 0.1, 1.0, hsvVal));
    
    col.xyz *= (1.0 - clearTransition);
    col.xyz += clear_col * clearTransition * 2;
    
    if (abs(uv.y) > 1.0 || uv.y < 0.0)
        col = vec4(0);
    col.a *= 1.0 - (thing * 70.0);
    col *= vec4(col.a);

	target = col;
    
}