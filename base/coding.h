// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//

#ifndef __BT_CODING_H__
#define __BT_CODING_H__

#include <boost/cstdint.hpp>
using namespace boost;
#include <string.h>

// Pointer-based variants of GetVarint...  These either store a value
// in *v and return a pointer just past the parsed value, or return
// NULL on error.  These routines only look at bytes in the range
// [p..limit-1]
extern const char* GetVarint32Ptr(const char* p,const char* limit, uint32_t* v);
extern const char* GetVarint64Ptr(const char* p,const char* limit, uint64_t* v);

// Returns the length of the varint32 or varint64 encoding of "v"
inline int VarintLength(uint64_t v)
{
	int len = 1;
	while (v >= 128) {
		v >>= 7;
		len++;
	}
	return len;
}

// Lower-level versions of Put... that write directly into a character buffer
// REQUIRES: dst has enough space for the value being written
inline void EncodeFixed32(char* buf, uint32_t value)
{
#ifndef __BIG_ENDIAN__
	memcpy(buf, &value, sizeof(value));
#else
	buf[0] = value & 0xff;
	buf[1] = (value >> 8) & 0xff;
	buf[2] = (value >> 16) & 0xff;
	buf[3] = (value >> 24) & 0xff;
#endif
}

inline void EncodeFixed64(char* buf, uint64_t value)
{
#ifndef __BIG_ENDIAN__
	memcpy(buf, &value, sizeof(value));
#else
	buf[0] = value & 0xff;
	buf[1] = (value >> 8) & 0xff;
	buf[2] = (value >> 16) & 0xff;
	buf[3] = (value >> 24) & 0xff;
	buf[4] = (value >> 32) & 0xff;
	buf[5] = (value >> 40) & 0xff;
	buf[6] = (value >> 48) & 0xff;
	buf[7] = (value >> 56) & 0xff;
#endif
}

// Lower-level versions of Put... that write directly into a character buffer
// and return a pointer just past the last byte written.
// REQUIRES: dst has enough space for the value being written
inline char* EncodeVarint32(char* dst, uint32_t v)
{
	// Operate on characters as unsigneds
	unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
	static const int B = 128;
	if (v < (1<<7)) {
		*(ptr++) = v;
	} 
	else if (v < (1<<14)) {
		*(ptr++) = v | B;
		*(ptr++) = v>>7;
	} 
	else if (v < (1<<21)) {
		*(ptr++) = v | B;
		*(ptr++) = (v>>7) | B;
		*(ptr++) = v>>14;
	} 
	else if (v < (1<<28)) {
		*(ptr++) = v | B;
		*(ptr++) = (v>>7) | B;
		*(ptr++) = (v>>14) | B;
		*(ptr++) = v>>21;
	}
	else {
		*(ptr++) = v | B;
		*(ptr++) = (v>>7) | B;
		*(ptr++) = (v>>14) | B;
		*(ptr++) = (v>>21) | B;
		*(ptr++) = v>>28;
	}
	return reinterpret_cast<char*>(ptr);
}

inline char* EncodeVarint64(char* dst, uint64_t v)
{
	static const uint64_t B = 128;
	unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
	while (v >= B) {
		*(ptr++) = (v & (B-1)) | B;
		v >>= 7;
	}
	*(ptr++) = static_cast<unsigned char>(v);
	return reinterpret_cast<char*>(ptr);
}

// Lower-level versions of Get... that read directly from a character buffer
// without any bounds checking.

inline uint32_t DecodeFixed32(const char* ptr)
{
#ifndef __BIG_ENDIAN__
    uint32_t result;
    memcpy(&result, ptr, sizeof(result));
    return result;
#else
    return ((static_cast<uint32_t>(static_cast<unsigned char>(ptr[0])))
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[2])) << 16)
        | (static_cast<uint32_t>(static_cast<unsigned char>(ptr[3])) << 24));
#endif
}

inline uint64_t
DecodeFixed64(const char* ptr)
{
#ifndef __BIG_ENDIAN__ 
	uint64_t result;
	memcpy(&result, ptr, sizeof(result));
	return result;
#else
    uint64_t lo = DecodeFixed32(ptr);
    uint64_t hi = DecodeFixed32(ptr + 4);
    return (hi << 32) | lo;
#endif
}

// Internal routine for use by fallback path of GetVarint32Ptr
inline const char*
GetVarint32PtrFallback(const char* p, const char* limit, uint32_t* value)
{
	uint32_t result = 0;
	for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
		uint32_t byte = *(reinterpret_cast<const unsigned char*>(p));
		p++;
		if (byte & 128) {
			// More bytes are present
			result |= ((byte & 127) << shift);
		} 
		else {
			result |= (byte << shift);
			*value = result;
			return reinterpret_cast<const char*>(p);
		}
	}
	return NULL;
}

inline const char*
GetVarint32Ptr(const char* p, const char* limit, uint32_t* value)
{
  if (p < limit) {
    uint32_t result = *(reinterpret_cast<const unsigned char*>(p));
    if ((result & 128) == 0) {
      *value = result;
      return p + 1;
    }
  }
  return GetVarint32PtrFallback(p, limit, value);
}

inline const char*
GetVarint64Ptr(const char* p, const char* limit, uint64_t* value)
{
	uint64_t result = 0;
	for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
		uint64_t byte = *(reinterpret_cast<const unsigned char*>(p));
		p++;
		if (byte & 128) {
			// More bytes are present
			result |= ((byte & 127) << shift);
		} 
		else {
			result |= (byte << shift);
			*value = result;
			return reinterpret_cast<const char*>(p);
		}
	}
	return NULL;
}

inline uint64_t 
encodeZigzag64(int64_t input)
{
	return ((input << 1) ^ (input >> 63));
}

inline int64_t 
decodeZigzag64(uint64_t input)
{
	return static_cast<int64_t>(((input >> 1) ^ -(static_cast<int64_t>(input) & 1)));
}

inline uint32_t 
encodeZigzag32(int32_t input)
{
	return ((input << 1) ^ (input >> 31));
}

inline int32_t 
decodeZigzag32(uint32_t input)
{
	return static_cast<int32_t>(((input >> 1) ^ -(static_cast<int64_t>(input) & 1)));
}

#endif

