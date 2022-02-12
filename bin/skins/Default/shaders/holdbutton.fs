#ifdef EMBEDDED
varying vec2 fsTex;
varying vec4 position;
#else
#extension GL_ARB_separate_shader_objects : enable
layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;
in vec4 position;
#endif

uniform sampler2D mainTex;
uniform float objectGlow;
uniform float trackPos;
uniform float trackScale;
uniform float hiddenCutoff;
uniform float hiddenFadeWindow;
uniform float suddenCutoff;
uniform float suddenFadeWindow;

// 20Hz flickering. 0 = Miss, 1 = Inactive, 2 & 3 = Active alternating.
uniform int hitState;

#ifdef EMBEDDED
void main()
{    
    vec4 mainColor = texture(mainTex, fsTex.xy);

    target = mainColor;
	target.xyz = target.xyz * (1.0 + objectGlow * 0.3);
    target.a = min(1.0, target.a + target.a * objectGlow * 0.9);
}

#else

// The OpenGL standard leave the case when `edge0 >= edge1` undefined,
// so this function was made to remove the ambiguity when `edge0 >= edge1`.
// Note that the case when `edge0 > edge1` should be avoided.
float smoothstep_fix(float edge0, float edge1, float x)
{
    if(edge0 >= edge1)
    {
        return x < edge0 ? 0.0 : x > edge1 ? 1.0 : 0.5;
    }

    return smoothstep(edge0, edge1, x);
}

float hide()
{
    float off = trackPos + position.y * trackScale;

    if (hiddenCutoff > suddenCutoff) {
        float sudden = 1.0 - smoothstep_fix(suddenCutoff - suddenFadeWindow, suddenCutoff, off);
        float hidden = smoothstep_fix(hiddenCutoff, hiddenCutoff + hiddenFadeWindow, off);
        return min(hidden + sudden, 1.0);
    }

    float sudden = 1.0 - smoothstep_fix(suddenCutoff, suddenCutoff + suddenFadeWindow, off);
    float hidden = smoothstep_fix(hiddenCutoff - hiddenFadeWindow, hiddenCutoff, off);

    return hidden * sudden;
}

void main()
{    
    vec4 mainColor = texture(mainTex, fsTex.xy);

    target = mainColor;


    target.xyz = target.xyz * (1.0 + objectGlow * 0.3);
    target.a = min(1.0, target.a + target.a * objectGlow * 0.9);
    target *= hide();
}
#endif