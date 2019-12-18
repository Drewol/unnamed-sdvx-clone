#version 330
#extension GL_ARB_separate_shader_objects : enable
layout(points) in;
layout(triangle_strip, max_vertices = 36) out;

// Input
in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[1];

// Output
out gl_PerVertex
{
	vec4 gl_Position;
};
layout(location=1) out vec2 fsTex;

uniform mat4 proj;
uniform mat4 world;

// Buton parameters
uniform vec2 size;
uniform vec4 border;
uniform vec4 texBorder;

void Quad(vec2 pos, vec2 size, vec2 uvMin, vec2 uvMax)
{
	gl_Position = proj * world * vec4(pos, 0, 1);
	fsTex = vec2(uvMin.x, uvMin.y);
	EmitVertex();

	gl_Position = proj * world * vec4(pos + vec2(size.x, 0), 0, 1);
	fsTex = vec2(uvMax.x, uvMin.y);
	EmitVertex();

	gl_Position = proj * world * vec4(pos + vec2(0, size.y), 0, 1);
	fsTex = vec2(uvMin.x, uvMax.y);
	EmitVertex();

	gl_Position = proj * world * vec4(pos + vec2(size.x, size.y), 0, 1);
	fsTex = vec2(uvMax.x, uvMax.y);
	EmitVertex();
	EndPrimitive();
}

void main()
{
	// Center
	vec2 inner = border.zw - border.xy;
	Quad(border.xy * size, size * inner, texBorder.xy, texBorder.zw);

	// Corners
	Quad(vec2(0,0), size * border.xy,
		vec2(0,0), texBorder.xy);
	Quad(vec2(0,size.y) - vec2(0,border.y * size.y) , size * border.xy,
		vec2(0,texBorder.w), vec2(texBorder.x,1));
	Quad(vec2(size.x,0) - vec2(border.x * size.x,0), size * border.xy,
		vec2(texBorder.z,0), vec2(1,texBorder.y));
	Quad(vec2(size.x,size.y) - vec2(border.x * size.x, border.y * size.y), size * border.xy,
		texBorder.zw, vec2(1,1));

	// Sides
	Quad(size * vec2(0,border.y), size * vec2(border.x, inner.y),
		vec2(0,texBorder.y), vec2(texBorder.x, texBorder.w));
	Quad(size * vec2(border.z,border.y), size * vec2(border.x, inner.y),
		vec2(texBorder.z,texBorder.y), vec2(1, texBorder.w));
	Quad(size * vec2(border.x,0), size * vec2(inner.x, border.y),
		vec2(texBorder.x,0), vec2(texBorder.z, texBorder.y));
	Quad(size * vec2(border.x,border.w), size * vec2(inner.x, 1-border.w),
		vec2(texBorder.x,texBorder.w), vec2(texBorder.z, 1));
}