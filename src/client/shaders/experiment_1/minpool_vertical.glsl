const char minpool_vertical_glsl[] = R""(
#version 460 core

layout (local_size_x = 8, local_size_y = 8) in;
layout (r16f, binding = 1) uniform restrict readonly image2D intermediate_texture;
layout (r16f, binding = 0) uniform restrict writeonly image2D fullres_depth_texture;

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    if(gid.x % 2 == 0){
        float depth_up = imageLoad(intermediate_texture, gid - ivec2(1, 0)).r;
        float depth_down = imageLoad(intermediate_texture, gid + ivec2(1, 0)).r;
        imageStore(fullres_depth_texture, gid, vec4(min(depth_up, depth_down), 0.0, 0.0, 0.0));
    }else{
        imageStore(fullres_depth_texture, gid, imageLoad(intermediate_texture, gid).rgba);
    }
}
)"";
