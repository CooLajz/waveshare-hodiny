#include "../WaveshareHodiny/SlovakRadarFrame.h"
#include <fstream>
#include <vector>
#include <random>
#include <cassert>
#include <iostream>
int main(int argc,char**argv){assert(argc==2);std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> original((std::istreambuf_iterator<char>(in)),{});nrd2::Frame f;assert(f.open(original.data(),original.size()));uint8_t out[800];assert(!f.row(550,out));for(int y=0;y<550;y++)assert(f.row(y,out) && f.validateRow(y));
for(size_t n:{0ul,31ul,2235ul,original.size()-1})assert(!f.open(original.data(),n));
std::mt19937 rng(42);for(int t=0;t<2000;t++){auto p=original;for(int k=0;k<4;k++)p[rng()%p.size()]=rng()%256;if(f.open(p.data(),p.size()))for(unsigned y=0;y<550;y++)if(f.validateRow(y)!=f.row(y,out))assert(false);}
std::cout<<"PASS: C++ decoder bounds, truncated files and 2000 deterministic mutations (ASan/UBSan)\n";
}
