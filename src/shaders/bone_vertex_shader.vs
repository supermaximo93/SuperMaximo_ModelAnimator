#version 150

in vec4 vertex;
in vec3 normal;
in vec3 ambientColor;
in vec3 diffuseColor;
in vec3 specularColor;
in vec3 texCoords;
in float mtlNum;
in float hasTexture;
in float shininess;
in float alpha;

flat out vec3 fragAmbientColor;
smooth out vec3 fragDiffuseColor;
flat out vec3 fragSpecularColor;

uniform mat4 modelviewMatrix;
uniform mat4 projectionMatrix;
uniform vec4 endVertex;
uniform mat4 boneModelviewMatrix;

void main(void) {
  vec3 surfaceNormal = vec3(modelviewMatrix*vec4(normal, 0.0));
  float diff = max(0.0, dot(normalize(surfaceNormal), normalize(vec3(0.0, 50.0, 100.0))));
  fragAmbientColor = ambientColor;
  fragDiffuseColor = diffuseColor*diff;
  fragSpecularColor = specularColor;

  mat4 matrixToUse = modelviewMatrix;
  vec4 vertexToUse = vec4(vertex.xyz, 1.0);
  if (vertexToUse.y > 1.0) {
    vertexToUse = endVertex;
    matrixToUse = boneModelviewMatrix;
  }
  gl_Position = projectionMatrix*(matrixToUse*vertexToUse);
}
