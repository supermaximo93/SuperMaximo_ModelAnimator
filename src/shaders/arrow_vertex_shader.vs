#version 150

in vec4 vertex;

uniform mat4 modelviewMatrix;
uniform mat4 projectionMatrix;
uniform float arrowLength;

void main(void) {
  vec4 vertexToUse = mix(vec4(arrowLength, 0.0, 0.0, 1.0), vertex, vertex.w);
  vertexToUse.xy *= mix(1.0, arrowLength/8.0, abs(vertexToUse.y));
  gl_Position = projectionMatrix*(modelviewMatrix*vertexToUse);
}
