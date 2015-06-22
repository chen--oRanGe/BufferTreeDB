#ifndef __BT_BUFFER_H
#define __BT_BUFFER_H
#include <algorithm>
#include <vector>

#include <assert.h>
#include <string.h>

#include "coding.h"

class Buffer
{
public:
	static const size_t kCheapPrepend = 8;
	static const size_t kInitialSize = 1024;

	Buffer()
		: buffer_(kCheapPrepend + kInitialSize),
		  readerIndex_(kCheapPrepend),
		  writerIndex_(kCheapPrepend)
	{
		assert(readableBytes() == 0);
		assert(writableBytes() == kInitialSize);
		assert(prependableBytes() == kCheapPrepend);
	}

	void swap(Buffer& rhs)
	{
		buffer_.swap(rhs.buffer_);
		std::swap(readerIndex_, rhs.readerIndex_);
		std::swap(writerIndex_, rhs.writerIndex_);
	}

	size_t readableBytes() const
	{ return writerIndex_ - readerIndex_; }

	size_t writableBytes() const
	{ return buffer_.size() - writerIndex_; }

	size_t prependableBytes() const
	{ return readerIndex_; }

	char* peek()
	{ return begin() + readerIndex_; }

/*
	const char* findCRLF()
	{
		const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
		return crlf == beginWrite() ? NULL : crlf;
	}

	const char* findCRLF(const char* start) const
	{
		assert(peek() <= start);
		assert(start <= beginWrite());
		const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
		return crlf == beginWrite() ? NULL : crlf;
	}

	const char* findEOL()
	{
		const void* eol = memchr(peek(), '\n', readableBytes());
		return static_cast<const char*>(eol);
	}

	const char* findEOL(const char* start) const
	{
		assert(peek() <= start);
		assert(peek() <= start);
		const void* eol = memchr(start, '\n', readableBytes());
		return static_cast<const char*>(eol);
	}
*/

	void retrieve(size_t len)
	{
		assert(len <= readableBytes());
		if(len < readableBytes()) {
			readerIndex_ += len;
		} else {
			retrieveAll();
		}
	}

	void retrieveInt8()
	{
	  	retrieve(sizeof(int8_t));
	}

	void retrieveInt32()
	{
		retrieve(sizeof(int32_t));
	}

	void retrieveInt64()
	{
		retrieve(sizeof(int64_t));
	}

	void retrieveAll()
	{
		readerIndex_ = kCheapPrepend;
		writerIndex_ = kCheapPrepend;
	}

	// write
	void append(const char* data, size_t len)
	{
		ensureWritableBytes(len);
		std::copy(data, data + len, beginWrite());
		hasWritten(len);
	}

	void appendString(std::string& str)
	{
		append(str.data(), str.size());
	}

	void ensureWritableBytes(size_t len)
	{
		if(writableBytes() <  len) {
			makeSpace(len);
		}
		assert(writableBytes() >= len);
	}

	char* beginWrite()
	{ return begin() + writerIndex_; }

/*
	const char* beginWrite()
	{ return begin() + writerIndex_; }
*/

	void hasWritten(size_t len)
	{ writerIndex_ += len; }

	void appendInt8(int8_t x)
	{
		append((char*)&x, sizeof(x));
	}

	void appendInt32(int32_t x)
	{
		int32_t be32;
		EncodeFixed32((char*)&be32, x);
		append((char*)&be32, sizeof(be32));
	}

	void appendInt64(int64_t x)
	{
		int64_t be64;
		EncodeFixed64((char*)&be64, x);
		append((char*)&be64, sizeof(be64));
	}

	// read
	void read(char* data, size_t len)
	{
		assert(readableBytes() >= len);
		::memcpy(data, peek(), len);
		retrieve(len);
	}
	
	uint8_t readInt8()
  	{
    	uint8_t result = peekInt8();
    	retrieveInt8();
    	return result;
  	}
	int32_t readInt32()
	{
		int32_t result = peekInt32();
		retrieveInt32();
		return result;
	}

	int64_t readInt64()
	{
		int64_t result = peekInt64();
		retrieveInt64();
		return result;
	}

	std::string readString()
	{
		int32_t len = readInt32();
		std::string str = peekString(len);
		retrieve(len);
		return str;
	}

	// peek
	uint8_t peekInt8()
  	{
    	assert(readableBytes() >= sizeof(uint8_t));
    	uint8_t x = *peek();
    	return x;
  	}
	
	uint32_t peekInt32()
	{
		assert(readableBytes() >= sizeof(int32_t));
		uint32_t be32 = 0;
		::memcpy(&be32, peek(), sizeof(be32));
		return DecodeFixed32((char*)&be32);
	}

	uint64_t peekInt64()
	{
		assert(readableBytes() >= sizeof(int64_t));
		uint64_t be64 = 0;
		::memcpy(&be64, peek(), sizeof(be64));
		return DecodeFixed64((char*)&be64);
	}

	std::string peekString(size_t len)
	{
		assert(readableBytes() >= len);
		char* str = peek();
		return std::string(str, len);
	}

	void updateWriterIndex(size_t n)
	{	
		assert(writableBytes() >= n);
		writerIndex_ += n;
	}

	char* getReaderAddr()
	{
		return begin() + readerIndex_;
	}

private:
	char* begin()
	{ return &*buffer_.begin(); }

	void makeSpace(size_t len)
	{
		if(writableBytes() + prependableBytes() < len + kCheapPrepend)
		{
			buffer_.resize(writerIndex_+len);
		} else {
			// move readable data to the front, make space inside buffer
			assert(kCheapPrepend < readerIndex_);
			size_t readable = readableBytes();
			std::copy(begin() + readerIndex_,
					begin() + writerIndex_,
					begin() + kCheapPrepend);
			readerIndex_ = kCheapPrepend;
			writerIndex_ = readerIndex_ + readable;
			assert(readable == readableBytes());
		}
	}

private:
	std::vector<char> buffer_;
	size_t readerIndex_;
	size_t writerIndex_;

	static const char kCRLF[];
};

#endif
