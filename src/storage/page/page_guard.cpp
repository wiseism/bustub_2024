#include "storage/page/page_guard.h"
#include "buffer/buffer_pool_manager.h"

namespace bustub {

BasicPageGuard::BasicPageGuard(BasicPageGuard &&that) noexcept {
    bpm_ = that.bpm_;
    page_ = that.page_; 
    that.bpm_ = nullptr;
    that.page_ = nullptr;
    this->is_dirty_ = that.is_dirty_;
}

void BasicPageGuard::Drop() {
    if(bpm_ == nullptr || page_ == nullptr) return;
    bpm_->UnpinPage(page_->GetPageId(), is_dirty_);
    bpm_ = nullptr;
    page_ = nullptr;
}

auto BasicPageGuard::operator=(BasicPageGuard &&that) noexcept -> BasicPageGuard & { 
    Drop();
    bpm_ = that.bpm_;
    page_ = that.page_;
    that.bpm_ = nullptr;
    that.page_ = nullptr;
    this->is_dirty_ = that.is_dirty_;
    return *this; }

BasicPageGuard::~BasicPageGuard(){this->Drop();};  // NOLINT

auto BasicPageGuard::UpgradeRead() -> ReadPageGuard { 
    if(bpm_ == nullptr || page_ == nullptr) return {nullptr, nullptr};
    ReadPageGuard guard(bpm_, page_);
    Drop();
    return guard; 
}

auto BasicPageGuard::UpgradeWrite() -> WritePageGuard { 
    if(bpm_ == nullptr || page_ == nullptr) return {nullptr, nullptr};
    WritePageGuard guard(bpm_, page_);
    Drop();
    return guard;  
}

ReadPageGuard::ReadPageGuard(BufferPoolManager *bpm, Page *page) {
    guard_ = BasicPageGuard(bpm, page);
    guard_.page_->RLatch();
}

ReadPageGuard::ReadPageGuard(ReadPageGuard &&that) noexcept
{
    guard_ = std::move(that.guard_);
};

auto ReadPageGuard::operator=(ReadPageGuard &&that) noexcept -> ReadPageGuard & { 
    this->Drop();
    guard_ = std::move(that.guard_);
    that.Drop();
    guard_.page_->RLatch();
    return *this; 
}

void ReadPageGuard::Drop() {
    guard_.page_->RUnlatch();
    guard_.Drop();
}

ReadPageGuard::~ReadPageGuard() {
    if(guard_.page_ == nullptr) return;
    Drop();
}  // NOLINT

WritePageGuard::WritePageGuard(BufferPoolManager *bpm, Page *page) {
    guard_ = BasicPageGuard(bpm, page);
    guard_.page_->WLatch();
}

WritePageGuard::WritePageGuard(WritePageGuard &&that) noexcept{
    guard_ = std::move(that.guard_);
};

auto WritePageGuard::operator=(WritePageGuard &&that) noexcept -> WritePageGuard & {
    this->Drop();
    guard_ = std::move(that.guard_);
    that.Drop();
    guard_.page_->WLatch();
    return *this;
}

void WritePageGuard::Drop() {
     guard_.page_->WUnlatch();
    guard_.Drop();
}

WritePageGuard::~WritePageGuard() {
    if(guard_.page_ == nullptr) return;
    Drop();
}  // NOLINT

}  // namespace bustub
