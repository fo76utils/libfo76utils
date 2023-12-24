# libfo76utils

Library of common source code used by [fo76utils](https://github.com/fo76utils/fo76utils) and [ce2utils](https://github.com/fo76utils/ce2utils).

Defining the macro BUILD\_CE2UTILS enables the use of default paths and environment variable names specific to Starfield.

* **ba2file.cpp**, **ba2file.hpp**: class BA2File: reads archive formats used by Oblivion, Fallout 3/New Vegas, Skyrim, Fallout 4, Fallout 76 and Starfield. Supports reading multiple archives and loose files.
* **bits.c**, **bits.h**, **bptc-tables.c**, **bptc-tables.h**, **decompress-bptc.c**, **decompress-bptc-float.c**, **detex.h**: [detex](https://github.com/hglm/detex) source code used for decoding textures in BC6 and BC7 formats.
* **cdb_file.cpp**, **cdb_file.hpp**, **matcomps.cpp**, **material.cpp**, **material.hpp**, **mat_dump.cpp**: Starfield CDB and material database support (class CDBFile and CE2MaterialDB).
* **common.cpp**, **common.hpp**: Various common functions, common.hpp is included by all other source files.
* **ddstxt.cpp**, **ddstxt.hpp**: class DDSTexture: DDS texture reader, supports decoding most common pixel formats to R8G8B8A8, bilinear and trilinear filtering, and cube maps.
* **downsamp.cpp**, **downsamp.hpp**: Image downsampler for 4x and 16x SSAA implementation.
* **esmfile.cpp**, **esmfile.hpp**: class ESMFile: ESM file reader.
* **filebuf.cpp**, **filebuf.hpp**: class FileBuffer: general file input, can be used to memory map files, or to read memory buffers using the same interface. The same source files also include code for writing uncompressed DDS images.
* **fp32vec4.hpp**: class FloatVector4: fast vector math using AVX instructions if available.
* **frtable.cpp**: Tables for Fresnel approximation using polynomials.
* **mat_json.cpp**, **mat_json.hpp**: Starfield material database to JSON conversion (class CDBMaterialToJSON).
* **mman.c**, **mman.h**: [mman-win32](https://github.com/alitrack/mman-win32) for memory mapping on Windows.
* **sdlvideo.cpp**, **sdlvideo.hpp**, **courb24.cpp**: class SDLDisplay: video output, keyboard/mouse input, and console using SDL 2. Enabled only if compiled with the macro HAVE\_SDL2 defined. Supports full screen mode and downsampling from higher than the native display resolution.
* **stringdb.cpp**, **stringdb.hpp**: class StringDB: support for reading Creation Engine strings files.
* **viewrtbl.cpp**: Tables of common view transformations used by the NIF and world space viewers.
* **zlib.cpp**, **zlib.hpp**: class ZLibDecompressor, decodes zlib, LZ4 and headerless LZ4 streams.

All source code is under the MIT license.

