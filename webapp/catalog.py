"""Bounded web feature catalogue derived from the firmware source of truth."""
import sys
import os
import xml.etree.ElementTree as ET
from pathlib import Path
ROOT = Path(os.environ.get('MAXCAM_SOURCE', Path(__file__).resolve().parent.parent))
sys.path.insert(0, str(ROOT / 'SW/tools'))
from gige_feature_catalog import FEATURES
XML = ET.parse(ROOT / 'SW/gige/MaxCamera.xml')
for node in XML.iter():
    node.tag = node.tag.split('}')[-1]
NODES = {n.get('Name'): n for n in XML.getroot() if n.get('Name')}
DENIED = {'FactoryUserSetSave', 'FeatureAccessLock', 'DeviceReset', 'UserSetSave', 'UserSetDefault',
          'SensorFeatureApply', 'SensorFeatureMask', 'SensorFeatureValue'}
FEATURE_MAP = {f['name']: dict(f) for f in FEATURES}
for name, address, low, high in [('Width',0x10000,160,2592),('Height',0x10004,120,1944),('PayloadSize',0x1000c,0,16000000)]:
    FEATURE_MAP[name] = dict(name=name,address=address,minimum=low,maximum=high,access='RO' if name=='PayloadSize' else 'RW',group='ImageFormatControl')
pf = NODES['PixelFormat']
FEATURE_MAP['PixelFormat'] = dict(name='PixelFormat', address=0x10008,minimum=0,maximum=0xffffffff,access='RW',group='ImageFormatControl',entries={e.get('Name'):int(e.findtext('Value')) for e in pf.findall('EnumEntry')})
for f in FEATURE_MAP.values():
    node = NODES.get(f['name'])
    if node is not None:
        f['description'] = node.findtext('Description') or f.get('description','')
    f['remote_writable'] = f['access'] != 'RO' and f['name'] not in DENIED and f['group'] not in {'NetworkControl','DeviceMaintenance','SensorRegisters'}
    f['kind'] = f.get('kind', 'Enumeration' if 'entries' in f else 'Integer')

def public_catalog():
    return [{k:v for k,v in f.items() if k not in {'address','key'}} for f in FEATURE_MAP.values()]
