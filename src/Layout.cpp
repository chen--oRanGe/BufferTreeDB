#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>

#include "Layout.h"
#include "Logger.h"
#include "Node.h"
#include "BufferTree.h"

using namespace bt;

Layout::Layout(std::string& name)
    : name_(name),
      curDataId_(0),
      rootNodeId_(0),
      maxNodeId_(0),
      curFd_(-1),
      curMetaFd_(-1),
      curPath_(DATA_PATH),
      metaPath_(META_PATH),
      metadata_(1024),
      writeBuf_(),
      tree_(NULL)
{
	curPath_ += name;
	metaPath_ += name;
	
	// ensure hold 4 nodes(1M = 256K * 4).
	writeBuf_.ensureWritableBytes(1024 * 1024 * 1024);
}

Layout::~Layout()
{

}

bool Layout::findDataFile()
{
    return false;
}

bool Layout::init()
{
    if(findDataFile()){
        if(!loadMetadata(10))
            return false;
    } else {
		curPath_ = curPath_ + "_" + std::string(1, '0');
        curFd_ = open(curPath_.data(), O_RDWR | O_LARGEFILE | O_CREAT);
        if(curFd_ < 0) {
			LOGFMTA("Layout::init open curfd error [%d]", curFd_);
			return false;
		}

		curMetaFd_ = open(metaPath_.data(), O_RDWR | O_LARGEFILE | O_CREAT);
        if(curMetaFd_ < 0) {
			LOGFMTA("Layout::init open curMetaFd_ error [%d]", curMetaFd_);
			return false;
		}
    }


    return true;
}

bool Layout::loadMetadata(size_t len)
{
    //结合系统缓存实现元数据信息的持久化
	writeBuf_.retrieveAll();
    readFile(metaPath_, 0, 16, writeBuf_);

    int magic = writeBuf_.readInt32();
    if(magic != MAGIC) {
        return false;
    }

    rootNodeId_ = writeBuf_.readInt32();
    maxNodeId_ = writeBuf_.readInt32();
    curDataId_ = writeBuf_.readInt32();

    Postion nodePos;
	readFile(metaPath_, HEADER, maxNodeId_ * POSTION_SIZE, writeBuf_);
	
    for(size_t i = 0; i < maxNodeId_; ++i) {
        nodePos.dataId = writeBuf_.readInt8();
        nodePos.offset = writeBuf_.readInt32();
        nodePos.size = writeBuf_.readInt32();
        metadata_.push_back(nodePos);
    }


	return true;

}

bool Layout::flushMetadata()
{
	writeBuf_.retrieveAll();
    writeBuf_.appendInt32(MAGIC);
    writeBuf_.appendInt32(rootNodeId_);
    writeBuf_.appendInt32(maxNodeId_);
    writeBuf_.appendInt32(curDataId_);

	writeFile(metaPath_, 0, 16, writeBuf_);

    for(size_t i = 0; i < metadata_.size(); ++i) {
        writeBuf_.appendInt8(metadata_[i].dataId);
        writeBuf_.appendInt32(metadata_[i].offset);
        writeBuf_.appendInt32(metadata_[i].size);
    }

    writeFile(metaPath_, HEADER, metadata_.size() * POSTION_SIZE, writeBuf_);

	return true;
}

/*
bool Layout::loadData(std::vector<Node*>& nodes)
{
    // 先从元数据中找到Node保存的位置信息
    // 然后，构造Node
    for(int i = 0; i < nodes.size(); ++i) {
        Slice s = metadata_[nodes.nid()];
    }
}

bool Layout::flushData(std::vector<Node*>& nodes)
{
    // 先写入数据
    // 然后更新元数据信息
    // 是否需要实现成事务？？
}*/


bool Layout::find(nid_t nid, Buffer& buf)
{
    // 从元数据中找到结点位置信息
    Postion nodePos = metadata_[nid];

    std::string path = DATA_PATH + name_ + "_" + std::string(1, (nodePos.dataId + '0'));
    LOGFMTI("Layout::find path: %s", path.c_str());
	
    readFile(path, nodePos.offset, nodePos.size, buf);

    return true;
}

// FIXME sort the nodes??
int Layout::write(std::map<nid_t, Node*>& dirtyNodes)
{

	Node* node;
	nid_t nid;
	size_t offset;
    size_t size = 0;
	writeBuf_.retrieveAll();


	std::map<nid_t, Node*>::iterator it0, itEnd = dirtyNodes.end();

	/*
	for(it0 = dirtyNodes.begin(); it0 != itEnd;) {
		Node* node = it0->second;
		nid = it0->first;
		n = 1;
		while((nid + n == it1->first) && n < 4 && it1 != itEnd) {
			n++;
			++it1;
		}
		
		LOGFMTI("Layout::write find continuous nodes [%lu])", n);

		// serialize nodes
		while(it0 != it1) {
			node = it0->second;
			nid = it0->first;
			offset = nid * POSTION_SIZE + HEADER;
			node->readLock();
			node->serialize(writeBuf_);
			node->readUnlock();
			++it0;
		}
		
        LOGFMTI("Layout::write offset, size: (%lu, %lu)", offset, size);
        writeFile(curPath_, offset, size, writeBuf_);

		

	}*/

	for(it0 = dirtyNodes.begin(); it0 != itEnd; ++it0) {
		node = it0->second;
		nid = it0->first;
		node->readLock();
		node->serialize(writeBuf_);
		node->readUnlock();

		offset = nid * (256 * 1024);
		size = writeBuf_.readableBytes();
		LOGFMTI("Layout::write offset, size: (%lu, %lu)", offset, size);
		writeFile(curPath_, offset, size, writeBuf_);

		//update the metadata
		if(nid > metadata_.size()) {
			metadata_.push_back(Postion(curDataId_, offset, size));
		} else 
			metadata_[nid] = Postion(curDataId_, offset, size);
	}

	return 0;
}

bool Layout::readFile(std::string& path, uint32_t offset, uint32_t size, Buffer& data)
{
    if(path != curPath_) {
        close(curFd_);
		curFd_ = open(path.data(), O_RDWR | O_LARGEFILE | O_CREAT);
        if(curFd_ < 0) {
			LOGFMTA("Layout::readFile open curfd error [%d]", curFd_);
			return false;
		}
        curPath_ = path;
    }

    int ret = pread(curFd_, data.beginWrite(), size, offset);
	if(ret != (int)size) {
		LOGFMTA("Layout::readFile error [%d]", ret);
		return false;
	}

	data.updateWriterIndex(size);

	return true;
}


bool Layout::writeFile(std::string& path, uint32_t offset, uint32_t size, Buffer& data)
{
    if(path != curPath_) {
        close(curFd_);
		curFd_ = open(path.data(), O_RDWR | O_LARGEFILE | O_CREAT);
        if(curFd_ < 0) {
			LOGFMTA("Layout::readFile open curfd error [%d]", curFd_);
			return false;
		}
        curPath_ = path;
    }

    int ret = pwrite(curFd_, data.beginRead(), size, offset);
	if(ret != (int)size) {
		LOGFMTA("Layout::readFile error [%d]", ret);
		return false;
	}

	data.updateReadIndex(size);
	
	return true;
}

nid_t Layout::getRootNid()
{
	return rootNodeId_;
}

nid_t Layout::getNodeCount()
{
	return maxNodeId_;
}

void Layout::setRootNid(nid_t rootId)
{
	rootNodeId_ = rootId;
}
