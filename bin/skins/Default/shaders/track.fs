#ifdef EMBEDDED
varying vec2 fsTex;
#else
#extension GL_ARB_separate_shader_objects : enable
layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;
#endif

uniform sampler2D mainTex;
uniform vec4 lCol;
uniform vec4 rCol;
uniform float hidden;
uniform bool reverseColor;
uniform float reverseColorAmount;
uniform ivec2 viewport;

float reverseMask()
{
    float screenY = gl_FragCoord.y / float(viewport.y);
    if(reverseColor)
        return step(1.0 - reverseColorAmount, screenY);
    return 1.0 - step(reverseColorAmount, screenY);
}

void main()
{	
	vec4 mainColor = texture(mainTex, fsTex.xy);
    vec4 col = mainColor;

    if(fsTex.y > hidden)
    {
        //Red channel to color right lane
        col.xyz = vec3(.9) * rCol.xyz * vec3(mainColor.x);

        //Blue channel to color left lane
        col.xyz += vec3(.9) * lCol.xyz * vec3(mainColor.z);

        //Color green channel white
        col.xyz += vec3(.6) * vec3(mainColor.y);
    }
    else
    {
        col.xyz = vec3(0.);
        col.a = col.a > 0.0 ? 0.3 : 0.0;
    }
    col.xyz = mix(col.xyz, vec3(0.85), reverseMask() * col.a);
    target = col;
}
