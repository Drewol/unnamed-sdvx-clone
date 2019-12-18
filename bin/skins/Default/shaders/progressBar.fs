#version 330
#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;

uniform float progress;


void main()
{
    target = vec4(0.);
    if(fsTex.y < 0.1)
    {
        target.a = 0.5;
        if (fsTex.x < 0.1 || fsTex.x > 0.9)
        {
            float y = fsTex.y / 0.05;
            y -= 1;
            target = vec4(0., .7 - pow(y,2), 1., 1. - pow(y,2));
        }
        else if(abs(fsTex.y - 0.05) < 0.02)
        {
            if(fsTex.x < progress * 0.8 + 0.1)
                target = vec4(0.0 , 1.0 , 0.0 , 1.0);
        }
    }
}