// Read-only diagnostic harness using public CipherNES core API.
// Input script: start_frame,end_frame,port1_mask,port2_mask (1-based, inclusive).
// Dumps unmodified ARGB32 framebuffer as RGB PPM for subsequent lossless PNG.
#include "ciphernes_core.h"
#include "debug_bridge.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <sys/stat.h>
struct Event { int lo,hi; uint8_t p1,p2; };
static uint32_t crc32(const void* data,size_t n) {
    uint32_t c=0xFFFFFFFFu; const uint8_t* b=static_cast<const uint8_t*>(data);
    for(size_t i=0;i<n;++i){c^=b[i];for(int j=0;j<8;++j)c=(c>>1)^((c&1u)?0xEDB88320u:0u);}return ~c;
}
static uint32_t imageCrc(CIPHERNES_Handle* core) {return crc32(ciphernes_get_framebuffer(core),256u*240u*4u);}
static int distinct(CIPHERNES_Handle* core) {
    const uint32_t* p=ciphernes_get_framebuffer(core);
    std::vector<uint32_t> v(p,p+256u*240u);std::sort(v.begin(),v.end());
    return int(std::unique(v.begin(),v.end())-v.begin());
}
static void screenshot(const std::string& filename,CIPHERNES_Handle* core){
    std::ofstream out(filename.c_str(),std::ios::binary);
    out<<"P6\n256 240\n255\n";
    const uint32_t* fb=ciphernes_get_framebuffer(core);
    for(size_t i=0;i<256u*240u;++i){
        char rgb[]={char((fb[i]>>16)&255u),char((fb[i]>>8)&255u),char(fb[i]&255u)};
        out.write(rgb,3);
    }
}
static std::vector<Event> parseSchedule(const std::string& filename) {
    std::vector<Event> events; if(filename=="-")return events;
    std::ifstream in(filename.c_str());if(!in)throw std::string("Cannot open input schedule");
    std::string line; int n=0;
    while(std::getline(in,line)){
        ++n;if(line.empty()||line[0]=='#'||line.find("start_frame")==0)continue;
        std::replace(line.begin(),line.end(),',',' ');
        std::istringstream ss(line);std::string p1,p2;Event e{};
        if(!(ss>>e.lo>>e.hi>>p1>>p2))throw std::string("Malformed input schedule line ")+std::to_string(n);
        const unsigned x=std::stoul(p1,nullptr,0),y=std::stoul(p2,nullptr,0);
        if(e.lo<1||e.hi<e.lo||x>255||y>255)throw std::string("Invalid schedule range/masks");
        e.p1=uint8_t(x);e.p2=uint8_t(y);events.push_back(e);
    }
    std::sort(events.begin(),events.end(),[](const Event&a,const Event&b){return a.lo<b.lo;});
    for(size_t i=1;i<events.size();++i)if(events[i].lo<=events[i-1].hi)throw std::string("Overlapping input intervals");
    return events;
}
static void inputFor(const std::vector<Event>& events,int frame,uint8_t& p1,uint8_t& p2){
    p1=p2=0;for(const auto& e:events)if(frame>=e.lo&&frame<=e.hi){p1=e.p1;p2=e.p2;break;}
}
int main(int argc,char** argv) {
    if(argc<5){std::cerr<<"Usage: frame_capture ROM.nes OUT_DIR CHECKPOINTS_CSV INPUT_SCHEDULE_CSV_OR_DASH\n";return 2;}
    try {
        const std::string outdir=argv[2];
        std::vector<int> checkpoints; std::istringstream in(argv[3]);std::string x;
        while(std::getline(in,x,',')) {int k=std::stoi(x);if(k<1||k>36000)throw std::string("Invalid checkpoint frame"); checkpoints.push_back(k);}
        if(checkpoints.empty())throw std::string("Empty checkpoints");
        std::sort(checkpoints.begin(),checkpoints.end());
        if(std::unique(checkpoints.begin(),checkpoints.end())!=checkpoints.end())throw std::string("Duplicate checkpoints");
        const auto script=parseSchedule(argv[4]);
        CIPHERNES_Handle* h=ciphernes_create();if(!h)throw std::string("Could not allocate core");
        if(!ciphernes_load_rom(h,argv[1])){std::cerr<<"ROM rejected: "<<argv[1]<<"\n";ciphernes_destroy(h);return 3;}
        ciphernes_set_audio_sample_rate(h,44100.0);
        std::ofstream csv(outdir+"/checkpoints.csv");
        if(!csv){ciphernes_destroy(h);throw std::string("Cannot open checkpoint CSV");}
        csv<<"frame,master_ticks_total,crc32_argb_hex,distinct_colors,port1_hex,port2_hex,pc_hex,cpu_bus_hex,ppu_scanline,ppu_cycle,cpu_divider,nmi_pending,dma_active,screenshot_ppm\n";
        uint64_t totalTicks=0;size_t cp=0;
        for(int frame=1;frame<=checkpoints.back();++frame){
            uint8_t p1=0,p2=0;inputFor(script,frame,p1,p2);
            totalTicks+=ciphernes_run_frame_with_input(h,p1,p2);
            if(frame==checkpoints[cp]){
                const std::string filename="frame_"+std::to_string(frame)+".ppm";
                screenshot(outdir+"/"+filename,h);
                char crc[12],b1[5],b2[5];
                std::snprintf(crc,sizeof(crc),"%08X",imageCrc(h));
                std::snprintf(b1,sizeof(b1),"0x%02X",p1);std::snprintf(b2,sizeof(b2),"0x%02X",p2);
                CoreDebugSnapshot dbg{}; ciphernes_debug_sample(h,&dbg);
                char pcText[8],busText[8]; std::snprintf(pcText,sizeof(pcText),"%04X",dbg.pc); std::snprintf(busText,sizeof(busText),"%04X",dbg.cpu_address_bus);
                csv<<frame<<','<<totalTicks<<','<<crc<<','<<distinct(h)<<','<<b1<<','<<b2<<','<<pcText<<','<<busText<<','<<dbg.scanline<<','<<dbg.ppu_cycle<<','<<int(dbg.cpu_divider)<<','<<int(dbg.nmi_pending)<<','<<int(dbg.dma_active)<<','<<filename<<'\n';
                if(++cp==checkpoints.size())break;
            }
        }
        csv.close();
        // Repeat from the final state using transient save/load; validates
        // consistent CPU/PPU/mapper state replay without touching disk saves.
        std::vector<uint8_t> state(ciphernes_get_state_size(h)+4096u);
        const uint32_t saved=ciphernes_save_state_transient(h,state.data(),uint32_t(state.size()));
        if(!saved){ciphernes_destroy(h);throw std::string("Transient state save failed");}
        uint64_t aheadTicks=0;for(int j=1;j<=24;++j){uint8_t p1=0,p2=0;inputFor(script,checkpoints.back()+j,p1,p2);aheadTicks+=ciphernes_run_frame_with_input(h,p1,p2);}
        const uint32_t first=imageCrc(h);
        if(!ciphernes_load_state(h,state.data(),saved)){ciphernes_destroy(h);throw std::string("Transient state load failed");}
        uint64_t replayTicks=0;for(int j=1;j<=24;++j){uint8_t p1=0,p2=0;inputFor(script,checkpoints.back()+j,p1,p2);replayTicks+=ciphernes_run_frame_with_input(h,p1,p2);}
        const uint32_t second=imageCrc(h);
        std::ofstream replay(outdir+"/replay.csv");replay<<"frames,first_crc32,replay_crc32,first_ticks,replay_ticks,verdict\n";
        char a[16],b[16];std::snprintf(a,sizeof(a),"%08X",first);std::snprintf(b,sizeof(b),"%08X",second);
        const bool ok=first==second&&aheadTicks==replayTicks;
        replay<<24<<','<<a<<','<<b<<','<<aheadTicks<<','<<replayTicks<<','<<(ok?"PASS":"FAIL")<<'\n';
        ciphernes_destroy(h);
        return 0; // Keep observations when replay timing differs; CSV verdict remains FAIL.
    }catch(const std::exception& ex){std::cerr<<"Diagnostic error: "<<ex.what()<<'\n';return 2;}
     catch(const std::string& ex){std::cerr<<"Diagnostic error: "<<ex<<'\n';return 2;}
}
