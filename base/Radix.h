#ifndef __BT_RADIX_H
#define __BT_RADIX_H

namespace bt {
struct RadixNode 
{
    RadixNode* right;
    RadixNode* left;
    RadixNode* parent;
    uint64_t value;
};

template<class T>
class Radix
{
public:
    Radix();
    ~Radix();

private:
    RadixNode* root;
    RadixNode* free;
};

template<class T>
Radix::Radix(Slab* slab)
    : slab_(slab)
{
    free_ = NULL;
    start_ = NULL;
    size_ = 0;
    root_ = slab_->alloc(sizeof(RadixNode));
    if(root_ == NULL) {
        return;
    }

    root_->right = NULL;
    root_->left = NULL;
    root_->parent = NULL;
    root_->value = NULL;

}

template<class T>
bool Radix::insert(uint32_t key, T* value)
{
    RadixNode* node = root_;
    RadixNode* next = root_;
    uint32_t bit = 0x80000000;
    while(bit) {
        if(key & bit) {
            next = right;
        } else {
            next = left;
        }

        if(next == NULL)
            break;

        bit >>= 1;
        node = next;
    }

    while(bit) {
        next = alloc();
        if(next == NULL) {
            return false;
        }

        next->right = NULL;
        next->left = NULL;
        next->parent = node;
        next->value = NULL;

        if(key & bit) {
            node->right = next;
        } else {
            node->left = next;
        }

        bit >>= 1;
        node = next;
    }

    node->value = value;

    return true;
}

template<class T>
bool Radix::del(uint32_t key)
{
    uint32_t bit = 0x80000000;
    RadixNode* node = root_;

    while(node && bit) {
        if(key & bit)
            node = node->right;
        else
            node = node->left;

        bit >>= 1;
    }

    if(node == NULL)
        return false;

    if(node->right || node->left) {
        if(node->value != NULL) {
            node->value = NULL;
            return true;
        }
        return false;
    }

    for(;;) {
        if(node->parent->right == node) {
            node->parent->right = NULL;
        } else {
            node->parent->left = NULL;
        }

        node->right = free_;
        free_ = node;

        node = node->parent;

        if(node->right || node->left)
            break;

        if(node->parent == NULL)
            break;
    }

    return true;
}

template<class T>
T* find(uint32_t key)
{
    uint32_t bit = 0x80000000;
    T* value = NULL;
    node = root_;

    while(node) {
        if(node->value != NULL)
            value = node->value;

        if(key & bit)
            node = node->right;
        else
            node = node->left;

        bit >>= 1;
    }

    return value;
}

template<class T>
RadixNode* Radix::alloc()
{
    RadixNode* p;
    if(free_) {
        p = free_;
        free_ = free_->right;
        return p;
    }

    p = slab_->alloc(sizeof(RadixNode));
    if(start_ == NULL)
        return NULL;

    size_ += sizeof(RadixNode);

    return p;
}
}
#endif
