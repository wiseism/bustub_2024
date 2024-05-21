//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_htable_directory_page.cpp
//
// Identification: src/storage/page/extendible_htable_directory_page.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/page/extendible_htable_directory_page.h"

#include <algorithm>
#include <unordered_map>

#include "common/config.h"
#include "common/logger.h"

namespace bustub {

void ExtendibleHTableDirectoryPage::Init(uint32_t max_depth) {
  max_depth_ = max_depth;
  global_depth_ = 0;
  memset(local_depths_, 0, sizeof(local_depths_));
  memset(bucket_page_ids_, INVALID_PAGE_ID, sizeof(bucket_page_ids_));
}

auto ExtendibleHTableDirectoryPage::HashToBucketIndex(uint32_t hash) const -> uint32_t { 
  return hash & ((1 << global_depth_) - 1); 
  }

auto ExtendibleHTableDirectoryPage::GetBucketPageId(uint32_t bucket_idx) const -> page_id_t { 
  if(bucket_idx >= HTABLE_DIRECTORY_ARRAY_SIZE) return INVALID_PAGE_ID;
  return bucket_page_ids_[bucket_idx]; 
  }

void ExtendibleHTableDirectoryPage::SetBucketPageId(uint32_t bucket_idx, page_id_t bucket_page_id) {
  bucket_page_ids_[bucket_idx] = bucket_page_id;
}

auto ExtendibleHTableDirectoryPage::GetSplitImageIndex(uint32_t bucket_idx) const -> uint32_t { return 0; }

auto ExtendibleHTableDirectoryPage::GetGlobalDepth() const -> uint32_t { return global_depth_; }

void ExtendibleHTableDirectoryPage::IncrGlobalDepth() {
  for(uint32_t idx = 0; idx < Size(); idx++) {
    SetBucketPageId(idx + Size(), GetBucketPageId(idx));
    SetLocalDepth(idx + Size(), GetLocalDepth(idx));
  }
  global_depth_ = global_depth_ + 1;
}

void ExtendibleHTableDirectoryPage::DecrGlobalDepth() {
  global_depth_ = global_depth_ - 1;
}

auto ExtendibleHTableDirectoryPage::CanShrink() -> bool {
  for(uint32_t idx = 0; idx < Size(); idx++) {
    if(GetLocalDepth(idx) == GetGlobalDepth()) return false;
  }
  return true;
}

auto ExtendibleHTableDirectoryPage::Size() const -> uint32_t { return 1 << global_depth_; }

auto ExtendibleHTableDirectoryPage::GetLocalDepth(uint32_t bucket_idx) const -> uint32_t { 
  return local_depths_[bucket_idx]; 
  }

void ExtendibleHTableDirectoryPage::SetLocalDepth(uint32_t bucket_idx, uint8_t local_depth) {
  if(local_depths_[bucket_idx] <= local_depth)
    local_depths_[bucket_idx] = local_depth;
  else
   {
    local_depths_[bucket_idx] = local_depth;
    if(bucket_idx + Size()/2 < Size())
    {
      SetBucketPageId(bucket_idx, GetBucketPageId(bucket_idx + Size()/2));
    }
   }
}

void ExtendibleHTableDirectoryPage::IncrLocalDepth(uint32_t bucket_idx) {
  local_depths_[bucket_idx]++;
  if(local_depths_[bucket_idx] <= global_depth_)
  {
    if(bucket_idx + Size()/2 < Size() && local_depths_[bucket_idx + Size()/2] < local_depths_[bucket_idx])
      local_depths_[bucket_idx + Size()/2]++;
    else if(bucket_idx - Size()/2 >= 0 && local_depths_[bucket_idx - Size()/2] < local_depths_[bucket_idx])
      local_depths_[bucket_idx - Size()/2]++;
  }
}

void ExtendibleHTableDirectoryPage::DecrLocalDepth(uint32_t bucket_idx) {
  local_depths_[bucket_idx]--;
}

}  // namespace bustub
