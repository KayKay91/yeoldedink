#version 150

in vec4 color;
in vec2 texCoord;

uniform sampler2D tex;
uniform vec4 myColor;
out vec4 FragColor;

void main(void)
{
    vec4 c = myColor * color;
    FragColor = texture(tex, texCoord) * c;
}