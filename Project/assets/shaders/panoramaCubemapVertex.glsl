#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec3 out_Position;

const vec3 positions[36] = vec3[]
(
	// Back
	vec3(-1.0f, -1.0f, -1.0f), 
	vec3( 1.0f,  1.0f, -1.0f),  
	vec3( 1.0f, -1.0f, -1.0f),           
	vec3( 1.0f,  1.0f, -1.0f),  
	vec3(-1.0f, -1.0f, -1.0f),  
	vec3(-1.0f,  1.0f, -1.0f),  
	// Front
	vec3(-1.0f, -1.0f,  1.0f),  
	vec3( 1.0f, -1.0f,  1.0f),  
	vec3( 1.0f,  1.0f,  1.0f),  
	vec3( 1.0f,  1.0f,  1.0f),  
	vec3(-1.0f,  1.0f,  1.0f),  
	vec3(-1.0f, -1.0f,  1.0f),  
	// Left
	vec3(-1.0f,  1.0f,  1.0f),
	vec3(-1.0f,  1.0f, -1.0f),
	vec3(-1.0f, -1.0f, -1.0f),
	vec3(-1.0f, -1.0f, -1.0f),
	vec3(-1.0f, -1.0f,  1.0f),
	vec3(-1.0f,  1.0f,  1.0f),
	// Right
	vec3( 1.0f,  1.0f,  1.0f),  
	vec3( 1.0f, -1.0f, -1.0f),  
	vec3( 1.0f,  1.0f, -1.0f),        
	vec3( 1.0f, -1.0f, -1.0f),  
	vec3( 1.0f,  1.0f,  1.0f),  
	vec3( 1.0f, -1.0f,  1.0f),      
	// Bottom
	vec3(-1.0f, -1.0f, -1.0f),  
	vec3( 1.0f, -1.0f, -1.0f),  
	vec3( 1.0f, -1.0f,  1.0f),  
	vec3( 1.0f, -1.0f,  1.0f),  
	vec3(-1.0f, -1.0f,  1.0f),  
	vec3(-1.0f, -1.0f, -1.0f),  
	// Top
	vec3(-1.0f,  1.0f, -1.0f),  
	vec3( 1.0f,  1.0f , 1.0f),  
	vec3( 1.0f,  1.0f, -1.0f),    
	vec3( 1.0f,  1.0f,  1.0f),  
	vec3(-1.0f,  1.0f, -1.0f),  
	vec3(-1.0f,  1.0f,  1.0f)
);

layout (push_constant) uniform Constants
{
	mat4 Projection;
	mat4 View;
} constants;

void main() 
{
	out_Position	= positions[gl_VertexIndex];	
	gl_Position 	= constants.Projection * constants.View * vec4(out_Position, 1.0f);
}