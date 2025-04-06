# NSPRE
A single header c++ library for reading and writing Neversoft pre/prx files.

## Including the library
As a single header library, the declarations and definitions in this library are in the same file. To avoid violating the [one definition rule](https://en.wikipedia.org/wiki/One_Definition_Rule) you need to write **`#define NSPRE_IMPL`** before **`#include "nspre.hpp"`** in **only one source file**. This will put the definitions for all the classes and functions in that source file. You can `#include "nspre.hpp"` as usual in any other file.

## ns-unpack and ns-pack
These are example programs to demonstrate basic use of the library.

Build using cmake:
```
cd nspre
mkdir build
cmake -S . -B build/
cmake --build build/
```

Programs can be found at `build/pack/ns-pack` and `build/unpack/ns-unpack`.

# Documentation
## Classes
## `nspre::Reader`
A class representing a successfully opened pre/prx file.

#### `Reader::Reader(const std::filesystem::path& path)`
Constructor for the Reader class.

`path:` Path to the pre/prx file to be read

#### `Reader::Reader()`
Constructor for the Reader class. You must use Reader::open() before you can do anything.

#### `int Reader::open(const std::filesystem::path& path)`
Open a pre/prx file. Equivalent to the Reader(const std::filesystem::path& path) constructor.

`path:` Path to the pre/prx file to be read

#### `void Reader::close()`
Reset a Reader to an uninitialized state.

#### `std::vector<Subfile>& Reader::files()`
Returns the [Subfiles](#nsprereadersubfile) from a successfully opened pre/prx file. Will be an empty vector if opening the file failed.

#### `int Reader::size()`
Returns the total file size as recorded in the file.

#### `std::vector<char> Reader::header()`
Returns a vector containing the raw 12 byte header of the file.

#### `int Reader::error()`
Returns the error state of the reader. Anything other than 0 indicates failure to open the file.

## `nspre::Reader::Subfile`
A subfile within the pre/prx file.

#### `std::string& Reader::Subfile::prepath()`
Returns a reference to the internal path recorded in the Subfile.

#### `std::string Reader::Subfile::filename()`
Returns the filename from the internal path.

#### `int Reader::Subfile::cmp_size()`
Returns the compressed size of the file if it is compressed. Returns 0 if it is uncompressed.

#### `int Reader::Subfile::size()`
Returns the actual size of the file.

#### `std::vector<char> Reader::Subfile::subheader()`
Returns a vector containing the raw 16 byte header of the Subfile.

#### `int Reader::Subfile::offset()`
Returns the offset to the start of the Subfile data.

#### `int Reader::Subfile::extract(char* data_out)`
Decompress the file if necessary and copy it to a char array. Size of the array must be greater than or equal to the value returned by size(). Returns 0 on success.

#### `int Reader::Subfile::extract(std::vector<char>& data_out)`
Decompress the file if necessary and copy it to a char vector. Vector will automatically be resized to appropriate size and overwritten. Returns 0 on success.

#### `int Reader::Subfile::extract(const std::filesystem::path& path)`
Decompress the file if necessary and write it to a file. Returns 0 on success.

## `nspre::Subfile`
Represents an external file and its associated internal path to be included in a pre file.

#### `Subfile::Subfile(const std::filesystem::path& i_source, const std::string& i_prepath)`
Constructor for the Subfile class.

`i_source:` Path to the file on disk

`i_prepath:` Internal path to be recorded in Subfile

#### `std::string& Subfile::prepath()`
Returns a reference to the internal path recorded in the Subfile.

#### `std::string Subfile::filename()`
Returns the filename from the internal path.

#### `Subfile::source`
The path to the file on disk.

## Functions

#### `int write(Subfile* subfiles, size_t count, const std::filesystem::path& path)`
Writes a list of files to a pre file. Can't perform compression. Returns 0 on success.

`subfiles:` Pointer to an array of [Subfiles](#nspresubfile)

`count:` Number of Subfiles

`path:` File to write to

#### `int write(std::vector<Subfile>& subfiles, const std::filesystem::path& path)`
Writes a list of files to a pre file. Can't perform compression. Returns 0 on success.

`subfiles:` Vector of [Subfiles](#nspresubfile)

`path:` File to write to