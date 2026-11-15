// CipherNES: strict state-serialization and frame-replay regression.
// External ROM argument; source ZIP must remain ROM-free.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "ciphernes_core.h"
static uint32_t crc32frame(CIPHERNES_Handle* c) {
  const uint8_t* b=reinterpret_cast<const uint8_t*>(ciphernes_get_framebuffer(c));
  uint32_t crc=0xFFFFFFFFu;
  for(size_t i=0;i<256u*240u*4u;++i){crc^=b[i];for(int k=0;k<8;++k)crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);}
  return ~crc;
}
static CIPHERNES_Handle* load(const char* name) {
  CIPHERNES_Handle* h=ciphernes_create();if(!h)return nullptr;
  if(!ciphernes_load_rom(h,name)){ciphernes_destroy(h);return nullptr;}
  ciphernes_set_audio_sample_rate(h,44100.0);return h;
}
int main(int argc,char**argv) {
  if(argc<2){std::fprintf(stderr,"Usage: state_replay_regression <ROM>\n");return 2;}
  const int anchors[]={30,60,120,240,241,300};
  unsigned assertions=0, failures=0;
  auto check=[&](bool ok,const char*label,int anchor){++assertions;if(!ok){++failures;std::fprintf(stderr,"FAIL anchor=%d %s\n",anchor,label);}};
  for(int anchor:anchors){
    CIPHERNES_Handle* a=load(argv[1]);CIPHERNES_Handle* b=load(argv[1]);
    if(!a||!b){std::fprintf(stderr,"ROM not loadable: %s\n",argv[1]);return 3;}
    for(int i=0;i<anchor;++i)ciphernes_run_frame_with_input(a,0,0);
    std::vector<uint8_t> bytes(ciphernes_get_state_size(a)+4096u);
    uint32_t used=ciphernes_save_state_transient(a,bytes.data(),(uint32_t)bytes.size());
    check(used>64,"v12 saved",anchor);
    check(used>64&&bytes[4]==12&&bytes[5]==0&&bytes[6]==0&&bytes[7]==0,"version 12 header",anchor);
    check(used>2&&ciphernes_load_state(b,bytes.data(),used)==1,"v12 full state loads",anchor);
    CIPHERNES_Handle* truncated=load(argv[1]);
    check(!ciphernes_load_state(truncated,bytes.data(),used-1),"truncated v12 rejected",anchor);
    ciphernes_destroy(truncated);
    CIPHERNES_Handle* legacy=load(argv[1]);
    std::vector<uint8_t> v11(bytes.begin(),bytes.begin()+used-2);
    v11[4]=11;
    check(ciphernes_load_state(legacy,v11.data(),(uint32_t)v11.size())==1,"v11 legacy payload still accepted",anchor);
    ciphernes_destroy(legacy);
    std::vector<uint8_t> unsupported(bytes.begin(),bytes.begin()+used);
    unsupported[4]=99;
    CIPHERNES_Handle* unknown=load(argv[1]);
    check(!ciphernes_load_state(unknown,unsupported.data(),used),"unknown version rejected",anchor);
    ciphernes_destroy(unknown);
    uint64_t totalA=0,totalB=0;int diverged=0;
    for(int i=0;i<24;++i){
      const uint8_t p1 = i>=3 && i<9 ? 0x10:0; // exercise scripted Start after save
      totalA+=ciphernes_run_frame_with_input(a,p1,0);
      totalB+=ciphernes_run_frame_with_input(b,p1,0);
      if(crc32frame(a)!=crc32frame(b))++diverged;
    }
    check(totalA==totalB,"master ticks identical over 24 frames",anchor);
    check(diverged==0,"all frame CRCs identical",anchor);
    std::printf("anchor=%d replay_tick_diff=%lld frame_crc_divergences=%d\n",anchor,(long long)totalB-(long long)totalA,diverged);
    ciphernes_destroy(a);ciphernes_destroy(b);
  }
  std::printf("STATE_REPLAY %u assertions %u failures\n",assertions,failures);
  return failures?1:0;
}
