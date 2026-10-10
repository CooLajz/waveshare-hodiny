#!/usr/bin/env python3
"""Exercise the production TLS deadline with delayed DNS/TCP/TLS/body mocks."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='radar-deadline-') as temp:
 p=Path(temp);(p/'lwip').mkdir()
 (p/'WiFiClientSecure.h').write_text(r'''
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
using std::min;
extern uint32_t ticks,dnsAt,tcpDelay,tlsDelay;extern void(*pending)();
inline uint32_t millis(){return ticks;}
inline void delay(int ms){ticks+=ms;if(pending&&ticks>=dnsAt){auto f=pending;pending=nullptr;f();}}
struct IPAddress {uint32_t value;IPAddress(uint32_t v=0):value(v){}};
struct Stream{unsigned streamTimeout=1000;void setTimeout(unsigned v){streamTimeout=v;}};
struct SSL{uint32_t handshake_timeout=0,socket_timeout=0;};
struct WiFiClientSecure:Stream {
 std::shared_ptr<SSL> sslclient=std::make_shared<SSL>();bool _stillinPlainStart=false;int _timeout=0;bool open=false;const char *_CA_cert="ca",*_cert=nullptr,*_private_key=nullptr;
 virtual int connect(const char*,uint16_t,int32_t){open=true;return 1;}
 int connect(IPAddress,uint16_t,const char* host,const char* ca,const char*,const char*){if(strcmp(host,"radar.nebovidy.cz")||strcmp(ca,"ca"))return 0;ticks+=min(uint32_t(_timeout),tcpDelay);open=tcpDelay<=uint32_t(_timeout);return open;}
 int startTLS(){ticks+=min(sslclient->handshake_timeout,tlsDelay);_stillinPlainStart=false;return tlsDelay<=sslclient->handshake_timeout;}
 virtual void stop(){open=false;}
 virtual int available(){return open?1:0;}virtual uint8_t connected(){return open;}
 virtual int read(){return open?'x':-1;}virtual int read(uint8_t*,size_t n){return open?n:-1;}
 virtual int peek(){return read();}virtual size_t write(const uint8_t*,size_t n){return open?n:0;}
 virtual size_t write(uint8_t){return open?1:0;}
};
''')
 (p/'lwip/dns.h').write_text(r'''
#pragma once
struct ip_addr_t{uint32_t addr=0x0100007f;};using err_t=int;
#define IP_IS_V4(a) true
#define ip_2_ip4(a) (a)
constexpr int ERR_OK=0,ERR_INPROGRESS=1;
extern bool dnsImmediate;
inline void dnsComplete();
inline int dns_gethostbyname(const char*,ip_addr_t*,void(*)(const char*,const ip_addr_t*,void*),void*){return dnsImmediate?ERR_OK:ERR_INPROGRESS;}
''')
 (p/'lwip/tcpip.h').write_text('#pragma once\ninline int tcpip_try_callback(void(*f)(void*),void* v){f(v);if(!dnsImmediate)pending=dnsComplete;return 0;}\n')
 (p/'test.cpp').write_text(r'''
#include "RadarTlsClient.h"
#include <cassert>
#include <iostream>
uint32_t ticks=100,dnsAt=0,tcpDelay=100,tlsDelay=100;void(*pending)()=nullptr;bool dnsImmediate=true;
inline void dnsComplete(){ip_addr_t address;radarDns::found(nullptr,&address,nullptr);}
void reset(){radarDns::lookup().state.store(0);pending=nullptr;ticks=100;tcpDelay=tlsDelay=100;dnsImmediate=true;}
int main(){
 RadarTlsClient c;
 reset();c.requestDeadline(true);assert(c.connect("radar.nebovidy.cz",18443,1000));ticks=c.began+2999;assert(c.available());++ticks;assert(!c.available()&&!c.connected()&&c.read()==-1&&c.write(uint8_t(1))==0);assert(c.streamTimeout==0);
 reset();dnsImmediate=false;dnsAt=5000;c.requestDeadline(true);assert(!c.connect("radar.nebovidy.cz",18443,1000));assert(ticks==850);delay(5000);assert(radarDns::lookup().state.load()==2);dnsImmediate=true;c.requestDeadline(true);assert(c.connect("radar.nebovidy.cz",18443,1000));
 reset();tcpDelay=9000;c.requestDeadline(true);assert(!c.connect("radar.nebovidy.cz",18443,1000));assert(ticks-c.began<=3000);
 reset();tlsDelay=9000;c.requestDeadline(true);assert(!c.connect("radar.nebovidy.cz",18443,1000));assert(ticks-c.began==3000);
 reset();ticks=UINT32_MAX-1000;c.requestDeadline(true);ticks+=3000;assert(c.expired());
 c.requestDeadline(false);assert(c.connect("opendata.chmi.cz",443,6000));ticks+=10000;assert(!c.expired());
 std::cout<<"PASS: aggregate deadline, DNS timeout/late callback, TCP/TLS delays, body cancellation, rollover and normal direct transport\n";
}
''')
 subprocess.run(['c++','-std=c++17','-I'+str(p),'-I'+str(ROOT/'WaveshareHodiny'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
