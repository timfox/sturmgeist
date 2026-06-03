#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

void main()
{
	vec3 dir = normalize(gl_WorldRayDirectionEXT);
	hitColor = vec3(0.15 + 0.35 * dir.y, 0.2, 0.35 - 0.2 * dir.y);
}
