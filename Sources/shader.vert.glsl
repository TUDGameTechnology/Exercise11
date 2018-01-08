#version 450

in vec3 pos;
in vec2 tex;
in vec3 nor;
out vec2 texCoord;
out vec3 normal;
uniform mat4 PV;
uniform mat4 M;

void main() {
	gl_Position = PV * M * vec4(pos.x, pos.y, pos.z, 1.0);
	texCoord = tex;
	normal = (PV * M * vec4(nor, 0.0)).xyz;
}
