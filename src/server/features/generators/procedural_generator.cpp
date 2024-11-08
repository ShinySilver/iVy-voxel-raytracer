#include "procedural_generator.h"
#include "FastNoise/FastNoise.h"
#include "../../../common/materials.h"

static Voxel get_voxel(float *heightmap, int x, int y, int z);
static Chunk *generate_chunk(float *heightmap, int x, int y, int z);

server::ProceduralGenerator::ProceduralGenerator() = default;
server::ProceduralGenerator::~ProceduralGenerator() = default;

Region *server::ProceduralGenerator::generate_region(int rx, int ry, int rz) {

    // Heightmap parameters
    const int height_offset = 64;
    const int height_multiplier = 32;

    // Generating the heightmap
    float *height_map = (float *) malloc(sizeof(float) * IVY_REGION_WIDTH * IVY_REGION_WIDTH);
    auto simplex = FastNoise::New<FastNoise::Simplex>();
    auto fractal = FastNoise::New<FastNoise::FractalFBm>();
    fractal->SetSource(simplex);
    fractal->SetOctaveCount(5);
    fractal->GenUniformGrid2D(height_map, 0, 0, IVY_REGION_WIDTH, IVY_REGION_WIDTH, 0.005f, 1337);
    for (int i = 0; i < IVY_REGION_WIDTH * IVY_REGION_WIDTH; i++) height_map[i] = height_offset + height_map[i] * height_multiplier;

    // Using the heightmap to generate
    Region *region = new Region();
    for (int z = rz; z < IVY_REGION_WIDTH; z += IVY_NODE_WIDTH) {
        for (int y = ry; y < IVY_REGION_WIDTH; y += IVY_NODE_WIDTH) {
            for (int x = rx; x < IVY_REGION_WIDTH; x += IVY_NODE_WIDTH) {
                Chunk *chunk = generate_chunk(height_map, x, y, z);
                if (chunk != nullptr) region->add_leaf_node(x - rx, y - ry, z - rz, chunk);
            }
        }
    }
    free(height_map);
    return region;
}

HeightMap *server::ProceduralGenerator::generate_heightmap(int x, int y, int z, int width) {
    return nullptr;
}

static Voxel get_voxel(float *heightmap, int x, int y, int z) {
    if (x < 0 || x >= IVY_REGION_WIDTH || y < 0 || y >= IVY_REGION_WIDTH || z < 0 || z >= IVY_REGION_WIDTH) return STONE;
    float h = heightmap[x + y * IVY_REGION_WIDTH];
    return (z <= h) ? STONE : AIR;
}

static Chunk *generate_chunk(float *heightmap, int x, int y, int z) {
    static __thread Chunk chunk;
    int voxel_count = 0;
    for (int dz = 0; dz < IVY_NODE_WIDTH; dz += 1) {
        for (int dy = 0; dy < IVY_NODE_WIDTH; dy += 1) {
            for (int dx = 0; dx < IVY_NODE_WIDTH; dx += 1) {
                Voxel v = get_voxel(heightmap, x + dx, y + dy, z + dz);
                if (v != AIR) {
                    bool is_surface = get_voxel(heightmap, x + dx + 1, y + dy, z + dz) == AIR ||
                                      get_voxel(heightmap, x + dx - 1, y + dy, z + dz) == AIR ||
                                      get_voxel(heightmap, x + dx, y + dy + 1, z + dz) == AIR ||
                                      get_voxel(heightmap, x + dx, y + dy - 1, z + dz) == AIR ||
                                      get_voxel(heightmap, x + dx, y + dy, z + dz + 1) == AIR ||
                                      get_voxel(heightmap, x + dx, y + dy, z + dz - 1) == AIR;
                    v = is_surface ? v : AIR;
                }
                chunk.set(dx, dy, dz, v);
                if (v != AIR) voxel_count += 1;
            }
        }
    }
    if (voxel_count == 0) return nullptr;
    //if(voxel_count == IVY_NODE_WIDTH_CUBED) return nullptr;
    return &chunk;
}
