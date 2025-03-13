//const char main_pass_glsl[] = R""(
#version 460 core

#define SIZEOF_NODE 12
#define NODE_WIDTH 4
#define NODE_WIDTH_SQRT 2
#define MINI_STEP_SIZE 5e-3f
#define OUTPUT_TYPE %d // legacy, ignored

layout (local_size_x = 8, local_size_y = 8) in;
layout (rgba8, binding = 0) uniform restrict writeonly image2D outImage;

uniform uvec2 screen_size;
uniform vec3 camera_position;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;
uniform uint tree_depth;

struct Node{
    uint bitmask_low;
    uint bitmask_high;
    uint header;
};

layout (std430, binding = 0) restrict readonly buffer _node_pool{
    Node node_pool[];
};

vec3 getRayDir(ivec2 screen_position) {
    vec2 screen_space = (screen_position + vec2(0.5)) / vec2(screen_size);
    screen_space.y = 1.0 - screen_space.y;
    vec4 clip_space = vec4(screen_space * 2.0f - 1.0f, - 1.0, 1.0);
    vec4 eye_space = vec4(vec2(inverse(projection_matrix) * clip_space), -1.0, 0.0);
    return normalize(vec3(inverse(view_matrix) * eye_space));
}

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

float sign11(float x) {
    return x < 0. ? -1. : 1.;
}

vec3 colors[] = {
    vec3(1.00, 0.40, 0.40), // DEBUG_RED
    vec3(0.40, 1.00, 0.40), // DEBUG_GREEN
    vec3(0.40, 0.40, 1.00), // DEBUG_BLUE
    vec3(0.55, 0.55, 0.55), // STONE
    vec3(0.42, 0.32, 0.25), // DIRT
    vec3(0.30, 0.59, 0.31)  // GRASS
};

void main() {
    // make sure current thread is inside the window bounds
    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, screen_size))) return;

    // calc ray direction for current pixel
    const vec3 ray_dir = getRayDir(ivec2(gl_GlobalInvocationID.xy));
    const vec3 inverted_ray_dir = 1.0f / ray_dir;
    const vec3 ray_sign_11 = vec3(sign11(ray_dir.x), sign11(ray_dir.y), sign11(ray_dir.z));
    const vec3 ray_sign_01 = max(ray_sign_11, 0.);
    vec3 ray_pos = camera_position, step_mask;

    // ray-box intersection to check if the camera is outside the voxel volume
    const float world_width = pow(NODE_WIDTH, tree_depth);
    const vec3 bmin = vec3(MINI_STEP_SIZE), bmax = vec3(world_width-MINI_STEP_SIZE); // Eventually consider adding a MINI_STEP_SIZE
    const float intersect = AABBIntersect(bmin, bmax, camera_position, inverted_ray_dir, step_mask);

    // if it is outside the terrain, offset the ray so its starting position is (slightly) in the voxel volume
    if (intersect > 0)  ray_pos += ray_dir * intersect + step_mask * ray_sign_11 * MINI_STEP_SIZE;

    // if the ray does not intersect the world volume, return the sky color
    if (intersect < 0) {
        imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(0.69, 0.88, 0.90, 1.00));
        return;
    }

    // if the ray intersect the world volume, raytrace
    vec3 lbmin = vec3(0), lbmax = vec3(world_width);
    uint node_width = uint(world_width) >> NODE_WIDTH_SQRT;
    float ray_step = 0;
    bool has_collided, exited_local = false, exited_global = false;

    // setting up the stack
    uint stack[7];
    uint depth = 0;
    uint current_node_index = 0;
    uint bitmask_index;
    Node current_node = node_pool[current_node_index];
    stack[depth] = current_node_index;

    // raytracing!
    do {
        /************************************************************************************
         * For the most part, we are doing the classical DDA algorithm in this do-while loop
         */
        do {
            // check hit
            uvec3 v = (uvec3(ray_pos) & uvec3((node_width * NODE_WIDTH) - 1u)) / uvec3(node_width);
            bitmask_index = v.x + (v.z << NODE_WIDTH_SQRT) + (v.y << NODE_WIDTH);
            uint mask = 1u << (bitmask_index & 31);
            has_collided = ((bitmask_index < 32) ? (current_node.bitmask_low & mask) : (current_node.bitmask_high & mask))!=0;
            if(has_collided) break;

            // dda step
            vec3 side_dist = inverted_ray_dir * (node_width * ray_sign_01 - mod(ray_pos, node_width));
            ray_step = min(side_dist.x, min(side_dist.y, side_dist.z));
            ray_pos += ray_dir * max(0, ray_step-MINI_STEP_SIZE*0.9);

            // mini-step + normal extraction
            step_mask = vec3(equal(vec3(ray_step), side_dist));
            ray_pos += MINI_STEP_SIZE * ray_sign_11 * step_mask;

            // check bbox
            exited_local = any(greaterThanEqual(ray_pos, lbmax)) || any(lessThan(ray_pos, lbmin));
            exited_global = any(greaterThanEqual(ray_pos, bmax)) || any(lessThan(ray_pos, bmin));
        } while(!has_collided && !exited_local && !exited_global);

        /************************************************************************************************************************
         * First possible reason for exiting the DDA main loop: we hit something, so we need to either go down or return a color
         */
        if(has_collided){
            do{
                // if there is a hit on a voxel in a terminal node: return hit color
                if((current_node.header & (0x1u << 30)) != 0) {
                    bool is_lod = (current_node.header & (0x1u << 31)) != 0;
                    uint color_index = is_lod ? (current_node.header & ~(0x3u << 30))-1 : 0;
                    vec3 color = colors[color_index]*dot(step_mask*vec3(0.9, 0.7, 0.4), vec3(1));
                    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1));
                    return;
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
                uvec3 v = (uvec3(ray_pos) & uvec3((node_width * NODE_WIDTH) - 1u)) / uvec3(node_width);
                bitmask_index = v.x + (v.z << NODE_WIDTH_SQRT) + (v.y << NODE_WIDTH);
                uint mask = 1u << (bitmask_index & 31);
                has_collided = ((bitmask_index < 32) ? (current_node.bitmask_low & mask) : (current_node.bitmask_high & mask))!=0;
            } while(has_collided);

            // update lbb
            lbmin = uvec3(ray_pos) & uvec3(~(node_width*NODE_WIDTH - 1u));
            lbmax = lbmin + uvec3(node_width*NODE_WIDTH);
        }

        /*****************************************************************************************************
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

            // check if we're good
            exited_local = any(greaterThanEqual(ray_pos, lbmax)) || any(lessThan(ray_pos, lbmin));
            } while(exited_local);
        }
    } while(!exited_global);

    // Third reason for exiting the DDA main loop: we exited the world boundaries, so we have to stop everything & return the sky color
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(0.69, 0.88, 0.90, 1.00));
}
)"";