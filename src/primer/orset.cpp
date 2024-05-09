#include "primer/orset.h"
#include <algorithm>
#include <string>
#include <vector>
#include "common/exception.h"
#include "fmt/format.h"

namespace bustub {

template <typename T>
auto ORSet<T>::Contains(const T &elem) const -> bool {
  // TODO(student): Implement this
  std::vector<T> elements = Elements();
  if(std::find(elements.begin(), elements.end(), elem) != elements.end())
    return true;
  else
    return false;
  throw NotImplementedException("ORSet<T>::Contains is not implemented");
}

template <typename T>
void ORSet<T>::Add(const T &elem, uid_t uid) {
  // TODO(student): Implement this
  elements_.push_back({elem, uid});
  return;
  throw NotImplementedException("ORSet<T>::Add is not implemented");
}

template <typename T>
void ORSet<T>::Remove(const T &elem) {
  // TODO(student): Implement this
  for(auto it = elements_.begin(); it != elements_.end();){
    if(it->element_ == elem){
      removed_.push_back(*it);
      it = elements_.erase(it);
    }else{
      it++;
    }
  }
  return;
  throw NotImplementedException("ORSet<T>::Remove is not implemented");
}

template <typename T>
void ORSet<T>::Merge(const ORSet<T> &other) {
  // TODO(student): Implement this
  std::vector<orset_node_t> temp;
  
  for(auto it = other.elements_.begin(); it != other.elements_.end(); it++){
    if(std::find(removed_.begin(), removed_.end(), *it) == removed_.end()){
      temp.push_back(*it);
    }
  }

  for(auto it = elements_.begin(); it != elements_.end(); it++){
    if(std::find(other.removed_.begin(), other.removed_.end(), *it) == other.removed_.end()
    && std::find(temp.begin(), temp.end(), *it) == temp.end()){
      temp.push_back(*it);
    }
  }

  for(auto it = other.removed_.begin(); it != other.removed_.end(); it++){
    if(std::find(removed_.begin(), removed_.end(), *it) == removed_.end())
      removed_.push_back(*it);
  }

  elements_.clear();

  for(auto it = temp.begin(); it != temp.end(); it++){
    elements_.push_back(*it);
  }

  return;
  throw NotImplementedException("ORSet<T>::Merge is not implemented");
}

template <typename T>
auto ORSet<T>::Elements() const -> std::vector<T> {
  // TODO(student): Implement this
  std::vector<T> elements;
  for(auto it = elements_.begin(); it != elements_.end(); it++){
    elements.push_back(it->element_);
  }
  return elements;
  throw NotImplementedException("ORSet<T>::Elements is not implemented");
}

template <typename T>
auto ORSet<T>::ToString() const -> std::string {
  auto elements = Elements();
  std::sort(elements.begin(), elements.end());
  return fmt::format("{{{}}}", fmt::join(elements, ", "));
}

template class ORSet<int>;
template class ORSet<std::string>;

}  // namespace bustub
