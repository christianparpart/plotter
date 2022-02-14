# Complex function plotter using Sixel graphics in the terminal

Proof of Concept! Bugs intended.

**IMPORTANT! You'll need a Sixel-capable terminal!**

### Sixel-capable Terminals

- Contour
- mlterm
- Wezterm
- Xterm (invoke via `xterm -ti 340`)

### Installing Dependencies

```sh
sudo apt install libsixel-dev
```

### Building from source

```sh
mkdir build
cmake -S . -B build
cmake --build build/
./build/plotter
```
