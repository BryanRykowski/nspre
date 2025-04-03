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
#include <cstring>

std::vector<nspre::Subfile> in_files;
std::filesystem::path out_file = "out.pre";

void print_help() {
	std::printf(
		"ns-pack - Create pre file from list of files.\n"
		"Usage: ns-pack [OPTIONS] [FILE LIST]\n"
		"  -o  Output file. Default is ./out.pre\n"
		"  -h  Show this help message\n"
		"\n"
		"File list format:\n"
		"  path_1,internal\\\\path\\\\1:path_2,internal\\\\path\\\\2:path_3,internal\\\\path\\\\3\n"
		"\n"
	);
}

int parse_filelist(std::string list) {
	std::vector<std::string> vals;

	size_t last_pos = 0;
	size_t pos = 0;
	while (pos != list.npos) {
		if (last_pos >= list.size()) break;
		pos = list.find(':', last_pos);
		if (pos == list.npos) {
			vals.push_back(list.substr(last_pos, list.size() - last_pos));
			break;
		}
		if (pos - last_pos > 0) {
			vals.push_back(list.substr(last_pos, pos - last_pos));
		}
		last_pos = pos + 1;
	}

	for (std::string s : vals) {
		size_t split = s.find(',');
		if (split == 0 || split == s.size() - 1 || split == s.npos) continue;
		auto filepath = s.substr(0, split);
		auto prepath = s.substr(split + 1, s.size() - split);
		in_files.emplace_back(filepath,prepath);
	}

	return 0;
}

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i) {
		bool has_val = false;
		if (argc > i + 1) has_val = true;
		if (has_val && std::strcmp(argv[i], "-o") == 0) {
			out_file = argv[i + 1];
			++i;
		}
		else if (std::strcmp(argv[i], "-h") == 0) {
			print_help();
			return 0;
		}
		else {
			parse_filelist(argv[i]);
		}
	}

	if (!in_files.size()) {
		std::fprintf(stderr, "no input files\n");
		print_help();
		return -1;
	}

	if (int err = nspre::write(in_files, out_file)) {
		switch (err) {
		case nspre::Error::FILE_OPEN:
			std::fprintf(stderr, "can't open input file\n");
			break;
		case nspre::Error::FILE_OPEN_OUTPUT:
			std::fprintf(stderr, "can't open output file\n");
			break;
		case nspre::Error::WRITE_SUBHEADER:
		case nspre::Error::WRITE_SUBPATH:
		case nspre::Error::WRITE_SUBFILE:
			std::fprintf(stderr, "error writing file\n");
			break;
		default:
			std::fprintf(stderr, "error (%d)\n", err);
		}
		return err;
	}

	return 0;
}
