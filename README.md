# iVy-voxel-raytracer
Tested with cmake 3.22.1, ninja 1.10.1 and g++-13, on Linux. Untested on Windows.

In order to install and run:
```sh
git clone https://github.com/ShinySilver/iVy-voxel-raytracer # Clone this repository
cd iVy-voxel-raytracer # Enter the newly-created folder
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release # Use cmake to setup the build
cd build # Enter the build folder
ninja iVy # Build
chmod +x iVy # On Linux, you need to autorise running the executable before running it
./iVy # Run the engine
```
If you are using X11 or Wayland, you [might need to install a few dependencies for GLFW to work](https://www.glfw.org/docs/latest/compile.html#compile_deps_wayland).

You can change the world size in [world.h, line 11](https://github.com/ShinySilver/iVy-voxel-raytracer/blob/master/src/common/world.h#L12C9-L12C30). 5 means 4**5=1024 voxels, 6 is 4096, 7 is 16384.
