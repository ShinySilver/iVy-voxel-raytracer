const char *main_pass_glsl = R""(
#version 460 core
#extension GL_NV_gpu_shader5 : enable

layout (local_size_x = 8, local_size_y = 8) in;
layout (rgba8, binding = 0) uniform restrict writeonly image2D outImage;

uniform uvec2 screen_size;
uniform vec3 camera_position;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;
uniform uint tree_depth;

#define SIZEOF_NODE 12 // 8 + 4
#define NODE_WIDTH 4
#define NODE_WIDTH_SQRT 2
#define MINI_STEP_SIZE 4e-2f

struct Node{
    uint bitmask_low;
    uint bitmask_high;
    uint header;
};

layout (std430, binding = 0) restrict readonly buffer _node_pool{
    Node node_pool[];
};

layout (std430, binding = 1) restrict readonly buffer _voxel_pool{
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

float AABBIntersect(vec3 bmin, vec3 bmax, vec3 orig, vec3 invdir, out vec3 aabb_mask) {
    vec3 t0 = (bmin - orig) * invdir;
    vec3 t1 = (bmax - orig) * invdir;

    vec3 vmin = min(t0, t1);
    vec3 vmax = max(t0, t1);

    float tmin = max(vmin.x, max(vmin.y, vmin.z));
    float tmax = min(vmax.x, min(vmax.y, vmax.z));

    if (!(tmax < tmin) && (tmax >= 0.0)) {
        aabb_mask = vec3(equal(vmin, vec3(tmin)));
        return max(0.0, tmin);
    }
    return -1.0;
}

float sign11(float x) {
    return x < 0. ? -1. : 1.;
}

void main() {
    // make sure current thread is inside the window bounds
    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, screen_size))) return;

    // calc ray direction for current pixel
    vec3 ray_dir = getRayDir(ivec2(gl_GlobalInvocationID.xy));
    vec3 inverted_ray_dir = 1.0f / ray_dir;
    vec3 previous_ray_pos, ray_pos = camera_position;
    vec3 ray_sign = vec3(sign11(ray_dir.x), sign11(ray_dir.y), sign11(ray_dir.z));

    // check if the camera is outside the voxel volume
    float world_width = pow(NODE_WIDTH, tree_depth);
    vec3 bmin = vec3(MINI_STEP_SIZE), bmax = vec3(world_width-MINI_STEP_SIZE); // Eventually consider adding a MINI_STEP_SIZE
    vec3 step_mask;
    float intersect = AABBIntersect(bmin, bmax, camera_position, inverted_ray_dir, step_mask);

    // if it is outside the terrain, offset the ray so its starting position is (slightly) in the voxel volume
    if (intersect > 0) {
        ray_pos += ray_dir * intersect + step_mask * ray_sign * MINI_STEP_SIZE;
    }

    // If the ray intersect the world volume, raytrace
    if (intersect >= 0) {

        // precomputing a few constants
        vec3 ray_sign_11 = vec3(sign11(ray_dir.x), sign11(ray_dir.y), sign11(ray_dir.z));
        vec3 ray_sign_01 = max(ray_sign_11, 0.);

        // setting up the stack
        uint stack[7];
        uint depth = 0;
        uint current_node_index = 0;
        Node current_node = node_pool[current_node_index];
        stack[depth] = current_node_index;

        // defining a few variables that will be useful in the traversal
        uint node_width = uint(world_width) >> NODE_WIDTH_SQRT;
        vec3 lbmin = vec3(0), lbmax = vec3(world_width);
        float ray_step = 0;
        bool has_collided;
        int dda_steps = 0, dda_step_limit = 0;
        int tree_steps = 0, tree_step_limit = 0;

        do {
            // while not in-local-bound, ascend the tree
            while(any(greaterThanEqual(ray_pos, lbmax)) || any(lessThan(ray_pos, lbmin))){

                // go up
                current_node_index = stack[depth];
                current_node = node_pool[current_node_index];
                depth -= 1;

                // update node width
                node_width = node_width << NODE_WIDTH_SQRT;

                // update lbb
                lbmin = uvec3(lbmin) & uvec3(~(node_width * NODE_WIDTH - 1u));
                lbmax = lbmin + uvec3(node_width*NODE_WIDTH);

                // keeping track of the number of tree steps for reference
                tree_steps += 1;
                if(tree_steps == tree_step_limit){imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(colors[1], 1.00)); return;}
            }

            // check hit
            uvec3 v = (uvec3(ray_pos) & uvec3((node_width * NODE_WIDTH) - 1u)) / uvec3(node_width); // TODO: replace division with a bitwise shift?
            int bitmask_index = int(v.x + v.z * NODE_WIDTH + v.y * NODE_WIDTH * NODE_WIDTH);
            if (bitmask_index < 32) {
                has_collided = ((current_node.bitmask_low & (0x1u << bitmask_index)) != 0);
            } else {
                has_collided = ((current_node.bitmask_high & (0x1u << (bitmask_index - 32))) != 0);
            }

            // we go down the tree until we either hit an empty node or we hit a voxel
            if(has_collided){
                do{
                    // if there is a hit on a voxel in a terminal node: return hit color
                    bool is_terminal = (current_node.header & (0x1u << 30)) != 0;
                    if(is_terminal){

                        // Extracting the base surface color from the tree
                        uint color_index = 0;
                        bool is_lod = (current_node.header & (0x1u << 31)) != 0;
                        if(is_lod){
                            color_index = current_node.header & ~(0x3u << 30);
                        }else{
                            // Not implemented. We return DEBUG_RED
                        }

                        // Calculating the final surface color && applying it :)
                        vec3 color = colors[color_index-1]*dot(step_mask*vec3(0.9, 0.7, 0.4), vec3(1));
                        imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1));
                        return;
                    }

                    // if we get a hit on a non-empty node, we lookup its node index to know where to descend to
                    uint filtered_low, filtered_high;
                    if (bitmask_index < 32) {
                        filtered_low = current_node.bitmask_low & ((1u << bitmask_index) - 1u);
                        filtered_high = 0u;
                    } else {
                        filtered_low = current_node.bitmask_low;
                        filtered_high = current_node.bitmask_high & ((1u << (bitmask_index - 32)) - 1u);
                    }
                    uint hit_index = uint(bitCount(filtered_low) + bitCount(filtered_high)) + uint((current_node.header & ~(0x3u << 30))/SIZEOF_NODE);

                    // going down
                    depth += 1;
                    stack[depth] = current_node_index;
                    current_node_index = hit_index;
                    current_node = node_pool[hit_index];

                    // updating node width
                    node_width = node_width >> NODE_WIDTH_SQRT;

                    // check hit
                    v = (uvec3(ray_pos) & uvec3((node_width * NODE_WIDTH) - 1u)) / uvec3(node_width);
                    bitmask_index = int(v.x + v.z * NODE_WIDTH + v.y * NODE_WIDTH * NODE_WIDTH);
                    if (bitmask_index < 32) {
                        has_collided = ((current_node.bitmask_low & (0x1u << bitmask_index)) != 0);
                    } else {
                        has_collided = ((current_node.bitmask_high & (0x1u << (bitmask_index - 32))) != 0);
                    }

                    // keeping track of the number of tree steps for reference
                    tree_steps += 1;
                    if(tree_steps == tree_step_limit){imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(colors[has_collided?1:2], 1.00)); return;}
                } while(has_collided);

                // update lbb
                lbmin = uvec3(ray_pos) & uvec3(~(node_width*NODE_WIDTH - 1u));
                lbmax = lbmin + uvec3(node_width*NODE_WIDTH);
            }

            // dda step
            vec3 tmax = inverted_ray_dir * (node_width * ray_sign_01 - mod(ray_pos, node_width));
            ray_step = min(tmax.x, min(tmax.y, tmax.z));
            ray_pos += ray_dir * ray_step;

            // mini-step + normal extraction
            step_mask = vec3(equal(vec3(ray_step), tmax));
            ray_pos += MINI_STEP_SIZE * ray_sign * step_mask;

            // Keeping track of the number of DDA step for reference
            dda_steps+=1;
            if(dda_steps == dda_step_limit){imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(colors[0], 1.00)); return;}
        } while(all(greaterThanEqual(ray_pos, bmin)) && all(lessThan(ray_pos, bmax))); // while in global-bound
    }

    // Applying the sky color to the framebuffer
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(0.69, 0.88, 0.90, 1.00));
}
)"";