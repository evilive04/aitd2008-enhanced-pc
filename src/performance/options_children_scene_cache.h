#pragma once
#include "options_children_art.h"
#include "ui_bounded_lru.h"
#include <array>
#include <cstring>
namespace options_children_presentation {
// Only owned CPU scenes; exact snapshot keys include selection, enabled/locked
// flags, footer bindings, descriptions and all native gauge values/domains.
class SceneCache {
 using Key=std::array<unsigned char,sizeof(options_children_art::View)>;
 struct Artwork{ui_art::Scene scene;options_children_art::Layout layout;};
 ui_cache::BoundedLru<Key,Artwork> cache{8,900000};
 static Key MakeKey(const options_children_art::View& view){Key key;std::memcpy(key.data(),&view,sizeof view);return key;}
 static bool Eligible(const options_children_art::View& view){return view.page>=options_children_art::Page::Audio&&view.page<=options_children_art::Page::Display;}
public:
 bool Find(const options_children_art::View& view,ui_art::Scene& scene,options_children_art::Layout& layout)noexcept{try{if(!Eligible(view))return false;Artwork art;if(!cache.Get(MakeKey(view),art))return false;scene=std::move(art.scene);layout=art.layout;return true;}catch(...){return false;}}
 void Remember(const options_children_art::View& view,const ui_art::Scene& scene,const options_children_art::Layout& layout)noexcept{try{if(!Eligible(view)||scene.batches.empty())return;std::size_t vertices=0;for(const auto& batch:scene.batches)vertices+=batch.vertices.size();if(vertices<=900000)cache.Put(MakeKey(view),Artwork{scene,layout},vertices);}catch(...){/* Optional CPU cache must never hide valid art. */}}
 std::size_t Size()const{return cache.Size();}std::size_t Vertices()const{return cache.Weight();}
};
}
