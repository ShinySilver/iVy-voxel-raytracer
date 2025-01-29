const char minpool_horizontal_glsl[] = R""(
#version 460 core
#extension GL_ARB_shader_clock : enable

layout (local_size_x = 8, local_size_y = 8) in;
layout (r16f, binding = 0) uniform restrict readonly image2D lowres_depth_texture;
layout (r16f, binding = 1) uniform restrict writeonly image2D intermediate_texture;

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    ivec2 lowres_coord = gid / 2; // Map fullres pixel to lowres texture (divide by 2)
    int half_pixel_x = (gid.x % 2 == 0) ? 0 : 1;

    float left_depth = imageLoad(lowres_depth_texture, lowres_coord).r;
    float right_depth = imageLoad(lowres_depth_texture, lowres_coord + ivec2(half_pixel_x, 0)).r;

    float min_depth = min(left_depth, right_depth);
    imageStore(intermediate_texture, gid, vec4(min_depth, 0.0, 0.0, 0.0));
}
)"";
