#version 150

in vec4 vertex;

uniform mat4 modelviewMatrix;
uniform mat4 projectionMatrix;
uniform vec2 startPosition;
uniform vec2 mousePosition;

void main(void) {
  vec4 vertexToUse = vec4(mix(startPosition.x, mousePosition.x, vertex.x), mix(startPosition.y, mousePosition.y, vertex.y), -1.0, 1.0);

  gl_Position = projectionMatrix*(modelviewMatrix*vertexToUse);
}
