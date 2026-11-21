// Native fixture runner. The UI itself has no tool or platform dependencies.
#include "model.h"
#include "../../ui/art.h"
#include "../../ui/fonts.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <new>
#include <string>
#include <filesystem>
#include <limits>
using namespace fixture;
namespace fs=std::filesystem;
static unsigned allocs;
void* operator new(std::size_t n) {++allocs;if(auto p=std::malloc(n))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

namespace {
using Frame=std::array<uint32_t,width*height>;
void check(bool ok,const char* what) {
    if(!ok){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}
}
uint64_t hash(const Frame& f) {
    uint64_t h=14695981039346656037ull;
    for(auto v:f){h^=v;h*=1099511628211ull;}
    return h;
}
struct Rig {
    Screen screen;Model m;Host h=host();Frame frame{};
    explicit Rig(int id=0):m(model(id)) {}
    bool paint(bool all=false) {return screen.paint(m,h,frame.data(),width,all);}
    void key(Key k,int v=1,bool shift=false) {
        Edit edits[4]{};int n=0;screen.input(m,k,v,shift,edits,n);
        for(int j=0;j<n;++j) for(int i=0;i<m.rowCount;++i) if(m.rows[i].id==edits[j].id) {
            m.rows[i].value=edits[j].value;format(m.rows[i],m.machine);
        }
    }
    void settle(){paint();h.nowMs+=300;paint();}
};
struct State { const char* file;int machine,cursor; };
constexpr State states[]={
    {"01-fm-algo",0,1},{"02-ratio-edit",0,3},{"03-filter",0,9},{"04-amp",0,15},
    {"05-lfo",0,21},{"06-bd-playing",1,1},{"07-sd",2,1},{"08-cp",3,1},{"09-hh",4,1},
    {"10-cy-cb",5,1},{"11-efm",6,1},{"12-gnd",7,1},{"13-kit-slot-3",8,2},
    {"14-shift",0,2},{"15-pickup",0,2},{"16-extremes",7,1},
    {"17-coral",0,1},{"18-mint",0,1},{"19-violet",0,1},{"20-yellow",0,1}
};
void configure(Rig& r,int i) {
    const State& s=states[i];r.key(Key::Wheel,s.cursor);
    if(i==1) r.key(Key::Push);
    if(i==7) {r.m.rows[1].value=3/7.f;format(r.m.rows[1],3);}
    if(i==5 || i==12) {motion(r.m,2);r.h.playing=true;}
    if(i==13) r.h.shift=true;
    if(i==14) {r.h.pot[1]=4;r.h.engaged[1]=false;}
    if(i==15) {
        constexpr const char* values[]={"IMPULSE","-64","120ms","+63","2.4k","127","LP","+0.25"};
        for(int j=0;j<8;++j) {
            Row& p=r.m.rows[1+j];p.value=(j%2)?0.f:1.f;std::snprintf(p.text,sizeof p.text,"%s",values[j]);
        }
    }
    if(i>=16) {constexpr uint32_t accents[]={0xffff7a5c,0xff46d9b0,0xff9d8cff,0xffe8d94a};r.h.accent=accents[i-16];}
    r.settle();
}
void ppm(const fs::path& p,const Frame& f) {
    auto* file=std::fopen(p.c_str(),"wb");check(file,"open preview output");
    std::fprintf(file,"P6\n%d %d\n255\n",width,height);
    for(uint32_t v:f) {unsigned char rgb[]={static_cast<unsigned char>(v>>16),
        static_cast<unsigned char>(v>>8),static_cast<unsigned char>(v)};std::fwrite(rgb,1,3,file);}
    check(!std::fclose(file),"write preview");
}
struct Diff {int n=0,x0=width,y0=height,x1=-1,y1=-1;};
Diff difference(const Frame& a,const Frame& b) {
    Diff d;for(int y=0;y<height;++y) for(int x=0;x<width;++x) if(a[y*width+x]!=b[y*width+x]) {
        ++d.n;d.x0=std::min(d.x0,x);d.y0=std::min(d.y0,y);d.x1=std::max(d.x1,x);d.y1=std::max(d.y1,y);
    }return d;
}
void animation(const char* name,const fs::path* dir,int mode) {
    Rig r;r.key(Key::Wheel,1);r.settle();
    if(mode>=2){motion(r.m,0);r.paint();}
    Frame prev=r.frame;
    int peak=0;
    for(int i=0;i<18;++i) {
        if(i==1) {
            if(!mode) r.m=model(1);
            if(mode==1)r.key(Key::Wheel,1);
        }
        if(i && mode>=2)motion(r.m,i,mode!=3,mode!=2);
        r.h.nowMs+=16;
        bool rendered=r.paint();
        Diff d=difference(r.frame,prev);check(rendered==(d.n!=0),"dirty return matches pixels");
        // The switch installs the new parameter set immediately. All following sweep frames are header-only.
        if(!mode && i>1)check(!d.n || (d.y0>=2 && d.y1<37),"sweep contained in masthead");
        if(i && (mode || i>1))peak=std::max(peak,d.n);
        if(!mode && i==1)std::printf("machine-change rows+header: %d pixels\n",d.n);
        if(dir)ppm(*dir/(std::string(name)+"-"+std::to_string(i)+".ppm"),r.frame);
        Frame partial=r.frame;r.paint(true);check(partial==r.frame,"partial equals full at animation time");
        prev=r.frame;
    }
    std::printf("%s peak=%d pixels\n",name,peak);
    int budget=mode==1?12000:mode==2?1500:mode==3?6000:12000;
    check(peak>0 && peak<budget,"motion pixel budget");
}
void test() {
    Rig r;uint32_t ids[3];int n;Edit out[4]{};
    r.settle();auto before=r.frame;auto allocations=allocs;
    for(int i=0;i<30;++i) {++r.h.nowMs;check(!r.paint(),"idle paint false");}
    check(before==r.frame && !r.screen.animating(r.m,r.h),"idle stable and asleep");
    r.key(Key::Wheel,1); r.screen.knobs(r.m,ids);check(ids[0]==1 && ids[1]==2 && ids[2]==3,"wheel maps knobs");
    r.key(Key::Push);r.screen.input(r.m,Key::Wheel,1,false,out,n);
    check(n==1 && out[0].id==1 && std::abs(out[0].value-1/7.f)<1e-6,"one algorithm step");
    r.key(Key::Push);r.key(Key::Wheel,1);r.key(Key::Push);
    r.screen.input(r.m,Key::Wheel,1,true,out,n);check(n==1 && out[0].value==r.m.rows[2].value+1/1024.f,"fine wheel");
    r.screen.input(r.m,Key::Nudge,-1,true,out,n);check(n==1 && out[0].value==r.m.rows[2].value-1/128.f,"coarse nudge");
    r.screen.input(r.m,Key::Wheel,INT32_MAX,false,out,n);check(n==1 && out[0].value==1,"value clamps");
    r.key(Key::Push);r.key(Key::Nudge,1);r.screen.knobs(r.m,ids);check(ids[0]==9,"next group");
    r.key(Key::Nudge,-1);r.screen.knobs(r.m,ids);check(ids[0]==0,"previous group start");
    r.key(Key::Wheel,INT32_MAX);r.screen.knobs(r.m,ids);check(ids[0]==24 && ids[1]==UINT32_MAX && ids[2]==UINT32_MAX,"last row");
    r.key(Key::Push);r.m=model(7);r.settle();r.key(Key::Wheel,-1);
    r.screen.knobs(r.m,ids);check(ids[0]==23,"machine clamps and ends edit");
    r.m.rowCount=0;r.settle();r.screen.knobs(r.m,ids);check(ids[0]==UINT32_MAX,"empty");
    r.m=model(8);r.key(Key::Wheel,2);r.settle();r.m.kitSlot=4;r.m.kitMachines[4]=6;r.paint();
    check(allocations==allocs,"allocation-free paint/input/knobs");
    constexpr int stride=width+7;
    std::array<uint32_t,stride*height> pad;pad.fill(0x12345678);
    check(r.screen.paint(r.m,r.h,pad.data(),stride,true),"whole on poisoned buffer");
    for(int y=0;y<height;++y) {
        for(int x=0;x<width;++x)check(pad[y*stride+x]==r.frame[y*width+x],"whole redraw");
        for(int x=width;x<stride;++x)check(pad[y*stride+x]==0x12345678,"stride untouched");
    }
    pad.fill(0xa5a5a5a5);
    check(!r.screen.paint(r.m,r.h,pad.data(),stride,false),"idle on write sentinel");
    for(auto v:pad)check(v==0xa5a5a5a5,"idle touched a pixel");
    Rig alg;Frame frames[8];
    for(int i=0;i<8;++i) {alg.m.rows[1].value=i/7.f;alg.paint();frames[i]=alg.frame;
        for(int j=0;j<i;++j) {
            bool different=false;for(int y=65;y<149;++y)for(int x=202;x<320;++x)
                different|=frames[j][y*width+x]!=frames[i][y*width+x];
            check(different,"distinct routing geometry");
        }
    }
    std::puts("input, dirty/full/stride, eight graphs, allocation checks passed; idle=0 pixels");
}
void bench() {
    Rig r;r.settle();constexpr int rounds=2400;
    for(int mode=0;mode<4;++mode) {
        unsigned painted=0;
        auto t=std::chrono::steady_clock::now();
        for(int i=0;i<rounds;++i) {
            r.h.nowMs+=100;
            if(mode==1)r.key(Key::Wheel,i%2?-1:1);
            if(mode==2)r.m.voices[0]=i%2?.2f:.9f;
            painted+=r.paint(!mode);
        }
        double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()/rounds;
        constexpr const char* names[]={"full","cursor","meter","idle"};
        std::printf("%-7s %.6f ms/paint (%u changed)\n",names[mode],ms,painted);
    }
    constexpr size_t bytes=sizeof(font::body)+sizeof(font::bodyWidth)+sizeof(font::title)+sizeof(font::titleWidth)
        +sizeof(art::inks)+sizeof(art::icons)+sizeof(art::band);
    std::printf("compiled fonts and art: %zu bytes; Screen: %zu bytes\n",bytes,sizeof(Screen));
}
}
int main(int argc,char** argv) {
    std::string mode=argc>1?argv[1]:"--render";
    if(mode=="--bench"){bench();return 0;}
    fs::path dir=argc>2?argv[2]:"tools/ui_preview/out/frames";
    bool rendering=mode=="--render", golden=mode=="--hashes";
    check(rendering || golden || mode=="--test","unknown command");
    if(rendering)fs::create_directories(dir);
    FILE* hashes=nullptr;
    if(mode=="--test"){test();hashes=std::fopen("tools/ui_preview/hashes.txt","r");check(hashes,"open stable fixture hashes");}
    for(int i=0;i<int(std::size(states));++i) {
        Rig r(states[i].machine);configure(r,i);
        auto digest=static_cast<unsigned long long>(hash(r.frame));
        if(golden)std::printf("%s %016llx\n",states[i].file,digest);
        if(hashes) {
            char s[64]{};unsigned long long expected=0;
            check(std::fscanf(hashes,"%63s %llx",s,&expected)==2 && !std::strcmp(s,states[i].file) &&
                  expected==digest,states[i].file);
        }
        if(rendering)ppm(dir/(std::string(states[i].file)+".ppm"),r.frame);
    }
    if(hashes)std::fclose(hashes);
    if(!golden) {
        const char* names[]={"sweep","glide","meters","scope","voices-scope"};
        for(int i=0;i<5;++i)animation(names[i],rendering?&dir:nullptr,i);
        std::puts("20 states passed");
    }
}
