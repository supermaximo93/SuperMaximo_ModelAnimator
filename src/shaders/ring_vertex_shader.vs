#version 150

in vec4 vertex;

uniform mat4 modelviewMatrix;
uniform mat4 projectionMatrix;
uniform float scale;

void main(void) {
  vec4 vertexToUse = vec4(vertex.xy*scale, vertex.zw);
  gl_Position = projectionMatrix*(modelviewMatrix*vertexToUse);
}
