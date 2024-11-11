# iVy-voxel-raytracer
Tested with ninja 1.10.1 and g++-13, on Linux. Untested on Windows.

In order to install:
```sh
$ git clone https://github.com/ShinySilver/iVy-voxel-raytracer # Clone this repository
$ cd iVy-voxel-raytracer # Enter the newly-created folder
$ mkdir build && cd build # Create a build folder and enter it
$ cmake -DCMAKE_BUILD_TYPE=Release .. && make -j8 # Build.
```
If you are using X11 or Wayland, you [might need to install a few dependencies for GLFW to work](https://www.glfw.org/docs/latest/compile.html#compile_deps_wayland).
