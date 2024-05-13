//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include "common/exception.h"
#include <algorithm>

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool { 
    latch_.lock();
    auto largest_node = std::max_element(node_store_.begin(), node_store_.end(), 
        [](const std::pair<frame_id_t, LRUKNode> &a, const std::pair<frame_id_t, LRUKNode> &b) {
            return a.second < b.second;
        });

    if(largest_node != node_store_.end() && largest_node->second.is_evictable()) {
        *frame_id = largest_node->first;
        node_store_.erase(largest_node);
        curr_size_--;
        latch_.unlock();
        return true;
    }
    else{   
        latch_.unlock();
        return false;
    } 
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id, [[maybe_unused]] AccessType access_type) {
    latch_.lock();
    if(size_t(frame_id) > replacer_size_) {
        latch_.unlock();
        throw Exception("frame_id out of range");
        return;
    }
    
    if(node_store_.find(frame_id) == node_store_.end()) {
        std::shared_ptr<LRUKNode> new_node = std::make_shared<LRUKNode>(frame_id);
        if(new_node->get_k() < k_) {
            new_node->add_to_history(current_timestamp_ + inf);
        }else{
            new_node->add_to_history(current_timestamp_);
        }
       
        node_store_.insert({frame_id, *new_node});
        current_timestamp_--;
    }
    else{
        node_store_[frame_id].set_k(node_store_[frame_id].get_k() + 1);
        if(node_store_[frame_id].get_k() < k_)
            node_store_[frame_id].add_to_history(current_timestamp_ + inf);
        else
        {
            std::list<size_t> hist = node_store_[frame_id].get_history();
            for(std::list<size_t>::iterator it = hist.begin(); it != hist.end(); it++)
            {    *it -= inf;}
            node_store_[frame_id].add_to_history(current_timestamp_);
        }
        current_timestamp_--;
    }
    latch_.unlock();
    return;
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
    latch_.lock();
    if(node_store_.find(frame_id) == node_store_.end()) {
        latch_.unlock();
        throw Exception("frame_id not find");
        return;
    }
    if(set_evictable != node_store_[frame_id].is_evictable())
    {
        if(set_evictable)
            curr_size_++;
        else
            curr_size_--;
    }
    node_store_[frame_id].set_evictable(set_evictable);
    latch_.unlock();
    return;
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
    latch_.lock();
    if(node_store_.find(frame_id) == node_store_.end()) {
        latch_.unlock();
        return;
    }

    node_store_.erase(frame_id);
    latch_.unlock();
    return;
}

auto LRUKReplacer::Size() -> size_t {
    latch_.lock();
    latch_.unlock();
    return curr_size_;}

}  // namespace bustub
