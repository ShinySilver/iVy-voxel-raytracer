const char *main_pass_glsl = R""(
#version 430 core
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
#define MINI_STEP_SIZE 4e-2

#define U64_MAX 0xfffffffffffffffful
#define U32_MAX 0xffffffffu

struct Node{
    uint64_t bitmask;
    uint header;
};

layout (packed, binding = 0) readonly buffer _node_pool{
    Node node_pool[];
};

layout (packed, binding = 0) readonly buffer _voxel_pool{
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

    // if the ray intersect the world volume, raytrace
    Node current_node = node_pool[0];
    uint hit = 0, steps = 0;
    //#if true
    if (intersect >= 0) {
        // first, reach the ray initial position on the edge of the world volume
        hit = 1;
        uint node_width = uint(world_width);
        bool is_terminal = (current_node.header & (0x1u << 30)) != 0;
        bool is_lod = (current_node.header & (0x1u << 31)) != 0;
        bool has_child;
        while(!is_terminal && !is_lod && steps < 64) {
            steps+=1;
            node_width /= NODE_WIDTH;
            uvec3 v = uvec3(ceil(ray_pos / float(node_width)));
            has_child = ((current_node.bitmask & (0x1 << v)) != 0);
            if(!has_child) break;
            uint64_t filtered = current_node.bitmask & (U64_MAX >> (64 - v.y - v.x * NODE_WIDTH - v.z * NODE_WIDTH * NODE_WIDTH));
            uint hit_index = bitCount(uint(filtered)) + bitCount(uint(filtered >> 32)) - 1; // Child with id 1 has index 0
            hit_index += (current_node.header & 0x3fffffffu)/SIZEOF_NODE; // must be a node array as it is not terminal
            current_node = node_pool[hit_index];
            is_lod = (current_node.header & (0x1u << 31)) != 0;
            is_terminal = (current_node.header & (0x1u << 30)) != 0;
            hit = 1+hit%3;
        }

        // Now that the ray found its initial location, we check if it's solid. If it is, we don't have any traversal to do
        uvec3 v = uvec3(floor(mod(ray_pos, node_width)*NODE_WIDTH/float(node_width)));
        bool is_solid = (current_node.bitmask & (U64_MAX >> (64 - v.x - v.y * NODE_WIDTH - v.z * NODE_WIDTH * NODE_WIDTH))) != 0;
        if(is_solid){
            // The ray initial location was found to be solid, so we can just extract the material
            if(is_lod){
                // Case one: this is a LOD chunk, where the (uniform) material is stored in the header
            }else{
                // Case two: this is a normal chunk, the material is stored in the voxel pool
                //uint64_t filtered = current_node.bitmask & ~(~0x0ul  << (v.x + v.y * NODE_WIDTH + v.z * NODE_WIDTH * NODE_WIDTH));
                //uint hit_index = bitCount(uint(filtered)) + bitCount(uint(filtered >> 32));
                //uint voxel_addressing_space = current_node.header & 0x3fffu;
                //hit = uint(voxel_pool[voxel_addressing_space+hit_index]);
            }
        }else{
            // The ray initial location was found to be air, so we have to traverse the volume with tree-dda
            // WHILE TRUE
            // call the bitmask_dda function to trace through the current node
            // if the bitmask_dda function got the ray out of the world volume, break.
            // if the bitmask_dda function does NOT return a hit, update ray position, change current node & reduce depth and step size & *continue*
            // if the bitmask_dda function does return a hit & we are at max depth, *return* the hit color
            // if the bitmask_dda function does return a hit & we are NOT at max depth, change current node & go deeper depth and step size, and *continue*
            // END-WHILE
        }
    }
    //#endif

    // If there was a hit, we extract the color from the last node & voxel pool
    if(hit > 0){
        color = colors[hit-1]; // air with id 0 is undefined
    }

    // output color to texture
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1));
}
)"";