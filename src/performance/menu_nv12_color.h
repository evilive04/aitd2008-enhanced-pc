#pragma once
namespace menu_nv12_color {
// MFVideoTransferMatrix / MFNominalRange enumeration values. Unknown matrix is
// explicitly BT709 per Microsoft's enum contract; unrecognized matrices fail.
inline bool Constants(unsigned matrix,unsigned range,float (&out)[8])noexcept{
    if(matrix>2||range<1||range>2)return false;
    const bool full=range==1;
    out[0]=full?1.f:255.f/219.f;out[1]=full?0.f:16.f/255.f;
    out[2]=full?1.f:255.f/224.f;out[3]=128.f/255.f;
    const bool bt601=matrix==2;
    out[4]=bt601?1.402f:1.5748f;out[5]=bt601?-.344136f:-.187324f;
    out[6]=bt601?-.714136f:-.468124f;out[7]=bt601?1.772f:1.8556f;
    return true;
}
inline bool ChromaOffset(unsigned siting,unsigned width,unsigned height,float (&out)[4])noexcept{
    if(!width||!height||(siting&~15u)||(siting&&!(siting&1)))return false;
    // Unknown metadata remains centered; no unsupported chroma alignment is
    // invented. Known horizontal/vertical cositing uses a half-luma-pixel shift.
    out[0]=(siting&4)? .5f/width:0;out[1]=(siting&2)? .5f/height:0;out[2]=out[3]=0;return true;
}
}
