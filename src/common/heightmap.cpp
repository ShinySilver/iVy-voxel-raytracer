#include <cstdlib>
#include <cassert>
#include <cstring>
#include "common/heightmap.h"

HeightMap::HeightMap(int resolution_x, int resolution_y) :
        data{(int *) std::malloc(sizeof(int) * resolution_x * resolution_y)}, resolution_x{resolution_x}, resolution_y{resolution_y} { std::memset(data, 0, resolution_x * resolution_y * sizeof(int)); }

HeightMap::~HeightMap() { free(data); }

int HeightMap::get_value(int x, int y) {
    assert(x >= 0 && y >= 0 && x < resolution_x && y < resolution_y);
    return data[x + y * resolution_x];
}

void HeightMap::set_value(int x, int y, int value) {
    assert(x >= 0 && y >= 0 && x < resolution_x && y < resolution_y);
    data[x + y * resolution_x] = value;
}

int HeightMap::get_resolution_x() const {
    return resolution_x;
}

int HeightMap::get_resolution_y() const {
    return resolution_y;
}