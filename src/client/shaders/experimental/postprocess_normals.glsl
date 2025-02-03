const char postprocess_normals_glsl[] = R""(
#version 460 core
layout (local_size_x = 8, local_size_y = 8) in;

layout (r16f, binding = 0) uniform restrict readonly image2D fullres_depth_texture;
layout (rgba8, binding = 1) uniform restrict readonly image2D voxel_and_normal_texture;
layout (rgba8, binding = 2) uniform restrict writeonly image2D out_color;

uniform vec3 camera_position;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;

const mat4 inverse_view_matrix = inverse(view_matrix);
const mat4 inverse_projection_matrix = inverse(projection_matrix);

const vec3 colors[] = {
    vec3(0.69, 0.88, 0.90), // SKY
    vec3(1.00, 0.40, 0.40), // DEBUG_RED
    vec3(0.40, 1.00, 0.40), // DEBUG_GREEN
    vec3(0.40, 0.40, 1.00), // DEBUG_BLUE
    vec3(0.55, 0.55, 0.55), // STONE
    vec3(0.42, 0.32, 0.25), // DIRT
    vec3(0.30, 0.59, 0.31)  // GRASS
};

const ivec2 screen_size = imageSize(fullres_depth_texture);

vec3 getRayDir(ivec2 screen_coordinates) {
    vec2 screen_space = (screen_coordinates + vec2(0.5)) / vec2(screen_size);
    screen_space.y = 1.0 - screen_space.y;
    vec4 clip_space = vec4(screen_space * 2.0f - 1.0f, -1.0, 1.0);
    vec4 eye_space = vec4(vec2(inverse_projection_matrix * clip_space), -1.0, 0.0);
    return normalize(vec3(inverse_view_matrix * eye_space));
}

vec3 snapNormal(vec3 normal, float threshold, float hardness) {
    vec3 absNormal = abs(normal);
    float maxComponent = max(absNormal.x, max(absNormal.y, absNormal.z));
    if(maxComponent<threshold) return normal;
    vec3 snappedNormal;
    if (absNormal.x == maxComponent) snappedNormal = vec3(sign(normal.x), 0.0, 0.0);
    else if (absNormal.y == maxComponent) snappedNormal = vec3(0.0, sign(normal.y), 0.0);
    else snappedNormal =  vec3(0.0, 0.0, sign(normal.z));
    return (normal*(1-hardness)+snappedNormal*(1+hardness))/2.;
}

void main() {
    // Getting raw color and normal
    ivec2 screen_coordinates = ivec2(gl_GlobalInvocationID.xy);
    vec4 voxel_and_normal = imageLoad(voxel_and_normal_texture, screen_coordinates);
    vec3 raw_color = colors[int(voxel_and_normal.r * 10)].rgb;
    vec3 normal = voxel_and_normal.gba;

    float depth = imageLoad(fullres_depth_texture, screen_coordinates).r;
    float depthRight = imageLoad(fullres_depth_texture, screen_coordinates + ivec2(1, 0)).r;
    float depthLeft = imageLoad(fullres_depth_texture, screen_coordinates + ivec2(-1, 0)).r;
    float depthUp = imageLoad(fullres_depth_texture, screen_coordinates + ivec2(0, 1)).r;
    float depthDown = imageLoad(fullres_depth_texture, screen_coordinates + ivec2(0, -1)).r;

    vec3 forward = getRayDir(screen_coordinates);
    vec3 forward_right = getRayDir(screen_coordinates + ivec2(1, 0));
    vec3 forward_left = getRayDir(screen_coordinates + ivec2(-1, 0));
    vec3 forward_up = getRayDir(screen_coordinates + ivec2(0, 1));
    vec3 forward_down = getRayDir(screen_coordinates + ivec2(0, -1));

    vec3 worldPos = camera_position + depth * forward;
    vec3 worldPosRight = camera_position + depthRight * forward_right;
    vec3 worldPosLeft = camera_position + depthLeft * forward_left;
    vec3 worldPosUp = camera_position + depthUp * forward_up;
    vec3 worldPosDown = camera_position + depthDown * forward_down;

    vec3 tangentU = (worldPosRight - worldPosLeft) * 0.5;
    vec3 tangentV = (worldPosUp - worldPosDown) * 0.5;

    normal = normalize(cross(tangentU, tangentV));
    normal = snapNormal(normal, 0.9, max(0, 1-depth*depth/4000.));

    // Getting the final color
    vec3 light_dir = normalize(vec3(-0.9, -0.7, -0.4));
    float ambient_light = 0.3;
    float intensity = ambient_light + max(0.0, dot(normal, light_dir)) * (1.0 - ambient_light);
    vec3 shaded_color = raw_color * intensity;
    imageStore(out_color, screen_coordinates, vec4(shaded_color, 1.0));
}

)"";