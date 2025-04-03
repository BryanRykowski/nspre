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

#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <functional>
#include <type_traits>

#define NSPRE_VERSION_MAJOR 1
#define NSPRE_VERSION_MINOR 0
#define NSPRE_VERSION_MINOR_MINOR 0

namespace nspre
{

enum Error : int {
	UNINITIALIZED = -1,
	NO_ERROR = 0,
	FILE_OPEN = 1,
	READ_HEADER = 2,
	READ_SUBHEADER = 3,
	READ_SUBPATH = 4,
	READ_SUBFILE = 256,
	EXTRACT_SUBFILE = 257,
	FILE_OPEN_OUTPUT = 258,
	WRITE_HEADER = 65536,
	WRITE_SUBHEADER = 65537,
	WRITE_SUBPATH = 65538,
	WRITE_SUBFILE = 65539
};

class SubfileBase {
protected:
	std::string m_prepath;
public:
	SubfileBase(const std::string& i_prepath) : m_prepath(i_prepath) {}
	std::string& prepath();
	std::string filename() const;
};

class Reader {
public:
	typedef std::function<int (char*,size_t)> Outfunc;
	class Subfile : public SubfileBase {
		std::ifstream& stream;
		char m_subheader[16];
		int m_cmp_size;
		int m_size;
		int m_offset;
		int extract(Outfunc& outfunc);
	public:
		Subfile(std::ifstream& i_stream, const char* i_subheader, const std::string& i_prepath, int i_offset);
		int cmp_size() const;
		int size() const;
		int offset() const;
		std::vector<char> subheader(); 
		int extract(char* data_out);
		int extract(std::vector<char>& data_out);
		int extract(const std::filesystem::path& path);
	};
private:
	std::ifstream stream;
	std::vector<Subfile> m_files;
	char m_header[12];
	int m_size;
	int m_error = Error::UNINITIALIZED;
public:
	Reader(const std::filesystem::path& path);
	std::vector<Subfile>& files();
	int size();
	std::vector<char> header();
	int error();
};

struct Subfile : public SubfileBase {
	std::filesystem::path source;
	Subfile(const std::filesystem::path& i_source, const std::string& i_prepath);
};

int write(Subfile* subfiles, size_t count, const std::filesystem::path& path);
int write(std::vector<Subfile>& subfiles, const std::filesystem::path& path);

#ifdef NSPRE_IMPL
std::string SubfileBase::filename() const {
	if (m_prepath.size() < 1) return std::string{""};

	int start = m_prepath.size() - 1;

	for (int i = m_prepath.size() - 1; i >= 0; --i) {
		if (m_prepath[i] == '\\') break;
		start = i;
	}

	return m_prepath.substr(start, m_prepath.npos);
}

std::string& SubfileBase::prepath() {
	return m_prepath;
}

Subfile::Subfile(const std::filesystem::path& i_source, const std::string& i_prepath) :
	source(i_source),
	SubfileBase(i_prepath)
{}

template <typename T>
T Read32LE(const char* buffer) {
	static_assert(std::is_integral_v<T>, "Read32LE requires integral T");
	static_assert(sizeof(T) >= 4, "Read32LE requires T >= 32 bits");
	if (buffer == nullptr) {
		return 0;
	}

	T out;
    out  = static_cast<unsigned char>(buffer[0]);
    out |= static_cast<unsigned char>(buffer[1]) << 8;
    out |= static_cast<unsigned char>(buffer[2]) << 16;
    out |= static_cast<unsigned char>(buffer[3]) << 24;
	
	return out;
}

template<typename T>
void Write32LE(char* buffer, T in) {
	static_assert(std::is_integral_v<T>, "Write32LE requires integral T");
	static_assert(sizeof(T) >= 4, "Write32LE requires T >= 32 bits");
	buffer[0] = static_cast<char>(in & 0xff);
	buffer[1] = static_cast<char>((in >> 8) & 0xff);
	buffer[2] = static_cast<char>((in >> 16) & 0xff);
	buffer[3] = static_cast<char>((in >> 24) & 0xff);
}

std::vector<Reader::Subfile>& Reader::files() {
	return m_files;
}

int Reader::size() {
	return m_size;
}

std::vector<char> Reader::header() {
	std::vector<char> v(12);
	std::copy(m_header, m_header + 12, v.begin());
	return v;
}

int Reader::error() {
	return m_error;
}

Reader::Reader(const std::filesystem::path& path) {
	stream.open(path, std::ios::binary);
	if (stream.fail()) {
		m_error = Error::FILE_OPEN;
		return;
	}

	stream.read(m_header, 12);
	if (stream.fail()) {
		m_error = Error::READ_HEADER;
		return;
	}

	// Pre file header layout:
	// Size Description

	// 4    Total file size
	// 2    Version?
	// 2    Unknown
	// 4    Number of subfiles
	
	m_size = Read32LE<int>(m_header);
	int count = Read32LE<int>(m_header + 8);

	for (int i = 0; i < count; ++i) {
		char subheader[16];
		stream.read(subheader, 16);
		if (stream.fail()) {
			m_error = Error::READ_SUBHEADER;
			return;
		}

		// Subfile header layout:
		// Size Description

		// 4    Uncompressed file size
		// 4    Compressed file size
		// 4    Path string size
		// 4    Path string checksum
		// n    Path string (size is a multiple of 4)

		int path_size = Read32LE<int>(&subheader[8]); // Path string length is always a multiple of 4 bytes.
		std::vector<char> path_bytes(path_size);
		stream.read(&path_bytes[0], path_size);
		if (stream.fail()) {
			m_error = Error::READ_SUBPATH;
			return;
		}

		std::string prepath(path_bytes.begin(), path_bytes.end());
		Subfile subfile(stream, subheader, prepath, stream.tellg());
		m_files.push_back(subfile);

		int file_size = subfile.cmp_size() ? subfile.cmp_size() : subfile.size(); // If the compressed size is 0 the file is uncompressed.
		int padding = (file_size % 4) ? 4 - (file_size % 4) : 0; // Files that are not a multiple of 4 bytes in size have padding at the end to maintain alignment.
		stream.ignore(file_size + padding);
	}

	m_error = 0;
}

int Reader::Subfile::cmp_size() const {
	return m_cmp_size;
}

int Reader::Subfile::size() const {
	return m_size;
}

int Reader::Subfile::offset() const {
	return m_offset;
}

std::vector<char> Reader::Subfile::subheader() {
	std::vector<char> v;
	std::copy(m_subheader, m_subheader + 16, v.begin());
	return v;
} 

Reader::Subfile::Subfile(std::ifstream& i_stream, const char* i_subheader, const std::string& i_prepath, int i_offset) :
	SubfileBase(i_prepath),
	stream(i_stream),
	m_offset(i_offset) 
{
	m_size = Read32LE<int>(i_subheader);
	m_cmp_size = Read32LE<int>(i_subheader + 4);
	std::copy(i_subheader, i_subheader + 16, m_subheader);
}

#ifndef NSPRE_CHUNK_SIZE
#define NSPRE_CHUNK_SIZE 1024
#endif

struct ExtractVars {
	int in_pos = 0;
	int rb_pos = 4078;
	std::vector<char> ring_buffer;

	ExtractVars() : ring_buffer(4096, 0) {}
};

static int CopyByte(ExtractVars& vars, std::ifstream& stream, Reader::Outfunc& outfunc) {
	char b;
	stream.read(&b, 1);
	if (stream.fail()) {
		return Error::READ_SUBFILE;
	}

	if (int err = outfunc(&b, 1)) {
		return err;
	}

	vars.ring_buffer[vars.rb_pos] = b;
	++vars.in_pos;
	vars.rb_pos = (vars.rb_pos + 1) % 4096;

	return Error::NO_ERROR;
}

static int CopyRB(ExtractVars& vars, std::ifstream& stream, Reader::Outfunc& outfunc) {
	char dict[2];
	stream.read(dict, 2);
	if (stream.fail()) {
		return Error::READ_SUBFILE;
	}

	vars.in_pos += 2;

	unsigned int offset, count;

	offset = static_cast<unsigned char>(dict[0]);
	offset |= ((static_cast<unsigned char>(dict[1]) & 0xf0) << 4);
	count = (static_cast<unsigned char>(dict[1]) & 0xf) + 3;

	for (int i = 0; i < count; ++i) {
		char b = vars.ring_buffer[(offset + i) % 4096];
		vars.ring_buffer[vars.rb_pos] = b;
		vars.rb_pos = (vars.rb_pos + 1) % 4096;

		if (int err = outfunc(&b, 1)) {
			return err;
		}
	}

	return Error::NO_ERROR;
}

int Reader::Subfile::extract(Outfunc& outfunc) {
	if (!stream.is_open()) {
		return Error::UNINITIALIZED;
	}

	stream.seekg(m_offset);
	if (stream.fail()) {
		std::fprintf(stderr,"seek fail\n");
		return Error::READ_SUBFILE;
	}

	// If cmp_size is 0 the file is uncompressed and can just be copied.
	if (m_cmp_size == 0) {
		std::vector<char> buffer(NSPRE_CHUNK_SIZE);
		int bytes = m_size;
		
		while (bytes > 0) {
			if (bytes >= NSPRE_CHUNK_SIZE) {
				stream.read(buffer.data(), NSPRE_CHUNK_SIZE);
				if (stream.fail()) {
					return Error::READ_SUBFILE;
				}

				if (int err = outfunc(buffer.data(), NSPRE_CHUNK_SIZE)) {
					return err;
				}

				bytes -= NSPRE_CHUNK_SIZE;
			}
			else {
				stream.read(buffer.data(), bytes);
				if (stream.fail()) {
					return Error::READ_SUBFILE;
				}

				if (int err = outfunc(buffer.data(), bytes)) {
					return err;
				}

				break;
			}
		}

		return Error::NO_ERROR;
}
	// Compressed subfiles in a prefile are made up of structures consisting of a
	// "type byte" followed by a combination of 8 "literal bytes" and "ring buffer lookups".
	
	// type byte:          1 byte
	// literal byte:       1 byte
	// ring buffer lookup: 2 bytes
	
	// Layout:
	//     tb = type byte
	//     x = literal byte or ring buffer lookup
	//     total size: 9-17 bytes
	//     [[tb][x][x][x][x][x][x][x][x]]
	
	// The 8 bits of a type byte indicate the mix of literal bytes and ring buffer lookups to
	// follow. The byte is read from least to most significant bit. A 1 indicates a literal
	// byte and a 0 indicates a ring buffer lookup.

	// Example:
	//     type byte: 00011111
	//     5 literal bytes followed by 3 ring buffer lookups.
	//     total size: 12 bytes (1 + (5 * 1) + (3 * 2))

	// The ring buffer is 4096 bytes and is written to starting at offset 4078. Once you reach the
	// end of the buffer, you start writing at offset 0. Files larger than 4 KiB will overwrite
	// previous data, providing a view of at most the last 4 KiB of the file.

	// All bytes written to the output file are also written to the ring buffer.

	// A ring buffer lookup is 2 bytes and provides 2 values, the offset and the length.
	// offset: A 12 bit value indicating where to begin copying from the ring buffer.
	// length: A 4 bit value indicating how many bytes to copy from the ring buffer.

	// Layout:
	//     [byte 0] [byte 1]
	//     aaaaaaaa bbbbcccc
	//     offset: bbbbaaaaaaaa
	//     length:         cccc

	// The length has 3 added to it resulting in a range of 3-18 instead of 0-15.

	ExtractVars vars;
	char type_byte = 0;

	while (vars.in_pos < m_cmp_size) {
		stream.read(&type_byte, 1);
		if (stream.fail()) {
			return Error::READ_SUBFILE;
		}

		++vars.in_pos;

		for (int i = 0; i < 8; ++i) {
			if (static_cast<unsigned char>(type_byte) & (0x1 << i)) {
				if (int err = CopyByte(vars, stream, outfunc)) {
					return err;
				}
				if (vars.in_pos >= m_cmp_size) break;
			}
			else {
				if (int err = CopyRB(vars, stream, outfunc)) {
					return err;
				}
				if (vars.in_pos >= m_cmp_size) break;
			}
		}
	}

	return Error::NO_ERROR;
}

int Reader::Subfile::extract(char* data_out) {
	size_t pos = 0;
	Outfunc outfunc = [&data_out, &pos](char* data, size_t count) {
		std::copy(data, data + count, data_out + pos);
		pos += count;
		return Error::NO_ERROR;
	};

	return extract(outfunc);
}

int Reader::Subfile::extract(std::vector<char>& data_out) {
	data_out.resize(m_size);
	return extract(data_out.data());
}

int Reader::Subfile::extract(const std::filesystem::path& path) {
	std::ofstream ostream(path, std::ios::binary);
	if (ostream.fail()) {
		return Error::FILE_OPEN_OUTPUT;
	}

	Outfunc outfunc = [&ostream](char* data, size_t count) {
		ostream.write(data, count);
		if (ostream.fail()) {
			return Error::EXTRACT_SUBFILE;
		}

		return Error::NO_ERROR;
	};

	return extract(outfunc);
}

static const unsigned int crc_table[] =
{
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
	0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
	0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
	0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
	0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
	0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
	0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
	0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
	0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
	0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
	0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
	0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
	0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
	0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
	0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
	0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
	0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
	0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
	0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
	0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
	0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
	0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
	0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
	0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
	0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
	0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
	0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
	0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
	0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
	0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
	0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
	0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
	0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
	0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
	0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
	0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
	0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
	0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
	0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static const unsigned int CRC_START = 0xffffffff;

static unsigned int string_crc(const std::string &str)
{
	unsigned int crc = CRC_START;

	for (char c : str)
	{
		unsigned int table_val = crc_table[static_cast<unsigned char>(crc) ^ static_cast<unsigned char>(c)];
		crc = table_val ^ (crc >> 8);
	}

	return crc;
}

static unsigned int buffer_crc(const char *buffer, unsigned int size)
{
	unsigned int crc = CRC_START;

	for (unsigned int i = 0; i < size; ++i)
	{
		unsigned int table_val = crc_table[static_cast<unsigned char>(crc) ^ static_cast<unsigned char>(buffer[i])];
		crc = table_val ^ (crc >> 8);
	}

	return crc;
}

static int write_subfile(std::ofstream& ostream, Subfile& subfile) {
	std::ifstream stream(subfile.source, std::ios::binary);
	if (stream.fail()) {
		return Error::FILE_OPEN;
	}

	std::vector<char> file_buffer;
	const size_t read_size = 1048576;
	size_t total_count = 0;
	size_t last_count = 0;
	while (!stream.eof()) {
		file_buffer.resize(total_count + read_size);
		stream.read(file_buffer.data() + total_count, read_size);
		last_count = stream.gcount();
		total_count += last_count;
		if (last_count < read_size) {
			file_buffer.resize(total_count);
			break;
		}
	}

	std::vector<char> path_buffer(subfile.prepath().size());
	std::copy(subfile.prepath().begin(), subfile.prepath().end(), path_buffer.begin());

	// Convert forward slashes to back slashes just in case.
	for (char& c : path_buffer) {
		if (c == '/') c = '\\';
	}

	// Even if the path string ends up being a multiple of 4 we need to pad it because there
	// needs to be a null at the end.
	int padding = 4 - (path_buffer.size() % 4);
	for (int i = 0; i < padding; ++i) {
		path_buffer.push_back(0);
	}

	char header[16]{};
	Write32LE<int>(header, file_buffer.size());
	Write32LE<int>(header + 4, 0);
	Write32LE<int>(header + 8, path_buffer.size());
	Write32LE<unsigned int>(header + 12, string_crc(subfile.prepath()));

	ostream.write(header, 16);
	if (ostream.fail()) {
		return Error::WRITE_SUBHEADER;
	}

	ostream.write(path_buffer.data(), path_buffer.size());
	if (ostream.fail()) {
		return Error::WRITE_SUBPATH;
	}

	// Pad end of file to maintain alignment.
	padding = (file_buffer.size() % 4) ? 4 - (file_buffer.size() % 4) : 0;
	for (int i = 0; i < padding; ++i) {
		file_buffer.push_back(0);
	}

	ostream.write(file_buffer.data(), file_buffer.size());
	if (ostream.fail()) {
		return Error::WRITE_SUBFILE;
	}

	return Error::NO_ERROR;
}

int write(Subfile* subfiles, size_t count, const std::filesystem::path& path) {

	std::ofstream ostream(path, std::ios::binary);
	if (ostream.fail()) {
		return Error::FILE_OPEN_OUTPUT;
	}

	char header[12];
	header[4] = 0x03;
	header[5] = 0x0;
	header[6] = 0xcd;
	header[7] = 0xab;
	Write32LE<int>(header + 8, count);

	ostream.write(header, 12);
	if (ostream.fail()) {
		return Error::WRITE_HEADER;
	}

	for (int i = 0; i < count; ++i) {
		if (int err = write_subfile(ostream, subfiles[i])) {
			return err;
		}
	}

	Write32LE<unsigned int>(header, static_cast<unsigned int>(ostream.tellp()));
	ostream.seekp(0);
	ostream.write(header, 12);
	if (ostream.fail()) {
		return Error::WRITE_HEADER;
	}

	return Error::NO_ERROR;
}

int write(std::vector<Subfile>& subfiles, const std::filesystem::path& path) {
	return write(subfiles.data(), subfiles.size(), path);
}

#endif
}
