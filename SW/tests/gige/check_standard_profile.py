#!/usr/bin/env python3
"""Engineering SFNC/PFNC audit against normalized public EMVA references.
Not the A3 Validation Framework or a complete GigE Vision compliance test.
"""
import argparse
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--report', type=Path)
a = p.parse_args()
ref = json.loads((root/'tests/gige/standards-reference.json').read_text())
xml = ET.parse(root/'gige/MaxCamera.xml').getroot()
ns = {'g': xml.tag.split('}')[0][1:]}
nodes = {n.get('Name'): n for n in xml}
errors = []
audited = []
for n in xml:
    if n.get('NameSpace') != 'Standard':
        if n.get('Name') in ref['features']:
            errors.append(f"{n.get('Name')}: SFNC name incorrectly marked Custom")
        continue
    name = n.get('Name')
    tag = n.tag.split('}')[-1]
    standard = ref['features'].get(name)
    if not standard:
        errors.append(f'{name}: Standard feature absent from SFNC reference')
        continue
    if {'StringReg': 'String'}.get(tag, tag) != standard['type']:
        errors.append(f'{name}: wrong SFNC interface {tag}')
    if standard.get('deprecated') and n.findtext('g:Visibility', namespaces=ns) != 'Invisible':
        errors.append(f'{name}: deprecated feature must be Invisible')
    for e in n.findall('g:EnumEntry', ns):
        ename = e.get('Name')
        value = int(e.findtext('g:Value', namespaces=ns), 0)
        if name == 'PixelFormat':
            if value & 0x80000000:
                if e.get('NameSpace') != 'Custom':
                    errors.append(f'{ename}: custom PFNC code marked Standard')
            else:
                canonical = ref['legacy_pixel_aliases'].get(ename, ename)
                if ref['pfnc'].get(canonical) != value:
                    errors.append(f'{ename}: wrong PFNC identifier')
                if canonical != ename and e.findtext('g:Visibility', namespaces=ns) != 'Invisible':
                    errors.append(f'{ename}: legacy pixel alias must be Invisible')
        elif e.get('NameSpace') == 'Standard' and ename not in standard['entries']:
            # SFNC UserSetSelector/UserSetDefault explicitly allow UserSetN.
            if not (name in ('UserSetSelector', 'UserSetDefault') and re.fullmatch(r'UserSet[0-9]+', ename)):
                errors.append(f'{name}.{ename}: unknown Standard enumeration entry')
    audited.append(dict(name=name, interface=standard['type'], deprecated=standard.get('deprecated', False)))
for name in ['Root', 'Device', 'DeviceSFNCVersionMajor', 'DeviceSFNCVersionMinor',
             'DeviceSFNCVersionSubMinor', 'TLParamsLocked']:
    if name not in nodes:
        errors.append(f'Mandatory GenICam feature absent: {name}')
for name, unit in [('Timestamp', 'ns'), ('TimestampLatchValue', 'ns'),
                   ('DeviceLinkHeartbeatTimeout', 'us'), ('ExposureTime', 'us'),
                   ('AcquisitionFrameRate', 'Hz'), ('DeviceTemperature', 'C')]:
    if nodes[name].findtext('g:Unit', namespaces=ns) != unit:
        errors.append(f'{name}: wrong unit')
for name in ('Timestamp', 'TimestampLatchValue'):
    if nodes[name].findtext('g:Inc', namespaces=ns) != '1000':
        errors.append(f'{name}: software 1 us counter must advertise 1000 ns resolution')
for selector, selected in [('GainSelector', ['Gain', 'GainAuto']),
                           ('BalanceRatioSelector', ['BalanceRatio']),
                           ('UserSetSelector', ['UserSetLoad', 'UserSetSave', 'UserSetValid'])]:
    if not set(selected) <= {n.text for n in nodes[selector].findall('g:pSelected', ns)}:
        errors.append(f'{selector}: missing selection dependencies')
result = dict(passed=not errors, scope=__doc__.strip(), sfnc=ref['sfnc'],
              standard_features=len(audited), features=audited, errors=errors,
              sources={k: ref[k] for k in ['reference_url', 'reference_sha256', 'pfnc_url', 'pfnc_sha256']})
if a.report:
    a.report.write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(dict(passed=not errors, standard_features=len(audited), errors=errors)))
raise SystemExit(bool(errors))
