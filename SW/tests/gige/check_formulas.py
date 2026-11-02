#!/usr/bin/env python3
"""Execute XML expressions with Aravis' GenApi expression evaluator."""
import argparse
import json
import xml.etree.ElementTree as ET
from pathlib import Path
import gi
gi.require_version('Aravis', '0.8')
from gi.repository import Aravis

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--report', type=Path)
a = p.parse_args()
r = Path(__file__).resolve().parents[2]
xml = ET.parse(r/'gige/MaxCamera.xml').getroot()
ns = {'g': xml.tag.split('}')[0][1:]}
nodes = {n.get('Name'): n for n in xml}


def evaluate(node, formula, values):
    e = Aravis.Evaluator.new(formula)
    for name, value in values.items():
        if node.tag.endswith('IntSwissKnife'):
            e.set_int64_variable(name, value)
        else:
            e.set_double_variable(name, value)
    return e.evaluate_as_int64() if node.tag.endswith('IntSwissKnife') else e.evaluate_as_double()


count = 0
for n in xml:
    for f in list(n.findall('g:Formula', ns))+list(n.findall('g:FormulaTo', ns))+list(n.findall('g:FormulaFrom', ns)):
        values = {v.get('Name'): 1 for v in n.findall('g:pVariable', ns)}
        values.update(FROM=1000, TO=1000, P=32000, W=640, H=480, N=1, V=1000, T=12345)
        evaluate(n, f.text, values)
        count += 1
cases = [
    ('OrientationLocked', dict(B=0,R=1), 1),
    ('OrientationLocked', dict(B=0,R=0), 0),
    ('AutomaticWhiteBalanceAvailable', dict(R=1), 0),
    ('AutomaticWhiteBalanceAvailable', dict(R=0), 1),
    ('PixelFormatAvailable_Mono8', dict(W=640,H=480,N=1), 1),
    ('PixelFormatAvailable_BayerBG10p', dict(W=642,H=480,N=1), 0),
    ('PixelFormatAvailable_BayerBG10p', dict(W=640,H=480,N=1), 1),
    ('PixelFormatAvailable_YUV8_UYV', dict(W=2592,H=1944,N=1), 0),
    ('PixelFormatAvailable_YUV8_UYV', dict(W=640,H=480,N=1), 1),
    ('JpegModeAvailable', dict(W=640,H=480,N=1,F=0x02100032), 1),
    ('JpegModeAvailable', dict(W=640,H=480,N=1,F=0x01080001), 0),
    ('SensorIdle', dict(R=1,B=0), 1), ('SensorIdle', dict(R=1,B=1), 0),
    ('ExposureLocked', dict(B=0,A=0), 0), ('ExposureLocked', dict(B=0,A=2), 1),
    ('GainLocked', dict(B=1,A=0), 1), ('WhiteBalanceLocked', dict(B=0,A=2), 1),
    ('UserSetLoadLocked', dict(B=0,R=1,V=1,S=0,F=0), 1),
    ('UserSetLoadLocked', dict(B=0,R=1,V=1,S=0,F=1), 0),
    ('UserSetSaveLocked', dict(B=0,R=1,V=1,S=0,F=1), 1),
    ('UserSetSaveLocked', dict(B=0,R=1,V=1,S=7,F=0), 0),
    ('FactoryUserSetSaveLocked', dict(B=0,R=1,V=1,S=0,F=1), 0),
    ('FactoryUserSetSaveLocked', dict(B=0,R=1,V=1,S=7,F=1), 1),
]
# Exhaustive dynamic Black Level lock truth table, evaluated by GenApi.
for b in (0,1):
    for l in (0,1):
        for s in (0,1):
            for active in (0,1):
                cases.append(('FpgaBlackLevelLocked',dict(B=b,L=l,S=s,A=active),
                              int(s or ((b or l) and not active))))
for name, values, expected in cases:
    actual = evaluate(nodes[name], nodes[name].findtext('g:Formula', namespaces=ns), values)
    assert actual == expected, (name, values, actual, expected)
result = dict(passed=True, formulas=count, boundary_cases=len(cases), evaluator='Aravis 0.8')
if a.report:
    a.report.write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result))
