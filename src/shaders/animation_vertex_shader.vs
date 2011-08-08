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
in float boneId;

flat out vec3 fragAmbientColor;
smooth out vec3 fragDiffuseColor;
flat out vec3 fragSpecularColor;
smooth out vec2 fragTexCoords;
flat out float fragMtlNum;
flat out int fragHasTexture;
flat out float fragShininess;
flat out float fragAlpha;
flat out float fragBoneId;

uniform mat4 modelviewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 boneModelviewMatrix[32];

void main(void) {
  mat4 matrixToUse;
  if (boneId == -1) matrixToUse = modelviewMatrix; else matrixToUse = boneModelviewMatrix[int(boneId)];
  vec3 surfaceNormal = vec3(matrixToUse*vec4(normal, 0.0));
  float diff = max(0.0, dot(normalize(surfaceNormal), normalize(vec3(0.0, 50.0, 100.0))));

  fragAmbientColor = ambientColor;
  fragDiffuseColor = diffuseColor*diff;
  fragSpecularColor = specularColor;
  fragTexCoords = texCoords.st;
  fragMtlNum = mtlNum;
  fragHasTexture = int(hasTexture);
  fragShininess = shininess;
  fragAlpha = alpha;
  fragBoneId = boneId;
  gl_Position = projectionMatrix*(matrixToUse*vertex);
}
