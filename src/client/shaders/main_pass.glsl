const char *main_pass_glsl = R""(
#version 460 core
#extension GL_NV_gpu_shader5 : enable

layout (local_size_x = 8, local_size_y = 8) in;
layout (rgba8, binding = 0) uniform writeonly image2D outImage;

uniform uvec2 screen_size;
uniform vec3 camera_position;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;
uniform uint tree_depth;

#define SIZEOF_NODE 12 // 8 + 4
#define NODE_WIDTH 4
#define NODE_WIDTH_SQRT 2
#define MINI_STEP_SIZE 4e-2

#define U64_MAX 0xfffffffffffffffful
#define U32_MAX 0xffffffffu

struct Node{
    uint32_t bitmask_low;
    uint32_t bitmask_high;
    uint header;
};

layout (std430, binding = 0) readonly buffer _node_pool{
    Node node_pool[];
};

layout (std430, binding = 1) readonly buffer _voxel_pool{
    uint8_t voxel_pool[];
};

// voxel palette. it mirrors materials.h
vec3 colors[] = {
    vec3(1.00, 0.40, 0.40), // DEBUG_RED
    vec3(0.40, 1.00, 0.40), // DEBUG_GREEN
    vec3(0.40, 0.40, 1.00), // DEBUG_BLUE
    vec3(0.55, 0.55, 0.55), // STONE
    vec3(0.42, 0.32, 0.25), // DIRT
    vec3(0.30, 0.59, 0.31)  // GRASS
};

vec3 getRayDir(ivec2 screen_position) {
    vec2 screenSpace = (screen_position + vec2(0.5)) / vec2(screen_size);
    screenSpace.y = 1.0 - screenSpace.y;
    vec4 clipSpace = vec4(screenSpace * 2.0f - 1.0f, - 1.0, 1.0);
    vec4 eyeSpace = vec4(vec2(inverse(projection_matrix) * clipSpace), -1.0, 0.0);
    return normalize(vec3(inverse(view_matrix) * eyeSpace));
}

float AABBIntersect(vec3 bmin, vec3 bmax, vec3 orig, vec3 invdir) {
    vec3 t0 = (bmin - orig) * invdir;
    vec3 t1 = (bmax - orig) * invdir;

    vec3 vmin = min(t0, t1);
    vec3 vmax = max(t0, t1);

    float tmin = max(vmin.x, max(vmin.y, vmin.z));
    float tmax = min(vmax.x, min(vmax.y, vmax.z));

    if (!(tmax < tmin) && (tmax >= 0)) return max(0., tmin);
    return -1;
}

void main() {
    // make sure current thread is inside the window bounds
    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, screen_size))) return;

    // calc ray direction for current pixel
    vec3 ray_dir = getRayDir(ivec2(gl_GlobalInvocationID.xy));
    vec3 previous_ray_pos, ray_pos = camera_position;

    // check if the camera is outside the voxel volume
    float world_width = pow(NODE_WIDTH, tree_depth);
    float intersect = AABBIntersect(vec3(MINI_STEP_SIZE), vec3(world_width-MINI_STEP_SIZE), camera_position, 1.0f / ray_dir);

    // if it is outside the terrain, offset the ray so its starting position is (slightly) in the voxel volume
    if (intersect > 0) {
        ray_pos += ray_dir * (intersect + MINI_STEP_SIZE);
    }

    // Setting the default color as a tint of blue
    vec3 color = vec3(0.69, 0.88, 0.90);

    // If the ray intersect the world volume, raytrace
    Node current_node = node_pool[0];
    uint hit = 0, steps = 0;
    if (intersect >= 0) {
        // first step of raytracing is descending in the tree tp reach the ray initial position on the edge of the world volume
        uint node_width = uint(world_width);
        bool is_terminal, has_child;
        do {
            // We add a limitation to the number of step as a failsafe
            steps+=1;

            // We update the size of a child node as we descend
            node_width = node_width >> NODE_WIDTH_SQRT;

            // Our first step is checking in the bitmask whether or not there is a child node to descend to
            uvec3 v = (uvec3(ray_pos) & uvec3((node_width * NODE_WIDTH) - 1u)) / uvec3(node_width);
            int bitmask_index = int(v.x + v.z * NODE_WIDTH + v.y * NODE_WIDTH * NODE_WIDTH);
            if (bitmask_index < 32) {
                has_child = ((current_node.bitmask_low & (0x1u << bitmask_index)) != 0);
            } else {
                has_child = ((current_node.bitmask_high & (0x1u << (bitmask_index - 32))) != 0);
            }

            // If the current node has no child to descend to in the ray position, we can just stop there
            if(!has_child) break;

            // In order to select a child node, we start by filtering out the bit that are above the bitmask index
            uint filtered_low, filtered_high;
            if (bitmask_index < 32) {
                filtered_low = current_node.bitmask_low & ((1u << bitmask_index) - 1u);
                filtered_high = 0u;
            } else {
                filtered_low = current_node.bitmask_low;
                filtered_high = current_node.bitmask_high & ((1u << (bitmask_index - 32)) - 1u);
            }

            // Counting the remaining bits to know the index of the target child node in the children array
            uint hit_index = uint(bitCount(filtered_low) + bitCount(filtered_high));

            // Adding the index of the children array, to have the absolute index of the target child node
            hit_index += uint((current_node.header & ~(0x3u << 30))/SIZEOF_NODE);

            // Descending in the target child node!
            current_node = node_pool[hit_index];
            is_terminal = (current_node.header & (0x1u << 30)) != 0;
        } while(!is_terminal && steps < 16);

        if(has_child){
            uvec3 v = uvec3(ray_pos) & uvec3(NODE_WIDTH) - 1u;
            int bitmask_index = int(v.x + v.z * NODE_WIDTH + v.y * NODE_WIDTH * NODE_WIDTH);
            if (bitmask_index < 32) {
                has_child = ((current_node.bitmask_low & (0x1u << bitmask_index)) != 0);
            } else {
                has_child = ((current_node.bitmask_high & (0x1u << (bitmask_index - 32))) != 0);
            }
            bool is_lod = (current_node.header & (0x1u << 31)) != 0;
            if(has_child) hit = 3;
        }
    }

    // If there was a hit, we extract the color from the last node & voxel pool
    if(hit > 0){
        color = colors[hit-1]; // air with id 0 is undefined
    }

    // output color to texture
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1));
}
)"";