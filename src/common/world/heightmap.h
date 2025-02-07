#pragma once

class HeightMap {
private:
    int *data;
    int resolution_x, resolution_y;
public:
    HeightMap(int resolution_x, int resolution_y);
    ~HeightMap();
    int get_value(int x, int y);
    void set_value(int x, int y, int value);
    int get_resolution_x() const;
    int get_resolution_y() const;
};