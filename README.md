# Fern-Script

This is a fun little side-project that implemements an interpereter for a custom script language called Fern-Script. It main purpose is to teach me about parsing text in general by following the [Guide to Interpreters Series](https://github.com/tlaceby/guide-to-interpreters-series) from Tyler Laceby, but instead of using TypeScript (like from the series), C is used instead.

This project was started as a result from being side tracked on figuring out how CSS-like syntax can be parsed and interpreted in [Clay Sculptor](https://github.com/elmfrain/clay-sculptor) since I didn't have expereince in this regard yet. I figured, in order to do it correctly and make it robust, I must learn and understand the fundementals of parsing before I start implmenting sub-par parsing algorithms.

This was also a good opportunity to learn and apply good C practices by using concepts demonstrated from [Nic Barker](https://github.com/nicbarker) such as memory arenas, bounds checking, custom strings, and leveraging macros effectively.

## Building

This project is built using one Makefile. Run:

```bash
make
```

and the execututable is available in `build/fernc`.

## Project Structure

* The root directory contains the `Makefile` that is used to build the project.
* Include `.h` files are in the `include` directory. There are no include directories defined in the `Makefile`, so each source file must reference them using the `include/some_header.h` pattern.
* Source `.c` files are in the root directory.
