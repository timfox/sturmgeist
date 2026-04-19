#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

hitAttributeEXT vec2 attribs;

void main()
{
	const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	hitColor        = vec3(0.95, 0.55, 0.15) * (0.35 + 0.65 * bary.x);
}
