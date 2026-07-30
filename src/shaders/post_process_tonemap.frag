#version 460

precision highp float;

layout(location=0)in vec2 uv;
layout(location=0)out vec4 outColor;

layout(set=0,binding=0)uniform sampler2D sceneColor;

// Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
vec3 ACESFitted(vec3 color){
    const float a=2.51;
    const float b=.03;
    const float c=2.43;
    const float d=.59;
    const float e=.14;
    return clamp((color*(a*color+b))/(color*(c*color+d)+e),0.,1.);
}

void main(){
    // Sample linear HDR color
    vec3 hdrColor=texture(sceneColor,uv).rgb;
    
    // Apply ACES tone mapping curve
    vec3 mapped=ACESFitted(hdrColor);
    
    // Convert to sRGB for display output
    mapped=pow(mapped,vec3(1./2.2));
    
    outColor=vec4(mapped,1.);
}
