const char minpool_horizontal_glsl[] = R""(
#version 460 core

layout (local_size_x = 8, local_size_y = 8) in;
layout (r16f, binding = 0) uniform restrict readonly image2D lowres_depth_texture;
layout (r16f, binding = 1) uniform restrict writeonly image2D intermediate_texture;

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    ivec2 lowres_coord = gid / 2; // Map fullres pixel to lowres texture
    if(gid.x%2!=0 && gid.y%2==0){
        float depth_left = imageLoad(lowres_depth_texture, lowres_coord - ivec2(0, 1)).r;
        float depth_right = imageLoad(lowres_depth_texture, lowres_coord + ivec2(0, 1)).r;
        imageStore(intermediate_texture, gid, vec4(min(depth_left, depth_right), 0.0, 0.0, 0.0));
    }else{
        imageStore(intermediate_texture, gid, imageLoad(lowres_depth_texture, lowres_coord).rgba);
    }
}
)"";
