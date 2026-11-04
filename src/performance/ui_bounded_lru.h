#pragma once
#include <map>
#include <cstddef>
#include <cstdint>
#include <utility>
// CPU artwork only. Callers provide synchronization when sharing across threads.
namespace ui_cache {
template<class Key,class Value> class BoundedLru {
 struct Entry{Value value;std::size_t weight;std::uint64_t used;};
 std::map<Key,Entry> entries;std::size_t limit,countLimit,total{};std::uint64_t clock{};
public:
 BoundedLru(std::size_t count,std::size_t weight):limit(weight),countLimit(count){}
 bool Get(const Key& key,Value& value){const auto i=entries.find(key);if(i==entries.end())return false;value=i->second.value;i->second.used=++clock;return true;}
 bool Put(const Key& key,const Value& value,std::size_t weight){
  if(!countLimit||weight>limit)return false;
  const auto old=entries.find(key);if(old!=entries.end()){old->second.used=++clock;return true;}
  // Copy before eviction: allocation failure leaves the existing working set valid.
  const auto inserted=entries.emplace(key,Entry{value,weight,++clock});if(!inserted.second)return false;total+=weight;
  while(entries.size()>countLimit||total>limit){auto victim=entries.begin();for(auto i=entries.begin();i!=entries.end();++i)if(i->second.used<victim->second.used)victim=i;total-=victim->second.weight;entries.erase(victim);}
  return true;
 }
 std::size_t Size()const{return entries.size();}std::size_t Weight()const{return total;}
 void Clear(){entries.clear();total=0;}
};
}
