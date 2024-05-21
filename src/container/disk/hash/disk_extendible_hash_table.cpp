//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// disk_extendible_hash_table.cpp
//
// Identification: src/container/disk/hash/disk_extendible_hash_table.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "buffer/buffer_pool_manager.h"
#include "common/config.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"
#include "common/rid.h"
#include "common/util/hash_util.h"
#include "container/disk/hash/disk_extendible_hash_table.h"
#include "storage/disk/disk_manager.h"
#include "storage/index/hash_comparator.h"
#include "storage/page/extendible_htable_bucket_page.h"
#include "storage/page/extendible_htable_directory_page.h"
#include "storage/page/extendible_htable_header_page.h"
#include "storage/page/page_guard.h"
#include "type/value.h"

namespace bustub {

template <typename K, typename V, typename KC>
DiskExtendibleHashTable<K, V, KC>::DiskExtendibleHashTable(const std::string &name, BufferPoolManager *bpm,
                                                           const KC &cmp, const HashFunction<K> &hash_fn,
                                                           uint32_t header_max_depth, uint32_t directory_max_depth,
                                                           uint32_t bucket_max_size)
    : bpm_(bpm),
      cmp_(cmp),
      hash_fn_(std::move(hash_fn)),
      header_max_depth_(header_max_depth),
      directory_max_depth_(directory_max_depth),
      bucket_max_size_(bucket_max_size) {
        BasicPageGuard header_guard = bpm_->NewPageGuarded(&header_page_id_);
        auto header_page = header_guard.AsMut<ExtendibleHTableHeaderPage>();
        header_page->Init(header_max_depth_);
        bpm_->FlushAllPages();
        header_guard.Drop();
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::GetValue(const K &key, std::vector<V> *result, Transaction *transaction) const
    -> bool {
      bool found = false;

      BasicPageGuard header_guard = bpm_->FetchPageBasic(header_page_id_);
      auto *header = header_guard.As<ExtendibleHTableHeaderPage>();

      BasicPageGuard directory_guard = bpm_->FetchPageBasic(header->GetDirectoryPageId(header->HashToDirectoryIndex(hash_fn_.GetHash(key))));
      auto *directory = directory_guard.As<ExtendibleHTableDirectoryPage>();
      uint32_t bucket_page_id = directory->GetBucketPageId( directory->HashToBucketIndex(Hash(key)));

      BasicPageGuard bucket_guard = bpm_->FetchPageBasic(bucket_page_id);
      auto *bucket = bucket_guard.As<ExtendibleHTableBucketPage<K, V, KC>>();
      
      if(result->empty())
        result->push_back(V());
      found = bucket->Lookup(key, result->front(), cmp_);
      if(!found)result->clear();
      
      bpm_->FlushAllPages();
      bucket_guard.Drop();
      directory_guard.Drop();
      header_guard.Drop();

      return found;
      
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::Insert(const K &key, const V &value, Transaction *transaction) -> bool {
  bool inserted = false;

  BasicPageGuard header_guard = bpm_->FetchPageBasic(header_page_id_);
  auto header = header_guard.AsMut<ExtendibleHTableHeaderPage>(); 

  uint32_t hash = hash_fn_.GetHash(key);

  uint32_t directory_idx = header->HashToDirectoryIndex(hash);
  page_id_t directory_page_id = header->GetDirectoryPageId(directory_idx);

  if(directory_page_id == INVALID_PAGE_ID)
  {
    inserted = InsertToNewDirectory(header, directory_idx, hash, key, value);
    bpm_->FlushAllPages();
    header_guard.Drop();
    return inserted;
  }  

  BasicPageGuard directory_guard = bpm_->FetchPageBasic(directory_page_id);
  auto directory = directory_guard.AsMut<ExtendibleHTableDirectoryPage>();
  uint32_t bucket_idx = directory->HashToBucketIndex(Hash(key));
  page_id_t bucket_page_id = directory->GetBucketPageId(bucket_idx);

  BasicPageGuard bucket_guard = bpm_->FetchPageBasic(bucket_page_id);
  auto bucket =bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();

  if(bucket->IsFull())
  {
    directory->IncrLocalDepth(bucket_idx);
    if(directory->GetLocalDepth(bucket_idx) > directory->GetGlobalDepth())
      directory->IncrGlobalDepth();
    if(directory->GetGlobalDepth() > directory_max_depth_)
      return false;
    
    uint32_t new_bucket_idx = bucket_idx + directory->Size() / 2;
    
    page_id_t new_bucket_page_id;
    BasicPageGuard new_bucket_guard = bpm_->NewPageGuarded(&new_bucket_page_id);
    if(new_bucket_page_id == INVALID_PAGE_ID)
      return false;
    directory->SetBucketPageId(new_bucket_idx, new_bucket_page_id);
    auto new_bucket = new_bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
    new_bucket->Init(bucket_max_size_);
    for(size_t i = 0; i < bucket->Size(); i++){
      uint32_t bucket_idx_new = directory->HashToBucketIndex(Hash(bucket->KeyAt(i)));
      if(bucket_idx_new == new_bucket_idx){
        new_bucket->Insert(bucket->KeyAt(i), bucket->ValueAt(i), cmp_);
        bucket->Remove(bucket->KeyAt(i), cmp_);
      }
    }
    inserted = this->Insert(key, value, transaction);
    
    bpm_->FlushAllPages();
    new_bucket_guard.Drop();
    return inserted;
  }else{
    inserted = bucket->Insert(key, value, cmp_);
    bpm_->FlushAllPages();
    bucket_guard.Drop();
    directory_guard.Drop();
    header_guard.Drop();
    return inserted;
  }


  return inserted;
}

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::InsertToNewDirectory(ExtendibleHTableHeaderPage *header, uint32_t directory_idx,
                                                            uint32_t hash, const K &key, const V &value) -> bool {
  page_id_t temp_directory_page_id = INVALID_PAGE_ID;
  BasicPageGuard directory_guard = bpm_->NewPageGuarded(&temp_directory_page_id);
  if(temp_directory_page_id == INVALID_PAGE_ID)
    return false;

  auto directory = directory_guard.AsMut<ExtendibleHTableDirectoryPage>();
  directory->Init(directory_max_depth_);
  header->SetDirectoryPageId(directory_idx, temp_directory_page_id);
  
  uint32_t bucket_idx = directory->HashToBucketIndex(Hash(key));
  bool inserted = InsertToNewBucket(directory, bucket_idx, key, value);
  
  directory_guard.Drop();

  return inserted;
}

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::InsertToNewBucket(ExtendibleHTableDirectoryPage *directory, uint32_t bucket_idx,
                                                          const K &key, const V &value) -> bool {
  page_id_t temp_bucket_page_id = INVALID_PAGE_ID;
  BasicPageGuard bucket_guard = bpm_->NewPageGuarded(&temp_bucket_page_id);
  auto bucket = bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
  if(temp_bucket_page_id == INVALID_PAGE_ID)
    return false;

  bucket->Init(bucket_max_size_);
  directory->SetBucketPageId(bucket_idx, temp_bucket_page_id);
  bucket->Insert(key, value, cmp_);

  bpm_->FlushAllPages();
  bucket_guard.Drop();
  return true;
}

template <typename K, typename V, typename KC>
void DiskExtendibleHashTable<K, V, KC>::UpdateDirectoryMapping(ExtendibleHTableDirectoryPage *directory,
                                                               uint32_t new_bucket_idx, page_id_t new_bucket_page_id,
                                                               uint32_t new_local_depth, uint32_t local_depth_mask) {
  throw NotImplementedException("DiskExtendibleHashTable is not implemented");
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::Remove(const K &key, Transaction *transaction) -> bool {
  

  BasicPageGuard header_guard = bpm_->FetchPageBasic(header_page_id_);
  auto *header = header_guard.AsMut<ExtendibleHTableHeaderPage>();

  BasicPageGuard directory_guard = bpm_->FetchPageBasic(header->GetDirectoryPageId(header->HashToDirectoryIndex(hash_fn_.GetHash(key))));
  auto *directory = directory_guard.AsMut<ExtendibleHTableDirectoryPage>();
  uint32_t bucket_page_id = directory->GetBucketPageId( directory->HashToBucketIndex(Hash(key)));

  BasicPageGuard bucket_guard = bpm_->FetchPageBasic(bucket_page_id);
  auto *bucket = bucket_guard.AsMut<ExtendibleHTableBucketPage<K, V, KC>>();
  
  std::vector<V> result;
  if(result.empty())
    result.push_back(V());
  bool found = bucket->Lookup(key, result.front(), cmp_);
  if(!found)
    return false;
  
  bucket->Remove(key, cmp_);
  return true;
}

template class DiskExtendibleHashTable<int, int, IntComparator>;
template class DiskExtendibleHashTable<GenericKey<4>, RID, GenericComparator<4>>;
template class DiskExtendibleHashTable<GenericKey<8>, RID, GenericComparator<8>>;
template class DiskExtendibleHashTable<GenericKey<16>, RID, GenericComparator<16>>;
template class DiskExtendibleHashTable<GenericKey<32>, RID, GenericComparator<32>>;
template class DiskExtendibleHashTable<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
