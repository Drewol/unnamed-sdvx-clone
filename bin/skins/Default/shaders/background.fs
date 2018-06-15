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

#define PI 3.14159265359
#define TWO_PI 6.28318530718

vec3 hsv2rgb(vec3 c) {
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

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


// Reference to
// http://thndl.com/square-shaped-shaders.html
// https://thebookofshaders.com/07/

float GetDistanceShape(vec2 st, int N){
    vec3 color = vec3(0.0);
    float d = 0.0;

    // Angle and radius from the current pixel
    float a = atan(st.x,st.y)+PI;
    float r = TWO_PI/float(N);

    // Shaping function that modulate the distance
    d = cos(floor(.5+a/r)*r-a)*length(st);

    return d;

}


  // Number of sides of your shape
  int N = 4;

  // "Stretch" | lower = "Stretchier"
  float Stretch = .3; 

  // Speed
  float speed = 1;

  // Default rotation in radians
  float BaseRotation = 0.0;

  // Rotation of texture for alignment, in radians
  float BaseTexRotation = 0.0;
  
  // Scale
  vec2 Scale = vec2(1.0, 0.6);

void main()
{
    float ar = float(viewport.x) / viewport.y;
    
	vec2 uv = vec2(texVp.x / viewport.x, texVp.y / viewport.y);
    uv.x *= ar;
    
    vec2 center = vec2(screenCenter) / vec2(viewport);
    center.x *= ar;
    
    vec2 point = uv;
    point = rotate_point(center, BaseRotation + (tilt * TWO_PI), point);
    vec2 point_diff = center - point;
    point_diff /= Scale;
    float diff = GetDistanceShape(point_diff,N);
    float thing2 = Stretch / (diff);
	float fog = -1. / (diff * 10. * Scale.x) + 1.;
    
    float texY = thing2;
    texY += timing.x * speed;

    
    float rot = (atan(point_diff.x,point_diff.y) + BaseTexRotation) / TWO_PI;

    vec4 col = texture(mainTex, vec2(rot,texY));
    float hsvVal = (col.x + col.y + col.z) / 3.0;
    vec4 clear_col = vec4(hsv2rgb(vec3(cos(rot * 10) + 0.4, 1.0, hsvVal)), col.a);

    col.xyz *= (1.0 - clearTransition);
    col.xyz += clear_col.xyz * clearTransition * 2;
    col.xyz *= 1.0 + (clearTransition);
    col.xyz *= vec3(fog);
    col *= col.a;
    target.xyz = col.xyz;
    target.a = 1.0;
}
