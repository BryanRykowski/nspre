// Copyright (c) 2025 Bryan Rykowski
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define NSPRE_IMPL
#include "nspre.hpp"
#include <cstdio>
#include <cstring>

std::filesystem::path inpath;
std::filesystem::path outdir;

bool quiet = false;
bool file_details = false;
bool comma_separated = false;
bool dry_run = false;

void print_help() {
	std::printf(
		"ns-unpack - Extract files from pre file.\n"
		"Usage: ns-unpack [OPTIONS] [INPUT FILE]\n"
		"  -o  Output directory - Default is ./\n"
		"  -n  Dry run - Don't write output files\n"
		"  -v  Show details - Show name, path, compressed size, and actual size of each file\n"
		"  -c  Show details with commas separating values instead of spaces\n"
		"  -q  Quiet - Don't show total size and number of files\n"
		"  -h  Show this help message\n"
		"\n"
	);
}

bool arg_proc(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		bool has_val = false;
		if (argc > i + 1) has_val = true;

		if (std::strlen(argv[i]) > 1 && argv[i][0] == '-') {
			if (std::strchr(argv[i], 'h')) {
				print_help();
				return true;
			}
			if (std::strchr(argv[i], 'n')) {
				dry_run = true;
			}
			if (std::strchr(argv[i], 'v')) {
				file_details = true;
			}
			if (std::strchr(argv[i], 'c')) {
				file_details = true;
				comma_separated = true;
			}
			if (std::strchr(argv[i], 'q')) {
				quiet = true;
			}
			if (has_val && std::strchr(argv[i], 'o')) {
				outdir = argv[i + 1];
				++i;
			}
		}
		else {
			inpath = argv[i];
		}
	}

	return false;
}

int main(int argc, char** argv) {
	if (arg_proc(argc, argv)) {
		return 0;
	}

	if (inpath.empty()) {
		std::fprintf(stderr, "no input file\n");
		print_help();
		return -1;
	}

	nspre::Reader reader(inpath);
	if (reader.error()) {
		std::fprintf(stderr, "can't open input file\n");
		return reader.error();
	}

	if (!quiet) {
		std::printf("size: %d\nfiles: %zu\n", reader.size(), reader.files().size());
	}
	
	if (file_details) {
		char c = comma_separated ? ',' : ' ';
		for (int i = 0; i < reader.files().size(); ++i) {
			std::printf(
				"%s%c%s%c%d%c%d\n",
				reader.files()[i].filename().c_str(),
				c,
				reader.files()[i].prepath().c_str(),
				c,
				reader.files()[i].cmp_size(),
				c,
				reader.files()[i].size()
			);
		}
	}

	if (!dry_run) {
		for (int i = 0; i < reader.files().size(); ++i) {
			if (int err = reader.files()[i].extract(outdir / reader.files()[i].filename())) {
				switch (err) {
				case nspre::Error::FILE_OPEN_OUTPUT:
					std::fprintf(stderr, "can't open output file\n");
					break;
				case nspre::Error::READ_HEADER:
				case nspre::Error::READ_SUBHEADER:
				case nspre::Error::READ_SUBPATH:
				case nspre::Error::READ_SUBFILE:
				case nspre::Error::EXTRACT_SUBFILE:
					std::fprintf(stderr, "error reading file %d\n", i);
					break;
				default:
					std::fprintf(stderr, "error (%d)\n", err);
				}
				return err;
			}
		}
	}

	return 0;
}
