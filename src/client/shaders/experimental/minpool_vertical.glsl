const char minpool_vertical_glsl[] = R""(
#version 460 core
#extension GL_ARB_shader_clock : enable

layout (local_size_x = 8, local_size_y = 8) in;
layout (r16f, binding = 0) uniform restrict writeonly image2D fullres_depth_texture;
layout (r16f, binding = 1) uniform restrict readonly image2D intermediate_texture;

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    int half_pixel_y = (gid.y % 2 == 0) ? 0 : 1;

    float top_depth = imageLoad(intermediate_texture, gid).r;
    float bottom_depth = imageLoad(intermediate_texture, gid + ivec2(0, half_pixel_y)).r;

    float min_depth = min(top_depth, bottom_depth);
    imageStore(fullres_depth_texture, gid, vec4(min_depth, 0.0, 0.0, 0.0));
}
)"";
