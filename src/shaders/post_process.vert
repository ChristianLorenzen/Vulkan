#version 450

layout(location = 0) out vec2 uv;

void main() {
    // Generate vertices (-1,-1), (3,-1), (-1,3)
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    
    // Convert UV [0..1] to Clip Space [-1..1]
    // Vulkan Y is down, so we flip it here (or keep it as is if 
    // your pipeline flips it elsewhere)
    gl_Position = vec4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
}