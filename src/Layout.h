#ifndef __BT_TABLE_H
#define __BT_TABLE_H
#include <stdint.h>
#include <map>
#include <deque>

#include <boost/noncopyable.hpp>
#include <boost/function.hpp>

#include "Buffer.h"
#include "Options.h"


namespace bt
{
#define MAGIC 0x11223344
#define DATA_PATH "/buffertree/data/data-"
#define META_PATH "/buffertree/data/meta-"

struct Postion
{
	char dataId;
	uint32_t offset;
	uint32_t size;
	Postion()
		: dataId(0), offset(0), size(0)
		{}
	Postion(char dataId_, uint32_t offset_, uint32_t size_)
		: dataId(dataId_), offset(offset_), size(size_)
		{}
};

class Node;
class BufferTree;

#define POSTION_SIZE sizeof(Postion)
#define HEADER 512

class Layout : boost::noncopyable
{
public:
    Layout(std::string& name);
    ~Layout();

	bool findDataFile();
    bool init();
	bool loadMetadata(size_t len);
	bool flushMetadata();
	bool find(nid_t nid, Buffer& buf);
	int write(std::vector<Node*>& nodes);
	bool readFile(std::string& path, uint32_t offset, uint32_t size, Buffer& data);
	bool writeFile(std::string& path, uint32_t offset, uint32_t size, Buffer& data);
	nid_t getRootNid();
	nid_t getNodeCount();
	void setRootNid(nid_t rootId);

private:
	std::string name_;
	size_t curDataId_;
	nid_t rootNodeId_;
	nid_t maxNodeId_;
	int curFd_;
	int curMetaFd_;
	std::string curPath_;
	std::string metaPath_;
	std::vector<Postion> metadata_;
	Buffer readBuf_;
	Buffer writeBuf_;
	BufferTree* tree_;
};
}

#endif
