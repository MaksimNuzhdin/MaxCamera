#!/usr/bin/env python3
"""Compile the actual board indicator predicate without accessing hardware."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root/'gige/GigeVisionBoard.cpp').read_text()
start = source.index('int GigeVisionBoard_StreamIndicatorActive(void)')
end = source.index('}', start)+1
function = source[start:end]
with tempfile.TemporaryDirectory(prefix='led-state-') as tmp:
    path = Path(tmp)/'state.cpp'
    path.write_text('''#include <cassert>
struct {int active;} camera;
static int reset_pending,ready;
int GigeCamera_Ready(void) {return ready;}
''' + function + '''
int main() {
 for(int active=0;active<2;++active)
 for(int reset=0;reset<2;++reset)
 for(int sensor=0;sensor<2;++sensor) {
  camera.active=active;reset_pending=reset;ready=sensor;
  assert(GigeVisionBoard_StreamIndicatorActive()==(active && !reset && sensor));
 }
 // Control ownership/link alone cannot start the indicator; stopped drains
 // likewise leave camera.active false. Fault and pending reset override active.
 camera.active=0;ready=1;reset_pending=0;
 assert(!GigeVisionBoard_StreamIndicatorActive());
}
''')
    binary = Path(tmp)/'state'
    subprocess.run(['c++','-Wall','-Wextra','-Werror',str(path),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print('PASS actual streaming predicate: acquisition, stopped, fault and pending reset')
