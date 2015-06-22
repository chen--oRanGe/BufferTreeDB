#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>

#include "Layout.h"
#include "Logger.h"
#include "Node.h"
#include "BufferTree.h"

using namespace bt;

#define NODE_SIZE sizeof(Node)

Layout::Layout(std::string& name)
    : name_(name),
      curDataId_(0),
      rootNodeId_(0),
      maxNodeId_(0),
      curFd_(-1),
      curMetaFd_(-1),
      curPath_(DATA_PATH),
      metaPath_(META_PATH),
      metadata_(),
      readBuf_(),
      writeBuf_(),
      tree_(NULL)
{
	curPath_ += name;
	metaPath_ += name;
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
		curPath_ = curPath_ + "-" + std::string(1, '0');
        curFd_ = open(curPath_.data(), O_RDWR | O_LARGEFILE);
        if(curFd_ < 0) {
			LOGFMTA("Layout::init open curfd error [%d]", curFd_);
			return false;
		}

		curMetaFd_ = open(metaPath_.data(), O_RDWR | O_LARGEFILE);
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
    Buffer buf;
    readFile(metaPath_, 0, 16, buf);

    int magic = buf.readInt32();
    if(magic != MAGIC) {
        return false;
    }

    rootNodeId_ = buf.readInt32();
    maxNodeId_ = buf.readInt32();
    curDataId_ = buf.readInt32();

    Postion nodePos;
    for(size_t i = 0; i < maxNodeId_; ++i) {
        nodePos.dataId = buf.readInt8();
        nodePos.offset = buf.readInt32();
        nodePos.size = buf.readInt32();
        metadata_.push_back(nodePos);
    }


	return true;

}

bool Layout::flushMetadata()
{
    Buffer buf;
    buf.appendInt32(MAGIC);
    buf.appendInt32(rootNodeId_);
    buf.appendInt32(maxNodeId_);
    buf.appendInt32(curDataId_);

    for(size_t i = 0; i < metadata_.size(); ++i) {
        buf.appendInt8(metadata_[i].dataId);
        buf.appendInt32(metadata_[i].offset);
        buf.appendInt32(metadata_[i].size);
    }

    writeFile(metaPath_, 0, 16 + metadata_.size() * 9, buf);

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
	
    std::string path = curPath_ + "-" + std::string(1, (nodePos.dataId + '0'));
    LOGFMTI("Layout::find path: %s", path.c_str());
	
    readFile(path, nodePos.offset, nodePos.size, buf);

    //char* p = (char*)slab_->alloc(readBuf_.size());
    //char* p = (char*)slab_->alloc(10);
	//assert(tree_);
    //Node* node = new (p) Node(tree_, nid, slab_);

    //node->deserialize(readBuf_);

    return true;
}

// FIXME sort the nodes??
int Layout::write(std::vector<Node*>& nodes)
{
    size_t i = 0, j = 0;
	nid_t nid;
	size_t offset;
    bool merged = false;
    std::map<nid_t, Postion> posMap;
    size_t size = 0;
    for(i = 0; i < nodes.size(); ++i) {
        // 序列化Node

        size = 0;
        merged = false;
        posMap.clear();

        nid = nodes[i]->nid();
        offset = nid * POSTION_SIZE + HEADER;
        nodes[i]->serialize(writeBuf_);
        
        Postion nodePos(curDataId_, offset, nodes[i]->size());
        posMap[nid] = nodePos;
        size += nodes[i]->size();


        for(j = i + 1; j < nodes.size(); ++j) {
            if(nodes[j]->nid() - nodes[i]->nid() == (j - i)) {
                // 序列化Node
				// if node is close, we can write together
                //nodes[j]->serialize(writeBuf_ + (j - i) * NODE_SIZE);
                nodes[i]->serialize(writeBuf_);
                nid = nodes[j]->nid();
                Postion nodePos(curDataId_, nid * POSTION_SIZE + HEADER, nodes[j]->size());
                posMap[nid] = nodePos;
                size += nodes[i]->size();
                merged = true;
            } else {
                break;
            }
        }
        // 写入磁盘
        LOGFMTI("Layout::write offset, size: (%d, %d)", offset, size);
        writeFile(curPath_, offset, size, writeBuf_);

		// update metadata
        for(std::map<nid_t, Postion>::iterator it = posMap.begin(); it != posMap.end(); ++it) {
            metadata_[it->first] = it->second;
        }

        if(merged) {
            i = j;
        }
    }



	return 0;
}

bool Layout::readFile(std::string& path, uint32_t offset, uint32_t size, Buffer& data)
{
    if(path != curPath_) {
        close(curFd_);
		curFd_ = open(path.data(), O_RDWR | O_LARGEFILE);
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

	data.updateWriterIndex(ret);

	return true;
}


bool Layout::writeFile(std::string& path, uint32_t offset, uint32_t size, Buffer& data)
{
    if(path != curPath_) {
        close(curFd_);
		curFd_ = open(path.data(), O_RDWR | O_LARGEFILE);
        if(curFd_ < 0) {
			LOGFMTA("Layout::readFile open curfd error [%d]", curFd_);
			return false;
		}
        curPath_ = path;
    }

    int ret = pwrite(curFd_, data.getReaderAddr(), size, offset);
	if(ret != (int)size) {
		LOGFMTA("Layout::readFile error [%d]", ret);
		return false;
	}

	// error??
	
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
