const char main_pass_glsl[] = R""(
#version 460 core
#extension GL_ARB_shader_clock : enable

#define SIZEOF_NODE 12
#define NODE_WIDTH 4
#define NODE_WIDTH_SQRT 2
#define MINI_STEP_SIZE 5e-3f

#define OUTPUT_TYPE %d // 0: color, 1: time, 2: dda_steps, 3: tree_steps
#define OUTPUT_DDA_STEPS_COLOR_SATURATION 64.0f
#define OUTPUT_TREE_STEPS_COLOR_SATURATION 32.0f
#define OUTPUT_TIME_COLOR_SATURATION 100000.0f

layout (local_size_x = 8, local_size_y = 8) in;
layout (rgba8, binding = 0) uniform restrict writeonly image2D outImage;

uniform uvec2 screen_size;
uniform vec3 camera_position;
uniform mat4 view_matrix;
uniform mat4 projection_matrix;
uniform uint tree_depth;

uniform int tree_step_limit;
uniform int dda_step_limit;
uniform vec3 sun_direction;

struct Node{
    uint bitmask_low;
    uint bitmask_high;
    uint header;
};

layout (std430, binding = 0) restrict readonly buffer _node_pool{
    Node node_pool[];
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
    vec2 screen_space = (screen_position + vec2(0.5)) / vec2(screen_size);
    screen_space.y = 1.0 - screen_space.y;
    vec4 clip_space = vec4(screen_space * 2.0f - 1.0f, - 1.0, 1.0);
    vec4 eye_space = vec4(vec2(inverse(projection_matrix) * clip_space), -1.0, 0.0);
    return normalize(vec3(inverse(view_matrix) * eye_space));
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

    // setting up a few variables that are used for alternative output modes
    #if OUTPUT_TYPE == 1
    uvec2 start = clock2x32ARB();
    #endif
    int dda_steps = 0;
    int tree_steps = 0;

    // if the ray intersect the world volume, raytrace
    if (intersect >= 0) {

        // setting up the stack
        uint stack[7];
        uint depth = 0;
        uint current_node_index = 0;
        Node current_node = node_pool[current_node_index];
        stack[depth] = current_node_index;

        // defining a few variables that will be useful in the traversal
        vec3 ray_sign_11 = vec3(sign11(ray_dir.x), sign11(ray_dir.y), sign11(ray_dir.z));
        vec3 ray_sign_01 = max(ray_sign_11, 0.);
        uint node_width = uint(world_width) >> NODE_WIDTH_SQRT;
        vec3 lbmin = vec3(0), lbmax = vec3(world_width);
        float ray_step = 0;
        bool has_collided;

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
                        #if OUTPUT_TYPE == 0
                        vec3 color = colors[color_index-1]*dot(step_mask*vec3(0.9, 0.7, 0.4), vec3(1));
                        imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1));
                        #elif OUTPUT_TYPE == 1
                        uvec2 end = clock2x32ARB();
                        imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4((end.x-start.x)/OUTPUT_TIME_COLOR_SATURATION, 0, 0, 1));
                        #elif OUTPUT_TYPE == 2
                        imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(dda_steps/OUTPUT_DDA_STEPS_COLOR_SATURATION, 0, 0, 1));
                        #elif OUTPUT_TYPE == 3
                        imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(tree_steps/OUTPUT_TREE_STEPS_COLOR_SATURATION, 0, 0, 1));
                        #endif
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
            vec3 side_dist = inverted_ray_dir * (node_width * ray_sign_01 - mod(ray_pos, node_width));
            ray_step = min(side_dist.x, min(side_dist.y, side_dist.z));
            ray_pos += ray_dir * max(0, ray_step-MINI_STEP_SIZE*0.9);

            // mini-step + normal extraction
            step_mask = vec3(equal(vec3(ray_step), side_dist));
            ray_pos += MINI_STEP_SIZE * ray_sign * step_mask;

            // keeping track of the number of DDA step for reference
            dda_steps+=1;
            if(dda_steps == dda_step_limit){imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(colors[0], 1.00)); return;}
        } while(all(greaterThanEqual(ray_pos, bmin)) && all(lessThan(ray_pos, bmax))); // while in global-bound
    }

    // applying the sky color to the framebuffer
    #if OUTPUT_TYPE == 0
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(0.69, 0.88, 0.90, 1.00));
    #elif OUTPUT_TYPE == 1
    uvec2 end = clock2x32ARB();
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(vec3(end.x-start.x, 0, 0) / OUTPUT_TIME_COLOR_SATURATION, 1));
    #elif OUTPUT_TYPE == 2
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(dda_steps/OUTPUT_DDA_STEPS_COLOR_SATURATION, 0, 0, 1));
    #elif OUTPUT_TYPE == 3
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), vec4(tree_steps/OUTPUT_TREE_STEPS_COLOR_SATURATION, 0, 0, 1));
    #endif
}
)"";