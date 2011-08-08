#version 150

flat in vec3 fragAmbientColor;
smooth in vec3 fragDiffuseColor;
flat in vec3 fragSpecularColor;
smooth in vec2 fragTexCoords;
flat in float fragMtlNum;
flat in int fragHasTexture;
flat in float fragShininess;
flat in float fragAlpha;
flat in float fragBoneId;

out vec4 fragColor;

uniform sampler2DArray colorMap;

void main(void) {
  fragColor = vec4(fragAmbientColor, fragAlpha);
  fragColor += vec4(fragDiffuseColor, fragAlpha);
  fragColor *= mix(vec4(1.0), texture2DArray(colorMap, vec3(fragTexCoords.st, fragMtlNum)), fragHasTexture);
}
