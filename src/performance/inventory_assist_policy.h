#pragma once
// Scalar snapshots only: remembered tokens are compared, never dereferenced.
namespace inventory_assist {
struct Key {unsigned token{},low{},high{};bool operator==(const Key& o)const{return token==o.token&&low==o.low&&high==o.high;}bool operator!=(const Key& o)const{return !(*this==o);}explicit operator bool()const{return token!=0;}};
struct Item {Key key{};unsigned definition{},kind{},hand{},revision{};bool lamp{},pistol{},throwable{};explicit operator bool()const{return static_cast<bool>(key);}};
struct Signature {
 unsigned definition{},kind{},hand{};bool valid{};
 static Signature Of(const Item& i){return {i.definition,i.kind,i.hand,static_cast<bool>(i)};}
 bool Matches(const Item& i)const{return valid&&i&&definition==i.definition&&kind==i.kind&&hand==i.hand;}
};
struct Frame {unsigned tick{};Key hero{},inventory{};bool eligible{};unsigned queue{};Item left{},right{},both{},remembered{},stock[12]{};};
struct Command {unsigned operation{},slot{};Key item{};explicit operator bool()const{return static_cast<bool>(item);}};
// Original world acquisition operation2 stores the item first. Its native
// flag20 re-equips only after successful storage, using the original hand.
inline bool AutoEquipAcquisition(const Frame& f,const Item& item,unsigned operation,unsigned slot,unsigned flags){
 if(!f.eligible||!f.hero||!f.inventory||f.queue>=7||operation!=2||!item||slot>2||item.hand!=slot||(flags&~0x20u))return false;
 if(!f.left&&!f.right&&!f.both)return true;
 return !f.left&&!f.both&&f.right.pistol&&item.hand==0&&item.throwable;
}
struct State {
 enum Phase {Idle,AwaitLamp,Held,AwaitEmpty,AwaitRestore}phase{Idle};
 Key hero{},inventory{},lampInstance{},previousInstance{};Signature lamp{},previous{};unsigned started{},missingSince{};
 Key pickupHero{},pickupInventory{},pickupPistol{},pickupBaseline{};Signature pickup{};unsigned pickupStarted{},pickupRevision{};
 void ClearLamp(){phase=Idle;hero=inventory=lampInstance=previousInstance={};lamp=previous={};started=missingSince=0;}
 void ClearPickup(){pickupHero=pickupInventory=pickupPistol=pickupBaseline={};pickup={};pickupStarted=pickupRevision=0;}
 void Clear(){ClearLamp();ClearPickup();}
 static const Item* UniqueStock(const Frame& f,const Signature& signature){
  const Item* found=nullptr;for(const auto& item:f.stock)if(signature.Matches(item)){if(found)return nullptr;found=&item;}return found;
 }
 bool SameOwner(const Frame& f)const{return f.hero&&f.inventory&&hero==f.hero&&inventory==f.inventory;}
 // The adapter calls this only for accepted native world pickup requests.
 void Picked(const Frame& f,const Item& item){
  if(!f.eligible||(f.right&&!f.right.pistol)||f.left||f.both||item.hand!=0||!item.throwable)return;
  unsigned matches=0;for(const auto& stock:f.stock)if(Signature::Of(item).Matches(stock))++matches;if(matches>1)return;
  pickupHero=f.hero;pickupInventory=f.inventory;pickupPistol=f.right.key;pickup=Signature::Of(item);pickupStarted=f.tick;
  if(const auto* existing=UniqueStock(f,pickup)){pickupBaseline=existing->key;pickupRevision=existing->revision;}else{pickupBaseline={};pickupRevision=0;}
 }
 Command Toggle(const Frame& f){
  if(!f.eligible||f.queue||!f.hero||!f.inventory)return {};
  if(phase==AwaitLamp||phase==AwaitEmpty||phase==AwaitRestore)return {};
  if(f.left.lamp){
   const bool ours=phase==Held&&SameOwner(f)&&lampInstance==f.left.key;
   if(!ours){ClearLamp();hero=f.hero;inventory=f.inventory;lamp=Signature::Of(f.left);}
   phase=AwaitEmpty;started=f.tick;ClearPickup();return {0,0,f.left.key};
  }
  const Item* light=nullptr;for(const auto& item:f.stock)if(item.lamp&&item.hand==0){if(light)return {};light=&item;}
  if(!light)return {};
  ClearLamp();hero=f.hero;inventory=f.inventory;lamp=Signature::Of(*light);previous=Signature::Of(f.both?f.both:f.left);previousInstance=(f.both?f.both:f.left).key;phase=AwaitLamp;started=f.tick;ClearPickup();return {1,0,light->key};
 }
 Command Update(const Frame& f){
  if(phase!=Idle&&!SameOwner(f))ClearLamp();
  if(phase!=Idle){
   if(phase!=Held&&f.tick-started>10000){ClearLamp();}
   else if(phase==AwaitLamp){
    // Native stock materialization may replace the queued template's GUID.
    if(f.left.lamp&&lamp.Matches(f.left)&&!f.both){lampInstance=f.left.key;phase=Held;}
    else if(!f.queue&&f.tick-started>1000&&((f.left&&!previous.Matches(f.left))||f.both))ClearLamp();
   }else if(phase==Held){
    if(f.both||(f.left&&f.left.key!=lampInstance))ClearLamp();
    else if(!f.left&&!f.queue){if(!missingSince)missingSince=f.tick;else if(f.tick-missingSince>1000)ClearLamp();}
    else missingSince=0;
   }else if(phase==AwaitEmpty){
    if(f.both||(f.left&&!lamp.Matches(f.left))){ClearLamp();}
    else if(f.eligible&&!f.queue&&!f.left){
     if(!previous.valid){ClearLamp();}
     else if(const auto* item=(f.remembered.key==previousInstance&&previous.Matches(f.remembered))?&f.remembered:UniqueStock(f,previous)){
      // Do not replace a weapon or another hand chosen after the Q gesture.
      if(item->hand>2||(item->hand==1&&f.right)||(item->hand==2&&f.right)){ClearLamp();}
      else{phase=AwaitRestore;started=f.tick;return {1,item->hand,item->key};}
     }else if(f.tick-started>2000){ClearLamp();}
    }
   }else if(phase==AwaitRestore){
    const Item& hand=previous.hand==2?f.both:previous.hand==1?f.right:f.left;
    if(previous.Matches(hand)){ClearLamp();}
    else if(hand&&!f.queue&&f.tick-started>1000)ClearLamp();
   }
  }
  if(pickup.valid){
   if(!f.eligible||f.hero!=pickupHero||f.inventory!=pickupInventory||f.right.key!=pickupPistol||(f.right&&!f.right.pistol)||f.both||f.tick-pickupStarted>5000){ClearPickup();return {};}
   if(f.left){ClearPickup();return {};}
   if(f.eligible&&!f.queue&&phase==Idle){
    if(const auto* item=UniqueStock(f,pickup))if(!pickupBaseline||item->key!=pickupBaseline||item->revision!=pickupRevision){const Command out{1,0,item->key};ClearPickup();return out;}
    // Allow a bounded native materialization interval after its queue drains.
    if(f.tick-pickupStarted>1000)ClearPickup();
   }
  }
  return {};
 }
};
} // namespace inventory_assist
