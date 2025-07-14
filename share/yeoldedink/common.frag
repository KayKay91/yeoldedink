#version 150

varying vec4 color;
varying vec2 texCoord;
out vec4 fragColor;

uniform sampler2D tex;

void main(void)
{
	fragColor = texture(tex, texCoord) * color;
}