#pragma once
namespace sss_hotkey {
// Sample the held bit, never the shared GetAsyncKeyState pressed-since bit.
// Focus changes and keys already held at startup must not toggle the setting.
struct State {
 bool focused=false,previous=false;
 bool Update(bool down,bool foreground,bool modified=false) noexcept {
  if(!foreground){focused=false;previous=down;return false;}
  if(!focused){focused=true;previous=down;return false;}
  const bool edge=down&&!previous;previous=down;return edge&&!modified;
 }
};
}
