#!/usr/bin/env python3
"""Generate GenApi XML + firmware ROM from the C defines (host C preprocessor)."""
import ast
import hashlib
import json
import uuid
from gige_feature_catalog import FEATURES
import os
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'gige'

def definitions(names):
    source = '\n'.join('#include "%s"' % h for h in (
        'GigeVisionRegisters.h', 'GigeVisionVendorDefines.h', 'GigeVisionUnitDefines.h'))
    source += '\n' + '\n'.join(names) + '\n'
    expanded = subprocess.run([os.environ.get('CC', 'cc'), '-E', '-P', '-x', 'c',
                               '-I', str(OUT), '-'], input=source, text=True,
                              check=True, capture_output=True).stdout.splitlines()
    values = {}
    for name, value in zip(names, expanded, strict=True):
        if value.startswith('"'):
            values[name] = ast.literal_eval(value)
        else:
            value = re.sub(r'(?<=\w)[uUlL]+\b', '', value)
            tree = ast.parse(value, mode='eval')
            allowed = (ast.Expression, ast.Constant, ast.BinOp, ast.Mult, ast.Add,
                       ast.Sub, ast.LShift, ast.RShift, ast.BitOr, ast.BitAnd)
            if any(not isinstance(n, allowed) for n in ast.walk(tree)):
                raise ValueError(value)
            values[name] = eval(compile(tree, '<C constant>', 'eval'), {'__builtins__': {}})
    return values


def main():
    reg_names = ['WIDTH', 'HEIGHT', 'PIXEL_FORMAT', 'PAYLOAD_SIZE', 'ACQUISITION_MODE',
                 'ACQUISITION_COMMAND', 'ACQUISITION_ACTIVE', 'TL_PARAMS_LOCKED',
                 'MANUFACTURER', 'MODEL', 'DEVICE_VERSION', 'MANUFACTURER_INFO', 'SERIAL']
    names = ['GIGE_REG_' + n for n in reg_names] + [
        'GIGE_VENDOR_NAME', 'GIGE_VENDOR_MODEL', 'GIGE_VENDOR_PRODUCT_GUID',
        'GIGE_VENDOR_XML_VERSION_GUID', 'GIGE_UNIT_WIDTH', 'GIGE_UNIT_HEIGHT',
        'GIGE_UNIT_WIDTH_MIN','GIGE_UNIT_HEIGHT_MIN','GIGE_UNIT_WIDTH_MAX','GIGE_UNIT_HEIGHT_MAX', 'GIGE_UNIT_PIXEL_FORMAT', 'GIGE_UNIT_PAYLOAD_SIZE', 'GIGE_XML_BASE', 'GIGE_XML_CAPACITY']
    d = definitions(names)
    root = ET.Element('RegisterDescription', {
        'ModelName': 'Camera_'+re.sub('[^0-9A-Za-z_]', '_', d['GIGE_VENDOR_MODEL']), 'VendorName': 'Vendor_'+re.sub('[^0-9A-Za-z_]', '_', d['GIGE_VENDOR_NAME']),
        'StandardNameSpace': 'None', 'SchemaMajorVersion': '1', 'SchemaMinorVersion': '1',
        'SchemaSubMinorVersion': '0', 'MajorVersion': '1', 'MinorVersion': '1',
        'SubMinorVersion': '1', 'ProductGuid': d['GIGE_VENDOR_PRODUCT_GUID'],
        'VersionGuid': d['GIGE_VENDOR_XML_VERSION_GUID'],
        'xmlns': 'http://www.genicam.org/GenApi/Version_1_1'})
    fingerprint = hashlib.sha256((Path(__file__).read_text() + json.dumps(FEATURES, sort_keys=True) + (ROOT/'src/Ov5640Registers.h').read_text() + json.dumps(d,sort_keys=True)).encode()).hexdigest()
    root.set('VersionGuid', str(uuid.uuid5(uuid.UUID(d['GIGE_VENDOR_XML_VERSION_GUID']),fingerprint)))
    def node(tag, name, namespace='Custom', **attrs):
        return ET.SubElement(root, tag, Name=name, NameSpace=namespace, **attrs)
    def child(parent, tag, value):
        ET.SubElement(parent, tag).text = str(value)
    groups = {
        'DeviceControl': ['DeviceVendorName', 'DeviceModelName', 'DeviceManufacturerInfo',
                          'DeviceVersion', 'DeviceID', 'DeviceSerialNumber', 'DeviceFirmwareVersion', 'DeviceUserID', 'DeviceSFNCVersionMajor', 'DeviceSFNCVersionMinor', 'DeviceSFNCVersionSubMinor', 'DeviceType', 'DeviceTemperature'],
        'ImageFormatControl': ['WidthMax', 'HeightMax', 'Width', 'Height', 'OffsetX', 'OffsetY', 'PixelFormat'],
        'AcquisitionControl': ['AcquisitionMode', 'AcquisitionStart', 'AcquisitionStop', 'ExposureAuto', 'ExposureTime'],
        'AnalogControl': ['Gain', 'GainSelector', 'GainAuto', 'BalanceWhiteAuto', 'BalanceRatioSelector', 'BalanceRatio'],
        'TransportLayerControl': ['PayloadSize', 'TLParamsLocked']}
    top = node('Category', 'Root', 'Standard')
    for group, features in groups.items():
        child(top, 'pFeature', group)
        category = node('Category', group, 'Standard')
        for feature in features:
            child(category, 'pFeature', feature)
    for name, reg, length in [('DeviceVendorName', 'MANUFACTURER', 32),
                              ('DeviceModelName', 'MODEL', 32),
                              ('DeviceVersion', 'DEVICE_VERSION', 32),
                              ('DeviceManufacturerInfo', 'MANUFACTURER_INFO', 48),
                              ('DeviceID', 'SERIAL', 16)]:
        n = node('StringReg', name, 'Standard')
        if name=='DeviceID':child(n,'Visibility','Invisible')
        for key, value in [('Address', hex(reg if isinstance(reg,int) else d['GIGE_REG_' + reg])), ('Length', length),
                           ('AccessMode', 'RO'), ('pPort', 'Device')]:
            child(n, key, value)
    def intreg(name, reg, access):
        n = node('IntReg', name)
        for key, value in [('Address', hex(reg if isinstance(reg,int) else d['GIGE_REG_' + reg])), ('Length', 4),
                           ('AccessMode', access), ('pPort', 'Device'),
                           ('Cachable', 'NoCache'), ('Sign', 'Unsigned'),
                           ('Endianess', 'BigEndian')]:
            child(n, key, value)
    for name, maximum, minimum, inc in [('Width',d['GIGE_UNIT_WIDTH_MAX'],d['GIGE_UNIT_WIDTH_MIN'],2),
                                         ('Height',d['GIGE_UNIT_HEIGHT_MAX'],d['GIGE_UNIT_HEIGHT_MIN'],2)]:
        n=node('Integer',name+'Max','Standard')
        child(n,'ImposedAccessMode','RO');child(n,'pValue',name+'Maximum');child(n,'Min',minimum);child(n,'Max',maximum)
        n=node('Integer',name,'Standard')
        child(n,'Description','Output dimensions. With FPGA processing/scaler enabled, InputWidth/InputHeight and ROI select sensor input; Polyphase supports bounded enlargement. Otherwise uses native sensor geometry/scaling. Stop before changes.')
        child(n,'pIsLocked','ImageConfigurationLocked')
        child(n,'pValue',name+'Register');child(n,'Min',minimum)
        child(n,'pMax',name+'Max')
        if name=='Width':child(n,'pInc','WidthIncrement')
        else:child(n,'Inc',inc)
        intreg(name+'Register',name.upper(),'RW')
    n=node('Integer','PayloadSize','Standard')
    child(n,'pValue','PayloadSizeRegister')
    intreg('PayloadSizeRegister','PAYLOAD_SIZE','RO')
    for name in ['OffsetX','OffsetY']:
        n=node('Integer',name,'Standard');child(n,'ImposedAccessMode','RO');child(n,'Value',0);child(n,'Min',0);child(n,'Max',0)
    n=node('Enumeration','PixelFormat','Standard')
    child(n,'Description','Native CSI capture or optional FPGA RAW10-derived RGB/BGR and YUV processing on available hardware. With FPGA processing disabled, RGB8/BGR8 use sensor YUV422 and CPU conversion. YUV444 remains limited to 640x480. Custom formats are documented byte layouts, not standard PFNC. RAW uses an unbinned crop with fixed BG phase; ReverseX/Y are locked in RAW. Output format and payload can change sensor cadence and Ethernet limits.')
    child(n,'pIsLocked','ImageConfigurationLocked')
    format_predicates=[]
    for entry,value in [('Mono8',0x01080001),('YUV422Packed',0x0210001f),('YUV422_YUYVPacked',0x02100032),
                        ('RGB8',0x02180014),('BGR8',0x02180015),('RGB565p',0x02100035),
                        ('BayerBG8',0x0108000b),('BayerBG10',0x0110000f),('BayerBG10p',0x010a0052),
                        ('YUV8_UYV',0x02180020),('RGB555_16BE',0x82100001),('RGB444_16BE',0x82100002),
                        ('YUV420Legacy',0x820c0003),('YUV422_YVYU',0x82100004),('YUV422_VYUY',0x82100005),
                        ('YUV422_8_UYVY',0x0210001f),('YUV422_8',0x02100032)]:
        e=ET.SubElement(n,'EnumEntry',Name=entry,NameSpace='Custom' if value&0x80000000 else 'Standard')
        if entry in ('YUV422Packed','YUV422_YUYVPacked'):child(e,'Visibility','Invisible')
        predicate='PixelFormatAvailable_'+entry
        child(e,'pIsAvailable',predicate)
        native=entry not in ('Mono8','YUV422Packed','YUV422_YUYVPacked','YUV422_8','YUV422_8_UYVY','RGB8','BGR8','RGB565p')
        alignment=16 if entry in ('BayerBG10','BayerBG10p') else 8 if entry=='YUV420Legacy' else 4 if entry=='YUV8_UYV' else 2
        formula=f'((W & {alignment-1}) = 0)'
        if native:formula+=' && N'
        if entry=='YUV8_UYV':formula+=' && (W <= 640) && (H <= 480)'
        format_predicates.append((predicate,formula))
        child(e,'Value',value)
    child(n,'pValue','PixelFormatRegister');intreg('PixelFormatRegister','PIXEL_FORMAT','RW')
    for predicate,formula in format_predicates:
        n=node('IntSwissKnife',predicate)
        for var,ref in [('W','WidthRegister'),('H','HeightRegister'),('N','NativePixelFormatsAvailableRegister')]:ET.SubElement(n,'pVariable',Name=var).text=ref
        child(n,'Formula',formula)
    n=node('IntSwissKnife','JpegModeAvailable')
    for var,ref in [('W','WidthRegister'),('H','HeightRegister'),('F','PixelFormatRegister'),('N','NativePixelFormatsAvailableRegister')]:ET.SubElement(n,'pVariable',Name=var).text=ref
    child(n,'Formula','N && (W = 640) && (H = 480) && (F = 0x02100032)')
    n=node('Enumeration','AcquisitionMode','Standard')
    e=ET.SubElement(n,'EnumEntry',Name='Continuous',NameSpace='Standard');child(e,'Value',1)
    child(n,'pValue','AcquisitionModeRegister');intreg('AcquisitionModeRegister','ACQUISITION_MODE','RO')
    n=node('IntSwissKnife','OrientationLocked')
    ET.SubElement(n,'pVariable',Name='B').text='SensorBusyRegister'
    ET.SubElement(n,'pVariable',Name='R').text='SensorRawActiveRegister'
    child(n,'Formula','B || R')
    n=node('IntSwissKnife','AutomaticWhiteBalanceAvailable')
    ET.SubElement(n,'pVariable',Name='R').text='SensorRawActiveRegister'
    child(n,'Formula','1 - R')
    n=node('IntSwissKnife','CompressionConfigurationLocked')
    ET.SubElement(n,'pVariable',Name='B').text='SensorBusyRegister'
    ET.SubElement(n,'pVariable',Name='L').text='TLParamsLockedRegister'
    child(n,'Formula','B || L')
    n=node('IntSwissKnife','ImageConfigurationLocked')
    ET.SubElement(n,'pVariable',Name='B').text='SensorBusyRegister'
    ET.SubElement(n,'pVariable',Name='L').text='TLParamsLockedRegister'
    ET.SubElement(n,'pVariable',Name='C').text='ImageCompressionModeRegister'
    child(n,'Formula','B || L || C')
    for name, value in [('AcquisitionStart', 1), ('AcquisitionStop', 0)]:
        n = node('Command', name, 'Standard')
        child(n, 'pValue', 'AcquisitionCommandRegister')
        child(n, 'CommandValue', value)
    intreg('AcquisitionCommandRegister', 'ACQUISITION_COMMAND', 'WO')
    n = node('Integer', 'TLParamsLocked', 'Standard')
    child(n, 'Visibility', 'Invisible')
    child(n, 'pValue', 'TLParamsLockedRegister')
    child(n, 'Min', 0)
    child(n, 'Max', 1)
    intreg('TLParamsLockedRegister', 'TL_PARAMS_LOCKED', 'RW')
    def category_feature(category,name):
        cat=next(x for x in root.findall('Category') if x.get('Name')==category)
        child(cat,'pFeature',name)
    for name,reg,access in [('DeviceSerialNumber',0xd8,'RO'),('DeviceFirmwareVersion',0x88,'RO'),('DeviceUserID',0xe8,'RW')]:
        n=node('StringReg',name,'Standard')
        if access=='RW':child(n,'pIsLocked','ConfigurationLocked')
        for key,value in [('Address',hex(reg)),('Length',32 if name=='DeviceFirmwareVersion' else 16),('AccessMode',access),('pPort','Device'),('Cachable','NoCache')]:child(n,key,value)
    # Advertise the naming convention actually targeted, rather than claiming
    # the newest SFNC solely because its document is available.
    for name,value in [('DeviceSFNCVersionMajor',2),('DeviceSFNCVersionMinor',8),('DeviceSFNCVersionSubMinor',0)]:
        n=node('Integer',name,'Standard');child(n,'Value',value)
    n=node('Enumeration','DeviceType','Standard');e=ET.SubElement(n,'EnumEntry',Name='Transmitter',NameSpace='Standard');child(e,'Value',0);child(n,'Value',0)
    for name,entry,value in [('DeviceScanType','Areascan',0),('DeviceTLType','GigEVision',0)]:
        category_feature('DeviceControl',name)
        n=node('Enumeration',name,'Standard');e=ET.SubElement(n,'EnumEntry',Name=entry,NameSpace='Standard');child(e,'Value',value);child(n,'Value',value)
    intreg('TransportVersionRegister',0,'RO')
    for name,formula in [('DeviceTLVersionMajor','V >> 16'),('DeviceTLVersionMinor','V & 65535'),('DeviceTLVersionSubMinor','0')]:
        category_feature('DeviceControl',name)
        n=node('Integer',name,'Standard');child(n,'pValue',name+'Value')
        n=node('IntSwissKnife',name+'Value');ET.SubElement(n,'pVariable',Name='V').text='TransportVersionRegister';child(n,'Formula',formula)
    category_feature('AcquisitionControl','ExposureMode')
    n=node('Enumeration','ExposureMode','Standard');e=ET.SubElement(n,'EnumEntry',Name='Timed',NameSpace='Standard');child(e,'Value',0);child(n,'Value',0)
    category_feature('ImageFormatControl','SensorWidth');category_feature('ImageFormatControl','SensorHeight')
    for name,value in [('SensorWidth',2592),('SensorHeight',1944)]:
        n=node('Integer',name,'Standard');child(n,'Value',value)
    category_feature('TransportLayerControl','GevInterfaceSelector')
    n=node('Integer','GevInterfaceSelector','Standard');child(n,'Value',0);child(n,'Min',0);child(n,'Max',0)
    for name,reg,access,rep in [('GevCurrentIPAddress',0x24,'RO','HexNumber'),('GevCurrentSubnetMask',0x34,'RO','HexNumber'),('GevCurrentDefaultGateway',0x44,'RO','HexNumber'),('GevPersistentIPAddress',0x64c,'RW','HexNumber'),('GevPersistentSubnetMask',0x65c,'RW','HexNumber'),('GevPersistentDefaultGateway',0x66c,'RW','HexNumber'),('GevHeartbeatTimeout',0x938,'RW','Linear'),('GevSCPD',0xd08,'RW','Linear'),('GevTimestampTickFrequency',0x940,'RO','Linear'),('GevSCDA',0xd18,'RW','HexNumber'),('GevSCPHostPort',0xd00,'RW','Linear')]:
        category_feature('TransportLayerControl',name)
        n=node('Integer',name,'Standard')
        if name in ('GevHeartbeatTimeout','GevTimestampTickFrequency'):child(n,'Visibility','Invisible')
        if name.startswith('GevPersistent'):child(n,'pIsLocked','ConfigurationLocked')
        elif name in ('GevSCPD','GevSCDA','GevSCPHostPort'):child(n,'pIsLocked','TLParamsLockedRegister')
        child(n,'pValue',name+'Register');child(n,'Min',500 if name=='GevHeartbeatTimeout' else 0)
        child(n,'Max',60000 if name=='GevHeartbeatTimeout' else 1000000 if name=='GevSCPD' else 65535 if name=='GevSCPHostPort' else 4294967295);child(n,'Representation',rep)
        intreg(name+'Register',reg,access)
    def masked(name,reg,bit,access):
        n=node('MaskedIntReg',name)
        for key,value in [('Address',hex(reg)),('Length',4),('AccessMode',access),('pPort','Device'),('Cachable','NoCache'),('Bit',31-bit),('Sign','Unsigned'),('Endianess','BigEndian')]:child(n,key,value)
    for name,bit in [('GevCurrentIPConfigurationPersistentIP',0),('GevCurrentIPConfigurationDHCP',1),('GevCurrentIPConfigurationLLA',2)]:
        category_feature('TransportLayerControl',name)
        n=node('Boolean',name,'Standard');child(n,'pIsLocked','ConfigurationLocked');child(n,'pValue',name+'Register');child(n,'OnValue',1);child(n,'OffValue',0)
        masked(name+'Register',0x14,bit,'RO' if bit==2 else 'RW')
    category_feature('TransportLayerControl','GevSCPSPacketSize')
    n=node('Integer','GevSCPSPacketSize','Standard');child(n,'pIsLocked','TLParamsLockedRegister');child(n,'pValue','GevSCPSPacketSizeRegister');child(n,'Min',576);child(n,'Max',1500)
    n=node('MaskedIntReg','GevSCPSPacketSizeRegister')
    for key,value in [('Address','0xd04'),('Length',4),('AccessMode','RW'),('pPort','Device'),('Cachable','NoCache'),('LSB',31),('MSB',16),('Sign','Unsigned'),('Endianess','BigEndian')]:child(n,key,value)
    for name,bit in [('GevSCPSDoNotFragment',30),('GevSCPSFireTestPacket',31)]:
        category_feature('TransportLayerControl',name)
        n=node('Boolean',name,'Standard');child(n,'pIsLocked','TLParamsLockedRegister');child(n,'pValue',name+'Register');child(n,'OnValue',1);child(n,'OffValue',0)
        masked(name+'Register',0xd04,bit,'RW')
    category_feature('TransportLayerControl','GevSCSP')
    n=node('Integer','GevSCSP','Standard');child(n,'pValue','GevSCSPRegister');child(n,'Min',0);child(n,'Max',65535)
    intreg('GevSCSPRegister',0xd1c,'RO')
    category_feature('TransportLayerControl','GevStreamChannelSelector')
    n=node('Integer','GevStreamChannelSelector','Standard');child(n,'Value',0);child(n,'Min',0);child(n,'Max',0)
    for selected in ['GevSCPD','GevSCDA','GevSCPHostPort','GevSCSP','GevSCPSPacketSize','GevSCPSDoNotFragment','GevSCPSFireTestPacket']:child(n,'pSelected',selected)
    for name,value in [('GevTimestampControlLatch',2),('GevTimestampControlReset',1)]:
        category_feature('TransportLayerControl',name)
        n=node('Command',name,'Standard');child(n,'Visibility','Invisible');child(n,'pValue','GevTimestampControlRegister');child(n,'CommandValue',value)
    intreg('GevTimestampControlRegister',0x944,'WO')
    category_feature('TransportLayerControl','GevTimestampValue')
    n=node('Integer','GevTimestampValue','Standard');child(n,'Visibility','Invisible');child(n,'pValue','GevTimestampValueRegister')
    n=node('IntReg','GevTimestampValueRegister')
    for key,value in [('Address','0x948'),('Length',8),('AccessMode','RO'),('pPort','Device'),('Cachable','NoCache'),('Sign','Unsigned'),('Endianess','BigEndian')]:child(n,key,value)
    for name,value in [('TimestampLatch',2),('TimestampReset',1)]:
        category_feature('DeviceControl',name)
        n=node('Command',name,'Standard');child(n,'pValue','GevTimestampControlRegister');child(n,'CommandValue',value)
    n=node('IntReg','CurrentTimestampRegister')
    for key,value in [('Address','0x10020'),('Length',8),('AccessMode','RO'),('pPort','Device'),('Cachable','NoCache'),('Sign','Unsigned'),('Endianess','BigEndian')]:child(n,key,value)
    for name,source in [('Timestamp','CurrentTimestampRegister'),('TimestampLatchValue','GevTimestampValueRegister')]:
        category_feature('DeviceControl',name)
        n=node('Integer',name,'Standard')
        child(n,'Description','Software device counter in nanoseconds with 1000 ns resolution. Frame timestamps mark DMA arm time, not exposure/SOF; no PTP synchronization is implemented.')
        child(n,'pValue',name+'Ns');child(n,'Min',0);child(n,'Inc',1000);child(n,'Unit','ns')
        n=node('IntSwissKnife',name+'Ns');ET.SubElement(n,'pVariable',Name='T').text=source;child(n,'Formula','T*1000')
    category_feature('DeviceControl','DeviceLinkHeartbeatTimeout')
    n=node('Float','DeviceLinkHeartbeatTimeout','Standard');child(n,'pValue','DeviceLinkHeartbeatTimeoutConverter');child(n,'Min',500000);child(n,'Max',60000000);child(n,'Unit','us')
    n=node('Converter','DeviceLinkHeartbeatTimeoutConverter');child(n,'FormulaTo','FROM/1000');child(n,'FormulaFrom','TO*1000');child(n,'pValue','GevHeartbeatTimeoutRegister');child(n,'Slope','Increasing')
    # Device controls use the same generated metadata as firmware dispatch.
    register_catalogue = re.findall(r'^#define OV5640_REG_(\w+) 0x([0-9A-Fa-f]+)U',
                                    (ROOT/'src/Ov5640Registers.h').read_text(), re.M)
    for group in dict.fromkeys(f['group'] for f in FEATURES):
        if group in groups:cat=next(x for x in root.findall('Category') if x.get('Name')==group)
        else:
            child(top,'pFeature',group)
            cat=node('Category',group,'Standard' if group in ('UserSetControl','TestControl') else 'Custom')
        if group=='SensorRegisters': child(cat,'Visibility','Guru')
        for f in FEATURES:
            if f['group']==group: child(cat,'pFeature',f['name'])
    # Lock expressions reference integer registers, avoiding ambiguous Boolean conversions.
    for name, formula, variables in [
        ('SensorIdle','R &amp;&amp; (B = 0)', [('R','SensorInitializedRegister'),('B','SensorBusyRegister')]),
        ('ExposureLocked','B || (A <> 0)',[('B','SensorBusyRegister'),('A','ExposureAutoRegister')]),
        ('GainLocked','B || (A <> 0)',[('B','SensorBusyRegister'),('A','GainAutoRegister')]),
        ('WhiteBalanceLocked','B || (A <> 0)',[('B','SensorBusyRegister'),('A','BalanceWhiteAutoRegister')])]:
        n=node('IntSwissKnife',name)
        for var,ref in variables: ET.SubElement(n,'pVariable',Name=var).text=ref
        child(n,'Formula',formula.replace('&amp;','&'))
    for f in FEATURES:
        name=f['name'];kind=f.get('kind','Integer');namespace='Standard' if f.get('standard') else 'Custom'
        if f.get('entries') or kind=='RegisterSelector': tag='Enumeration'
        elif kind in ('Float','TemperatureADC'): tag='Float'
        elif kind in ('Boolean','Command'): tag=kind
        else: tag='Integer'
        n=node(tag,name,namespace)
        if f.get('description'): child(n,'Description',f['description'])
        if f.get('available'): child(n,'pIsAvailable',f['available'])
        if f.get('locked'): child(n,'pIsLocked',f['locked'])
        if tag=='Enumeration':
            entries=f.get('entries') or {'Reg_'+key:int(value,16) for key,value in register_catalogue}
            for entry,value in entries.items():
                standard_entry=entry in ('Off','Continuous','Default','JPEG','Black') or (name in ('UserSetSelector','UserSetDefault') and re.fullmatch(r'UserSet[0-9]+',entry))
                e=ET.SubElement(n,'EnumEntry',Name=entry,NameSpace='Standard' if f.get('standard') and standard_entry else 'Custom')
                if name=='BalanceWhiteAuto' and entry=='Continuous':child(e,'pIsAvailable','AutomaticWhiteBalanceAvailable')
                if name=='ImageCompressionMode' and entry=='JPEG':child(e,'pIsAvailable','JpegModeAvailable')
                child(e,'Value',value)
        value_node=name+'Converter' if tag=='Float' else name+'Register'
        child(n,'pValue',value_node)
        if name=='UserSetSelector':
            for selected in ('UserSetLoad','UserSetSave','UserSetValid','FactoryUserSetSave'):child(n,'pSelected',selected)
        if tag=='Command': child(n,'CommandValue',1)
        elif tag=='Boolean':
            child(n,'OnValue',1);child(n,'OffValue',0)
        elif tag in ('Integer','Float'):
            if kind=='TemperatureADC': lo,hi=-40,125
            else:
                scale=f.get('scale',1);lo,hi=f['minimum']/scale,f['maximum']/scale
            if name=='AcquisitionFrameRate': child(n,'pMin','AcquisitionFrameRateMinimum')
            elif name=='ExposureTime': child(n,'pMin','SensorLinePeriod')
            else: child(n,'Min',int(lo) if tag=='Integer' else lo)
            if name=='ExposureTime': child(n,'pMax','ExposureTimeMaximum')
            elif name in ('ExposureLimit50Hz','ExposureLimit60Hz'): child(n,'pMax','ExposureMaximumLines')
            elif name=='AcquisitionFrameRate': child(n,'pMax','AcquisitionFrameRateMaximum')
            else: child(n,'Max',int(hi) if tag=='Integer' else hi)
            if tag=='Integer': child(n,'Inc',f.get('increment',1))
            if f.get('representation'): child(n,'Representation',f['representation'])
            if tag=='Float' and f.get('unit'): child(n,'Unit',f['unit'])
            if kind=='TemperatureADC': child(n,'Unit','C')
        for selected in f.get('selected',[]):child(n,'pSelected',selected)
        if tag=='Float':
            converter=node('Converter',name+'Converter')
            if kind=='TemperatureADC':
                to='(FROM+273.15)*1024/503.975';fr='TO*503.975/1024-273.15'
            elif name=='ExposureTime':
                ET.SubElement(converter,'pVariable',Name='P').text='SensorLinePeriodRegister'
                to='FROM*1000/P';fr='TO*P/1000'
            else: to=f"FROM*{f['scale']}";fr=f"TO/{f['scale']}"
            child(converter,'FormulaTo',to);child(converter,'FormulaFrom',fr)
            child(converter,'pValue',name+'Register')
            child(converter,'Slope','Increasing')
        intreg(name+'Register',f['address'],f['access'])
        if f.get('signed'): root[-1].find('Sign').text='Signed'
    category_feature('SensorControl','Gain');category_feature('SensorControl','GainSelector')
    n=node('Enumeration','GainSelector','Standard');e=ET.SubElement(n,'EnumEntry',Name='All',NameSpace='Standard');child(e,'Value',0);child(n,'Value',0);child(n,'pSelected','Gain');child(n,'pSelected','GainAuto')
    n=node('Float','Gain','Standard');child(n,'Description','Combined sensor gain as a linear multiplier, identical to GainFactor.');child(n,'pIsAvailable','SensorInitialized');child(n,'pIsLocked','GainLocked');child(n,'pValue','GainFactorConverter');child(n,'Min',1);child(n,'Max',1023/16);child(n,'Unit','x')
    category_feature('WhiteBalance','BalanceRatioSelector');category_feature('WhiteBalance','BalanceRatio')
    n=node('Enumeration','BalanceRatioSelector','Standard')
    for name,value in [('Red',0),('Green',1),('Blue',2)]:
        e=ET.SubElement(n,'EnumEntry',Name=name,NameSpace='Standard');child(e,'Value',value)
    child(n,'pValue','BalanceRatioSelectorValue');child(n,'pSelected','BalanceRatio')
    n=node('Integer','BalanceRatioSelectorValue');child(n,'Value',0);child(n,'Min',0);child(n,'Max',2)
    n=node('Float','BalanceRatio','Standard');child(n,'pIsAvailable','SensorInitialized');child(n,'pIsLocked','WhiteBalanceLocked');child(n,'pValue','BalanceRatioConverter');child(n,'Min',1/1024);child(n,'Max',4095/1024)
    n=node('Converter','BalanceRatioConverter');child(n,'FormulaTo','FROM*1024');child(n,'FormulaFrom','TO/1024');child(n,'pValue','BalanceRatioRegister');child(n,'Slope','Increasing')
    n=node('IntSwissKnife','BalanceRatioAddress');ET.SubElement(n,'pVariable',Name='S').text='BalanceRatioSelectorValue'
    wb=next(f['address'] for f in FEATURES if f['key']=='WB_RED');child(n,'Formula',f'{wb}+4*S')
    n=node('IntReg','BalanceRatioRegister')
    for key,value in [('pAddress','BalanceRatioAddress'),('Length',4),('AccessMode','RW'),('pPort','Device'),('Cachable','NoCache'),('Sign','Unsigned'),('Endianess','BigEndian')]:child(n,key,value)
    n=node('IntSwissKnife','ConfigurationLocked')
    for var,ref in [('B','SensorBusyRegister'),('L','TLParamsLockedRegister'),('S','UserSetBusyRegister')]:ET.SubElement(n,'pVariable',Name=var).text=ref
    child(n,'Formula','B || L || S')
    n=node('IntSwissKnife','FpgaBlackLevelLocked')
    for var,ref in [('B','SensorBusyRegister'),('L','TLParamsLockedRegister'),('S','UserSetBusyRegister'),('A','FpgaBlackLevelLiveActiveRegister')]:ET.SubElement(n,'pVariable',Name=var).text=ref
    child(n,'Formula','S || ((B || L) && (A = 0))')
    category_feature('NetworkControl','DeviceProvisioningRecord')
    n=node('Register','DeviceProvisioningRecord');child(n,'Description','Factory-only atomic 64-byte identity/network record. Provision using tools/gige_provision.py; applied after firmware reboot.');child(n,'pIsAvailable','FactoryAccessUnlocked');child(n,'pIsLocked','ConfigurationLocked')
    for key,value in [('Address','0x13000'),('Length',64),('AccessMode','RW'),('pPort','Device'),('Cachable','NoCache')]:child(n,key,value)
    for name,formula in [('UserSetLoadLocked','B || (R = 0) || (V = 0) || ((S = 0) && (F = 0))'),('UserSetSaveLocked','B || (R = 0) || (S = 0)'),('FactoryUserSetSaveLocked','B || (R = 0) || (S <> 0) || (F = 0)')]:
        n=node('IntSwissKnife',name)
        for var,ref in [('B','ConfigurationLocked'),('S','UserSetSelectorRegister'),('F','FactoryAccessUnlockedRegister'),('R','UserSetStorageAvailableRegister'),('V','UserSetValidRegister')]:ET.SubElement(n,'pVariable',Name=var).text=ref
        child(n,'Formula',formula)
    n=node('StringReg','FeatureAccessPassword')
    child(n,'Description','Write-only factory access password. Unlock expires after 300 seconds or when the controlling connection releases ownership. Password is never read back or saved in a UserSet.')
    for key,value in [('Address','0x12000'),('Length',32),('AccessMode','WO'),('pPort','Device'),('Cachable','NoCache')]:child(n,key,value)
    for category in root.findall('Category'):
        if category.get('Name')=='UserSetControl':child(category,'pFeature','FeatureAccessPassword')
    n=node('SwissKnife','ExposureTimeMaximum')
    ET.SubElement(n,'pVariable',Name='V').text='ExposureMaximumLinesRegister'
    ET.SubElement(n,'pVariable',Name='P').text='SensorLinePeriodRegister'
    child(n,'Formula','V*P/1000')
    node('Port', 'Device', 'Standard')
    # Address definitions + validation metadata for the board adapter, from the same catalogue.
    header=['/* Generated by tools/generate_gige_xml.py; do not edit. */',
            '#ifndef GIGE_FEATURE_REGISTERS_H','#define GIGE_FEATURE_REGISTERS_H','#include <stdint.h>']
    for f in FEATURES: header.append(f"#define GIGE_F_{f['key']} 0x{f['address']:08X}U")
    header+=['#define GIGE_FEATURE_FIRST 0x00011000U',f"#define GIGE_FEATURE_END 0x{0x11000+4*len(FEATURES):08X}U",
             'typedef struct { uint32_t address; int64_t minimum, maximum; uint8_t readable, writable, stopped; } GigeFeatureDescriptor;',
             'static const GigeFeatureDescriptor gige_feature_descriptors[] = {']
    for f in FEATURES:
        header.append(f"    {{{f['address']}U, {f['minimum']}LL, {f['maximum']}LL, {int(f['access']!='WO')}, {int(f['access']!='RO')}, {int(bool(f.get('locked')))}}},")
    header+=['};','#define GIGE_FEATURE_COUNT (sizeof(gige_feature_descriptors)/sizeof(gige_feature_descriptors[0]))',
             'static const uint16_t gige_sensor_register_catalogue[] = {']
    header+=['    '+','.join('0x'+v+'U' for _,v in register_catalogue[i:i+16])+',' for i in range(0,len(register_catalogue),16)]
    header+=['};','#define GIGE_SENSOR_REGISTER_COUNT (sizeof(gige_sensor_register_catalogue)/sizeof(gige_sensor_register_catalogue[0]))','#endif','']
    (OUT/'GigeFeatureRegisters.h').write_text('\n'.join(header))
    reference=['# GenICam feature reference','',
        'Generated from `tools/gige_feature_catalog.py`. Addresses are virtual GVCP words.',
        'Sensor writes that touch hardware require stopped acquisition. Staged tables can be edited while streaming and committed after Stop.',
        'Hardware status/auto-control telemetry is cached and refreshed one sensor register per20ms.',
        'Fan thresholds use the hardware10-bit thermal ADC. PWM applies when thermal hysteresis enables the fan; no RPM feedback is available.','',
        '| Feature | Group | Address | Access | Values / notes |','|---|---|---|---|---|']
    for f in FEATURES:
        notes=', '.join(f['entries']) if f.get('entries') else f.get('description','')
        if not notes: notes=str(f['minimum'])+'…'+str(f['maximum'])+' raw'
        reference.append(f"| {f['name']} | {f['group']} | 0x{f['address']:08X} | {f['access']} | {notes.replace('|','/')} |")
    reference+=['','## Register catalogue','',
        f'SensorRegisterSelector provides {len(register_catalogue)} named choices from Ov5640Registers.h. SensorRegisterValue reads the selected byte while stopped.',
        'SensorFeatureApply uses FeatureMask/FeatureValue and accepts stopped-only masked writes to AWB, CIP, CMX, gamma, SDE, LENC and BLC image-control ranges.',
        'PLL, reset, geometry, transport, OTP programming and MCU execution stay write-protected. The complete named register catalogue remains available for inspection.',
        'VCM controls are available only when GIGE_UNIT_VCM_AVAILABLE is provisioned for a verified actuator. JPEG uses the qualified CSI-byte production path, VGA YUYV and ImageCompressionMode=JPEG. FREX/MCU remain unavailable.','',
        'LED1: main-loop heartbeat (500ms high/500ms low). LED2: sensor/pipeline initialized and healthy. LED3: GVCP bound, Ethernet link up and usable IP address assigned.',
        'LED3 does not depend on whether a client is connected. LED4 blinks while acquiring on compatible version 10 and later images with independent video reset; it is off when stopped. Legacy images reserve LED4 for video reset.','']
    (OUT/'FeatureReference.md').write_text('\n'.join(reference))
    ET.indent(root, space='  ')
    data = ET.tostring(root, encoding='utf-8', xml_declaration=True) + b'\n'
    assert len(data) <= d['GIGE_XML_CAPACITY']
    (OUT / 'MaxCamera.xml').write_bytes(data)
    url = f"Local:MaxCamera-{fingerprint[:12]}.xml;{d['GIGE_XML_BASE']:x};{len(data):x}"
    lines = ['/* Generated by tools/generate_gige_xml.py; do not edit. */',
             '#ifndef GIGE_VISION_XML_ROM_H', '#define GIGE_VISION_XML_ROM_H',
             f'#define GIGE_XML_LENGTH {len(data)}U', f'#define GIGE_XML_URL "{url}"',
             'static const unsigned char gige_xml_rom[GIGE_XML_LENGTH + 1U] =']
    # Compact byte-exact string literals keep the generated source reviewable.
    # Three-digit octal escapes avoid ambiguity before an adjacent numeric byte.
    def c_byte(value):
        if value == 34: return '\\\"'
        if value == 92: return '\\\\'
        if value == 10: return '\\n'
        if 32 <= value < 127: return chr(value)
        return '\\%03o' % value
    for offset in range(0, len(data), 96):
        lines.append('    "' + ''.join(c_byte(b) for b in data[offset:offset+96]) + '"')
    lines += [';', '#endif', '']
    (OUT / 'GigeVisionXmlRom.h').write_text('\n'.join(lines))
    print(f'{len(data)} XML bytes; {url}')

if __name__ == '__main__':
    main()
