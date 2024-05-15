//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"

#include "common/config.h"
#include "common/exception.h"
#include "common/macros.h"
#include "storage/page/page.h"
#include "storage/page/page_guard.h"
#include <cstddef>

namespace bustub {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, size_t replacer_k,
                                     LogManager *log_manager)
    : pool_size_(pool_size), disk_scheduler_(std::make_unique<DiskScheduler>(disk_manager)), log_manager_(log_manager) {

  // we allocate a consecutive memory space for the buffer pool
  pages_ = new Page[pool_size_];
  replacer_ = std::make_unique<LRUKReplacer>(pool_size, replacer_k);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() { delete[] pages_; }

auto BufferPoolManager::NewPage(page_id_t *page_id) -> Page * { 
  latch_.lock();
  frame_id_t frame_id;
  if(free_list_.empty())
  {
    if(replacer_->Evict(&frame_id) == false)
    {
      page_id = nullptr;
      latch_.unlock();
      return  nullptr;
    }

    free_list_.push_back(frame_id);
  }
  frame_id = free_list_.front();
  free_list_.pop_front();
  latch_.unlock();
  FlushPage(pages_[frame_id].GetPageId());
  latch_.lock();

  pages_[frame_id].page_id_ = AllocatePage();
  pages_[frame_id].is_dirty_ = false;
  pages_[frame_id].pin_count_ = 1;
  pages_[frame_id].data_ = new char[BUSTUB_PAGE_SIZE];

  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);
  
  *page_id = pages_[frame_id].page_id_;
  latch_.unlock();
  return pages_ + frame_id; 
}

auto BufferPoolManager::FetchPage(page_id_t page_id, [[maybe_unused]] AccessType access_type) -> Page * {
  latch_.lock();
  for(size_t i = 0; i < pool_size_; i++)
  {
    if(pages_[i].page_id_ == page_id)
    {
      pages_[i].pin_count_++;
      replacer_->RecordAccess(i, access_type);
      pages_[i].is_dirty_ = true;
      latch_.unlock();
      return pages_ + i;
    }
  }

  frame_id_t frame_id;
  if(free_list_.empty())
  {
    if(replacer_->Evict(&frame_id) == false)
    {
      latch_.unlock();
      return  nullptr;
    }

    free_list_.push_back(frame_id);
  }
  frame_id = free_list_.front();
  free_list_.pop_front();
  latch_.unlock();
  FlushPage(pages_[frame_id].GetPageId());
  latch_.lock();

  pages_[frame_id].page_id_ = page_id;
  pages_[frame_id].is_dirty_ = true;
  pages_[frame_id].pin_count_ = 1;
  pages_[frame_id].data_[0] = '\0';

  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);

  auto promise = disk_scheduler_->CreatePromise();
  auto future = promise.get_future();
  disk_scheduler_->Schedule({false, pages_[frame_id].GetData(), pages_[frame_id].page_id_, std::move(promise)});
  future.get();
  pages_[frame_id].is_dirty_ = false;  

  latch_.unlock();

  return pages_ + frame_id;
}

auto BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty, [[maybe_unused]] AccessType access_type) -> bool {
  latch_.lock();
  for(size_t i = 0; i < pool_size_; i++)
  {
    if(pages_[i].page_id_ == page_id)
    {
      if(pages_[i].pin_count_ <= 0) 
      {
        latch_.unlock();
        return false;
      }
      
      pages_[i].pin_count_--;
      if(pages_[i].pin_count_ == 0)
        replacer_->SetEvictable(i, true);
      pages_[i].is_dirty_ = is_dirty;
      latch_.unlock();
      
      return true;
    }
  }

  latch_.unlock();
  return false;
}

auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool { 
  latch_.lock();
  for(size_t i = 0; i < pool_size_; i++)
  {
    if(pages_[i].page_id_ == page_id)
    {
      auto promise = disk_scheduler_->CreatePromise();
      auto future = promise.get_future();
      disk_scheduler_->Schedule({true, pages_[i].GetData(), pages_[i].page_id_, std::move(promise)});
      future.get();
      pages_[i].is_dirty_ = false;
      latch_.unlock();
      return true;
    }
  }  
  latch_.unlock();
  return false; 
}

void BufferPoolManager::FlushAllPages() {
  latch_.lock();
  for(size_t i = 0; i < pool_size_; i++)
  {
    if(pages_[i].is_dirty_)
    {
      auto promise = disk_scheduler_->CreatePromise();
      auto future = promise.get_future();
      disk_scheduler_->Schedule({true, pages_[i].GetData(), pages_[i].page_id_, std::move(promise)});
      pages_[i].is_dirty_ = false;
    }
  }
  latch_.unlock();
}

auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool {
  
  latch_.lock();
  for(size_t i = 0; i < pool_size_; i++)
  {
    if(pages_[i].page_id_ == page_id && pages_[i].pin_count_ == 0)
    {
      free_list_.push_back(i);
      latch_.unlock();
      return true;
    }else if(pages_[i].page_id_ == page_id && pages_[i].pin_count_ > 0)
    {
      latch_.unlock();
      return false;
    }
  }
  latch_.unlock();
  return true;
}

auto BufferPoolManager::AllocatePage() -> page_id_t { return next_page_id_++; }

auto BufferPoolManager::FetchPageBasic(page_id_t page_id) -> BasicPageGuard { 
  auto page = FetchPage(page_id);
  return {this, page}; 
}

auto BufferPoolManager::FetchPageRead(page_id_t page_id) -> ReadPageGuard {
  auto page = FetchPage(page_id);
  return {this, page};
}

auto BufferPoolManager::FetchPageWrite(page_id_t page_id) -> WritePageGuard {
  auto page = FetchPage(page_id);
  return {this, page};
 }

auto BufferPoolManager::NewPageGuarded(page_id_t *page_id) -> BasicPageGuard {
  auto page = NewPage(page_id);
  return {this, page};
}

}  // namespace bustub
