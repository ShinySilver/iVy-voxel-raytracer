const char main_pass_glsl[] = R""(
#version 460 core

#define SIZEOF_NODE 12
#define NODE_WIDTH 4
#define NODE_WIDTH_SQRT 2
#define MINI_STEP_SIZE 5e-3f
#define MICRO_STEP_SIZE 45e-3f

/**
 * A tree node
 */
struct Node{
    uint bitmask_low;
    uint bitmask_high;
    uint header;
};

/**
 * Layout & uniforms
 */
layout (local_size_x = 8, local_size_y = 8) in;
layout (rgba8, binding = 0) uniform restrict writeonly image2D outImage;
layout (std430, binding = 0) readonly buffer _node_pool { Node node_pool[]; };
uniform uvec2 screen_size;
uniform vec3 camera_position;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;
uniform uint world_width;

/**
 * Basic axis-aligned bounding box collision check.
 */
float AABBIntersect(vec3 bmin, vec3 bmax, vec3 orig, vec3 invdir, out vec3 aabb_mask) {
    vec3 t0 = (bmin - orig) * invdir;
    vec3 t1 = (bmax - orig) * invdir;
    vec3 vmin = min(t0, t1), vmax = max(t0, t1);
    float tmin = max(vmin.x, max(vmin.y, vmin.z));
    float tmax = min(vmax.x, min(vmax.y, vmax.z));
    if (tmax < tmin || tmax < 0.0) return -1.0;
    aabb_mask = step(vmin, vec3(tmin));
    return max(0.0, tmin);
}

/**
 * Our raytracing function
 */
uint raytrace(inout vec3 ray_pos, vec3 ray_dir){
    // caching a few commonly used values
    const vec3 inverted_ray_dir = 1.0f / ray_dir;
    const vec3 ray_sign_11 = vec3(ray_dir.x < 0. ? -1. : 1., ray_dir.y < 0. ? -1. : 1., ray_dir.z < 0. ? -1. : 1.);
    const vec3 ray_sign_01 = max(ray_sign_11, 0.);

    // a variable used to know which direction we have to mini-step to after each step (including the AABB jump step)
    vec3 step_mask;

    // ray-box intersection and a big step if the camera is outside the voxel volume
    const vec3 bmin = vec3(MINI_STEP_SIZE), bmax = vec3(world_width-MINI_STEP_SIZE);
    if(any(greaterThanEqual(ray_pos, bmax) || lessThan(ray_pos, bmin))){
        const float intersect = AABBIntersect(bmin, bmax, camera_position, inverted_ray_dir, step_mask);
        if (intersect < 0) return 0; // not bothering with rays that will not hit the bbox
        if (intersect > 0) ray_pos += ray_dir * intersect + step_mask * ray_sign_11 * MINI_STEP_SIZE;
    }

    // creating variables to track the size and bounding box of the **current** node, as well as the state of the current step
    vec3 lbmin = vec3(0), lbmax = vec3(world_width);
    uint node_width = uint(world_width) >> NODE_WIDTH_SQRT;
    bool has_collided, exited_local = false, exited_global = false;

    // setting up the stack & recurrent variables that will be used to traverse the 64-tree
    uint stack[7];
    uint depth = 0;
    uint current_node_index = 0;
    uint bitmask_index;
    Node current_node = node_pool[current_node_index];
    stack[depth] = current_node_index;

    // Doing the actual traversal!
    do {
        /**
         * For the most part, we are doing the classical DDA algorithm in this do-while loop
         */
        do {
            // check hit
            uvec3 v = (uvec3(ray_pos) & (node_width * NODE_WIDTH - 1u)) >> findMSB(node_width);
            bitmask_index = v.x + (v.z << NODE_WIDTH_SQRT) + (v.y << NODE_WIDTH);
            has_collided = (((bitmask_index < 32) ? current_node.bitmask_low : current_node.bitmask_high) & (1u << (bitmask_index & 31)))!=0; // 3.30 -> 3.23
            if(has_collided) break;

            // dda step, normal extraction, and mini-step
            vec3 side_dist = inverted_ray_dir * (node_width * ray_sign_01 - mod(ray_pos, node_width));
            float ray_step = min(min(side_dist.x, side_dist.y), side_dist.z);
            step_mask = vec3(equal(ray_step.xxx, side_dist));
            ray_pos += ray_dir * ray_step + MINI_STEP_SIZE * ray_sign_11 * step_mask;

            // check bbox
            exited_local = any(greaterThanEqual(ray_pos, lbmax) || lessThan(ray_pos, lbmin));
            exited_global = any(greaterThanEqual(ray_pos, bmax) || lessThan(ray_pos, bmin));
        } while(!has_collided && !exited_local && !exited_global);

        /**
         * First possible reason for exiting the DDA main loop: we hit something, so we need to either go down or return a color
         */
        if(has_collided){
            do{
                // if there is a hit on a voxel in a terminal node: return hit color
                if((current_node.header & (0x1u << 30)) != 0) {
                    bool is_lod = (current_node.header & (0x1u << 31)) != 0;
                    uint color_index = is_lod ? (current_node.header & ~(0x3u << 30)) : 1;
                    ray_pos -= vec3(MINI_STEP_SIZE)*ray_sign_11;
                    return color_index;
                }

                // if we get a hit on a non-empty node, we lookup its node index to know where to descend to
                uint filtered_low, filtered_high;
                if (bitmask_index < 32) { filtered_low = current_node.bitmask_low & ((1u << bitmask_index) - 1u); filtered_high = 0u; }
                else { filtered_low = current_node.bitmask_low; filtered_high = current_node.bitmask_high & ((1u << (bitmask_index - 32)) - 1u); }
                uint hit_index = uint(bitCount(filtered_low) + bitCount(filtered_high)) + uint((current_node.header & ~(0x3u << 30))/SIZEOF_NODE);

                // going down
                depth += 1;
                stack[depth] = current_node_index;
                current_node_index = hit_index;
                current_node = node_pool[hit_index];
                node_width = node_width >> NODE_WIDTH_SQRT;

                // check hit
                uvec3 v = (uvec3(ray_pos) & (node_width * NODE_WIDTH - 1u)) >> findMSB(node_width);
                bitmask_index = v.x + (v.z << NODE_WIDTH_SQRT) + (v.y << NODE_WIDTH);
                uint mask = 1u << (bitmask_index & 31);
                has_collided = ((bitmask_index < 32) ? (current_node.bitmask_low & mask) : (current_node.bitmask_high & mask))!=0;
            } while(has_collided);

            // update lbb
            lbmin = uvec3(ray_pos) & uvec3(~(node_width*NODE_WIDTH - 1u));
            lbmax = lbmin + uvec3(node_width*NODE_WIDTH);
        }

        /**
         * Second possible reason for exiting the DDA main loop: we exited the current node, we have to go up
         */
        else if(exited_local && !exited_global){
            do {
                // go up
                current_node_index = stack[depth];
                current_node = node_pool[current_node_index];
                depth -= 1;

                // update node width
                node_width = node_width << NODE_WIDTH_SQRT;

                // update lbb
                lbmin = uvec3(lbmin) & uvec3(~(node_width * NODE_WIDTH - 1u));
                lbmax = lbmin + uvec3(node_width*NODE_WIDTH);

                // check if we're good to resume the DDA
                exited_local = any(greaterThanEqual(ray_pos, lbmax) || lessThan(ray_pos, lbmin));
            } while(exited_local);
        }
    } while(!exited_global);
    return 0;
}

/**
 * Doing some maths to get the ray dir given the camera position, direction, and the target pixel on-screen coordinates
 */
vec3 getRayDir(ivec2 screen_position) {
    vec2 screen_space = (screen_position + vec2(0.5)) / vec2(screen_size);
    screen_space.y = 1.0 - screen_space.y;
    vec4 clip_space = vec4(screen_space * 2.0f - 1.0f, - 1.0, 1.0);
    vec4 eye_space = vec4(vec2(inverse(projection_matrix) * clip_space), -1.0, 0.0);
    return normalize(vec3(inverse(view_matrix) * eye_space));
}

/**
 * Our voxel color table
 */
const vec3 colors[] = {
    vec3(0.69, 0.88, 0.90), // SKY_BLUE
    vec3(1.00, 0.40, 0.40), // DEBUG_RED
    vec3(0.40, 1.00, 0.40), // DEBUG_GREEN
    vec3(0.40, 0.40, 1.00), // DEBUG_BLUE
    vec3(0.55, 0.55, 0.55), // STONE
    vec3(0.42, 0.32, 0.25), // DIRT
    vec3(0.30, 0.59, 0.31)  // GRASS
};

/**
 * Main function!
 */
void main() {
    // make sure current thread is inside the window bounds
    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, screen_size))) return;

    // get the ray direction for the current pixel
    const vec3 ray_dir = getRayDir(ivec2(gl_GlobalInvocationID.xy));

    // primary ray
    vec3 ray_pos = camera_position;
    uint voxel_index = raytrace(ray_pos, ray_dir);
    vec3 voxel_color = colors[voxel_index];

    // secondary ray with fixed sun direction
    const vec3 sun_direction = normalize(vec3(0.4, 0.4, 1.0));
    if (voxel_index != 0) {
        uint sun_voxel_index = raytrace(ray_pos, sun_direction);
        if(sun_voxel_index!=0) voxel_color *= 0.5; // Reduced brightness for shadows
    }

    // writing to the framebuffer
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(voxel_color, 0));
}
)"";