#!/usr/bin/env python3
"""Check generated ROM/URL/XML/map coherence; no hardware or GenApi runtime required."""
from pathlib import Path
import importlib.util
import re
import os
import subprocess
import tempfile
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('generator', root / 'tools/generate_gige_xml.py')
gen = importlib.util.module_from_spec(spec)
import sys
sys.path.insert(0,str(root/'tools'))
spec.loader.exec_module(gen)
files = [root / 'gige/MaxCamera.xml', root / 'gige/GigeVisionXmlRom.h', root / 'gige/GigeFeatureRegisters.h']
before = [p.read_bytes() for p in files]
gen.main()
assert before == [p.read_bytes() for p in files], 'Generated artifacts were stale; regenerated, rerun check'
xml = files[0].read_bytes()
header = files[1].read_text()
url = re.search(r'#define GIGE_XML_URL "([^"]+)"', header)[1]
filename, address, length = url.removeprefix('Local:').split(';')
assert filename.startswith('MaxCamera-') and filename.endswith('.xml')
assert int(length, 16) == len(xml)
# Verify actual compiled bytes rather than depending on initializer formatting.
with tempfile.TemporaryDirectory(prefix='maxcamera-rom-') as directory:
    executable = str(Path(directory) / 'rom')
    source = '#include <stdio.h>\n#include "GigeVisionXmlRom.h"\nint main(void){fwrite(gige_xml_rom,1,sizeof(gige_xml_rom),stdout);return 0;}\n'
    subprocess.run(['cc','-x','c','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'gige'),'-','-o',executable],input=source.encode(),check=True)
    assert subprocess.check_output([executable]) == xml+b'\0'
ns = {'g': 'http://www.genicam.org/GenApi/Version_1_1'}
tree = ET.fromstring(xml)
nodes = {n.attrib['Name']: n for n in tree if 'Name' in n.attrib}
assert len(nodes) == len(tree), 'Duplicate node names'
for node in tree.iter():
    tag = node.tag.split('}')[-1]
    if tag in {'pFeature', 'pValue', 'pPort', 'pIsAvailable', 'pIsLocked', 'pVariable'}:
        assert node.text in nodes, f'Unresolved {tag}: {node.text}'
for name in ['Width', 'Height', 'PixelFormat', 'PayloadSize', 'AcquisitionMode',
             'AcquisitionStart', 'AcquisitionStop', 'TLParamsLocked']:
    assert name in nodes
assert nodes['WidthMax'].find('g:pValue',ns).text=='WidthMaximum'
assert nodes['HeightMax'].find('g:pValue',ns).text=='HeightMaximum'
for name in ('Width','Height','PixelFormat'):
    assert nodes[name+'Register'].find('g:AccessMode',ns).text=='RW'
    assert nodes[name].find('g:pIsLocked',ns).text=='ImageConfigurationLocked'
assert len(nodes['PixelFormat'].findall('g:EnumEntry',ns))==17  # 15 layouts + 2 legacy aliases
assert nodes['AcquisitionStart'].find('g:CommandValue', ns).text == '1'
assert nodes['AcquisitionStop'].find('g:CommandValue', ns).text == '0'
addresses = [int(n.find('g:Address', ns).text, 0) for n in tree
             if n.tag.split('}')[-1]=='IntReg' and n.find('g:Address', ns) is not None]
assert len(addresses) == len(set(addresses))
assert all(a % 4 == 0 and a < int(address, 16) for a in addresses)
schema = Path(os.environ.get('GENAPI_SCHEMA', 'schema-not-installed.xsd'))
if schema.exists():
    subprocess.run(['xmllint','--noout','--schema',str(schema),str(files[0])],check=True)
# Compile the public headers as C and C++ without any Xilinx dependencies.
source = '''#include "GigeVisionDefaults.h"
#include "GigeVisionXmlRom.h"
typedef char valid_payload[GIGE_UNIT_PAYLOAD_SIZE == 2592U*1944U*2U ? 1 : -1];
typedef char valid_stride[GIGE_UNIT_INPUT_STRIDE >= GIGE_UNIT_WIDTH*2U ? 1 : -1];
typedef char valid_xml[sizeof(gige_xml_rom) == GIGE_XML_LENGTH+1U ? 1 : -1];
int main(void) { return gige_xml_rom[0] != '<'; }
'''
with tempfile.TemporaryDirectory() as tmp:
    for compiler, language in [('cc', 'c'), ('c++', 'c++')]:
        binary = str(Path(tmp) / language.replace('+', 'p'))
        subprocess.run([compiler, '-Wall', '-Wextra', '-Werror', '-x', language,
                        '-I', str(root / 'gige'), '-', '-o', binary],
                       input=source, text=True, check=True)
        subprocess.run([binary], check=True)
print('PASS: reproducible XML, ROM, URL, node references, register alignment, C/C++ headers')
