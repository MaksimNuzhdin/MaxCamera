#!/usr/bin/env python3
"""Execute the firmware's reset-register selection with isolated MMIO stubs."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root/'gige/GigeCamera.cpp').read_text()
start = source.index('    uint32_t reset_address=')
end = source.index('    usleep(1000);', start)
block = source[start:end]
with tempfile.TemporaryDirectory(prefix='led-reset-test-') as tmp:
    path = Path(tmp)/'reset.cpp'
    path.write_text('''#include <cassert>
#include <cstdint>
static const uint32_t GIGE_UNIT_VIDEO_RESET_GPIO=0xA0010000U;
static const uint32_t GIGE_UNIT_VIDEO_RESET_MASK=8U;
struct {int independent_stream_led;} fpga_isp;
static uint32_t address,values[2],initial;
static unsigned writes,delays;
uint32_t Xil_In32(uint32_t a) {assert(a==address);return initial;}
void Xil_Out32(uint32_t a,uint32_t v) {assert(a==address && writes<2);values[writes++]=v;}
void usleep(unsigned n) {assert(n==10 && writes==1);++delays;}
void pulse() {
''' + block + '''
}
int main() {
 for(int independent=0;independent<2;++independent) {
  fpga_isp.independent_stream_led=independent;
  address=0xA0010000U+(independent?8:0);
  for(uint32_t state=0;state<16;++state) {
   initial=state;writes=delays=0;pulse();
   const uint32_t mask=independent?1:8;
   assert(writes==2 && delays==1);
   assert(values[0]==(initial&~mask) && values[1]==(initial|mask));
   assert((values[0]&~mask)==(initial&~mask));
  }
 }
}
''')
    binary = Path(tmp)/'reset'
    subprocess.run(['c++','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    str(path),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS actual reset code: legacy channel1/bit3, new channel2/bit0, active-low pulse, preserved other bits')
