#version 460
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUV;

struct Vertex
{
	vec3 position;
	float uvX;
	vec3 normal;
	float uvY;
	vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};

layout (push_constant) uniform PushConstants
{	
	mat4 worldMatrix;
	VertexBuffer vertexBuffer;
} pushConstants;

void main()
{
	Vertex vertex = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

	outColor = vertex.color.xyz;
	outUV.x = vertex.uvX;
	outUV.y = vertex.uvY;

	gl_Position = pushConstants.worldMatrix * vec4(vertex.position, 1.0f);
}
