#include <cfloat>
#include <cmath>
#include <sys/param.h>
#include "procedural_generator.h"
#include "FastNoise/FastNoise.h"

static Voxel get_voxel(float *heightmap, int x, int y, int z);
static Chunk *generate_chunk(float *heightmap, int x, int y, int z);

server::ProceduralGenerator::ProceduralGenerator() = default;
server::ProceduralGenerator::~ProceduralGenerator() = default;

void server::ProceduralGenerator::generate_view(int rx, int ry, int rz, ChunkStore& view) {

    // Heightmap parameters
    const int height_offset = 64;
    const int height_multiplier = 32;
    const int scale_multiplier = 1;

    // Generating the full-res heightmap
    float *height_map = (float *) malloc(sizeof(float) * IVY_REGION_WIDTH * IVY_REGION_WIDTH);
    auto simplex = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(simplex);
    fractal->SetOctaveCount(5);
    fractal->GenUniformGrid2D(height_map, 0, 0, IVY_REGION_WIDTH, IVY_REGION_WIDTH, 0.005f/scale_multiplier, 1337);
    for (int i = 0; i < IVY_REGION_WIDTH * IVY_REGION_WIDTH; i++) height_map[i] = height_offset + height_map[i] * height_multiplier;

    // Generating the low-res heightmap
    int *chunk_min = (int *) malloc(sizeof(int) * IVY_REGION_WIDTH * IVY_REGION_WIDTH);
    int *chunk_max = (int *) malloc(sizeof(int) * IVY_REGION_WIDTH * IVY_REGION_WIDTH);
    for (int y = ry; y < IVY_REGION_WIDTH; y += IVY_NODE_WIDTH) {
        for (int x = rx; x < IVY_REGION_WIDTH; x += IVY_NODE_WIDTH) {
            float min = FLT_MAX, max = FLT_MIN;
            for (int dy = 0; dy < IVY_NODE_WIDTH; dy++) {
                for (int dx = 0; dx < IVY_NODE_WIDTH; dx++) {
                    float h = height_map[x + dx + (y + dy) * IVY_REGION_WIDTH];
                    min = MIN(min, h);
                    max = MAX(max, h);
                }
            }
            chunk_min[x + y * IVY_REGION_WIDTH / IVY_NODE_WIDTH] = int((std::floor(min / IVY_NODE_WIDTH) - 1) * IVY_NODE_WIDTH);
            chunk_max[x + y * IVY_REGION_WIDTH / IVY_NODE_WIDTH] = int(std::ceil(max / IVY_NODE_WIDTH) * IVY_NODE_WIDTH);
        }
    }

    // Using the heightmap to generate
    for (int y = ry; y < ry + IVY_REGION_WIDTH; y += IVY_NODE_WIDTH) {
        for (int x = rx; x < rx + IVY_REGION_WIDTH; x += IVY_NODE_WIDTH) {
            int min = chunk_min[x + y * IVY_REGION_WIDTH / IVY_NODE_WIDTH];
            int max = chunk_max[x + y * IVY_REGION_WIDTH / IVY_NODE_WIDTH];
            if (max < rz || min > rz + IVY_REGION_WIDTH) continue;
            for (int z = MAX(rz, min); z <= MIN(max, rz + IVY_REGION_WIDTH); z += IVY_NODE_WIDTH) {
                Chunk *chunk = generate_chunk(height_map, x, y, z);
                if (chunk != nullptr) view.add_chunk(x - rx, y - ry, z - rz, chunk);
            }
        }
    }
    free(height_map);
}

static Voxel get_voxel(float *heightmap, int x, int y, int z) {
    if (x < 0 || x >= IVY_REGION_WIDTH || y < 0 || y >= IVY_REGION_WIDTH || z < 0 || z >= IVY_REGION_WIDTH) return Voxel{STONE};
    float h = heightmap[x + y * IVY_REGION_WIDTH];
    return (z <= h) ? Voxel{STONE} : Voxel{AIR};
}

static Chunk *generate_chunk(float *heightmap, int x, int y, int z) {
    static __thread Chunk chunk;
    int voxel_count = 0;
    for (int dz = 0; dz < IVY_NODE_WIDTH; dz += 1) {
        for (int dy = 0; dy < IVY_NODE_WIDTH; dy += 1) {
            for (int dx = 0; dx < IVY_NODE_WIDTH; dx += 1) {
                Voxel v = get_voxel(heightmap, x + dx, y + dy, z + dz);
                if (v.material != AIR) {
                    bool is_surface = get_voxel(heightmap, x + dx + 1, y + dy, z + dz).material == AIR ||
                                      get_voxel(heightmap, x + dx - 1, y + dy, z + dz).material == AIR ||
                                      get_voxel(heightmap, x + dx, y + dy + 1, z + dz).material == AIR ||
                                      get_voxel(heightmap, x + dx, y + dy - 1, z + dz).material == AIR ||
                                      get_voxel(heightmap, x + dx, y + dy, z + dz + 1).material == AIR ||
                                      get_voxel(heightmap, x + dx, y + dy, z + dz - 1).material == AIR;
                    v = is_surface ? v : Voxel{AIR};
                }
                chunk.set(dx, dy, dz, v);
                if (v.material != AIR) voxel_count += 1;
            }
        }
    }
    if (voxel_count == 0) return nullptr;
    //if(voxel_count == IVY_NODE_WIDTH_CUBED) return nullptr;
    return &chunk;
}
