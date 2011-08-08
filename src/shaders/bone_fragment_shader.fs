#version 150

flat in vec3 fragAmbientColor;
smooth in vec3 fragDiffuseColor;
flat in vec3 fragSpecularColor;

out vec4 fragColor;

uniform int selected;

void main(void) {
  fragColor = vec4(fragAmbientColor, 1.0);
  fragColor += vec4(fragDiffuseColor, 1.0);
  fragColor *= mix(vec4(1.0), vec4(1.0, 0.0, 0.0, 1.0), selected);
}
