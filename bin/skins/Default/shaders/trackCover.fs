#version 330
#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;

uniform sampler2D mainTex;
uniform float hiddenCutoff;
uniform float hiddenFadeWindow;
uniform float suddenCutoff;
uniform float suddenFadeWindow;

void main()
{
	target = texture(mainTex, vec2(fsTex.x, fsTex.y * 2.0));

	float off = 1.0 - (fsTex.y * 2.0);

    if(hiddenCutoff < suddenCutoff)
    {
        float hiddenCutoffFade = hiddenCutoff - hiddenFadeWindow;
        if (off > hiddenCutoffFade && off < hiddenCutoff) {
            target.a = target.a * max(0.0f, (hiddenCutoff - off) / hiddenFadeWindow);
        }

		if (off < suddenCutoff && off > hiddenCutoff) {
			target.a = 0.0f;
		}

        float suddenCutoffFade = suddenCutoff + suddenFadeWindow;
        if (off < suddenCutoffFade && off > suddenCutoff) {
            target.a = target.a * max(0.0f, (off - suddenCutoff) / suddenFadeWindow);
        }
    }
    else
    {
        float hiddenCutoffFade = hiddenCutoff + hiddenFadeWindow;
        if (off > hiddenCutoff) {
            target.a = target.a * max(0.0f, (hiddenCutoffFade - off) / hiddenFadeWindow);
        }

        float suddenCutoffFade = suddenCutoff - suddenFadeWindow;
        if (off < suddenCutoff) {
            target.a = target.a * max(0.0f, (off - suddenCutoffFade) / suddenFadeWindow);
        }
    }
}