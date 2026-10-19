#!/usr/bin/env python3
"""Verify production board detection and safe ordering of shared INT pin setup."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "WaveshareHodiny/Touch_CST820.cpp").read_text()
start = source.index("bool Touch_DetectHardware()")
end = source.index("TouchController Touch_GetController()", start)
code = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
enum class TouchController : uint8_t { Unknown, CST820, GT911 };
TouchController controller;
constexpr int CST820_INT_PIN=16, OUTPUT=1, INPUT_PULLUP=2, LOW=0, HIGH=1;
constexpr int EXIO_PIN2=2, Low=0, High=1, CST820_ADDR=0x15, CST820_REG_ChipID=0xa7;
unsigned attempts=0, failures=0, outputs=0, gtProbes=0, resets=0;
bool cst=false,gt=false,resetOk=true;
void pinMode(int pin,int mode){assert(pin==16); if(mode==OUTPUT){++outputs; assert(attempts>=6);}}
void digitalWrite(int,int){}
void vTaskDelay(unsigned){}
unsigned pdMS_TO_TICKS(unsigned x){return x;}
bool Set_EXIO_Checked(int,int){++resets;return resetOk;}
bool I2C_Read_Touch(int address,int reg,uint8_t*,unsigned length,bool){
 assert(address==0x15 && reg==0xa7 && length==3);++attempts;
 return !cst || attempts<=failures;
}
bool GT911_Detect(){++gtProbes;return gt;}
void reset(){attempts=failures=outputs=gtProbes=resets=0;cst=gt=false;resetOk=true;}
""" + source[start:end] + r"""
int main(){
 reset();cst=true;assert(Touch_DetectHardware());assert(controller==TouchController::CST820);assert(outputs==0 && gtProbes==0);
 reset();cst=true;failures=5;assert(Touch_DetectHardware());assert(attempts==6 && outputs==0 && gtProbes==0);
 reset();gt=true;assert(Touch_DetectHardware());assert(controller==TouchController::GT911);assert(attempts==6 && outputs==1 && gtProbes==1);
 reset();assert(!Touch_DetectHardware());assert(controller==TouchController::Unknown && gtProbes==3);
 reset();resetOk=false;assert(!Touch_DetectHardware());assert(outputs==0 && attempts==0 && gtProbes==0);
 puts("PASS: board detection, CST820 boot retries, GPIO ordering, GT911 selection, unknown and expander failure");
}
"""
with tempfile.TemporaryDirectory(prefix="waveshare-board-") as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(code)
    subprocess.run(["clang++", "-std=c++17", "-fsanitize=address,undefined",
                    str(path / "test.cpp"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
