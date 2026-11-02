"""Synthetic fixtures test the analyzer, not sensor settling or optical quality."""
import copy
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
from analyze_plateaus import analyze


class PlateauTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.manifest = dict(schema=1, format='RAW10LE16', width=4, height=4,
            black=16, white=1000, manual_exposure=True, manual_gain=True,
            firmware_sha256='a'*64, bitstream_sha256='b'*64, patch=[0,0,4,4],
            maximum_relative_drift=.05, configuration_generation=3, plateaus=[])
        for phase,lines in enumerate([100,400,100]):
            frames=[]
            for i in range(3):
                seq=phase*3+i
                data=struct.pack('<16H', *([16+lines]*16))
                name=f'frame-{seq}.raw';(self.root/name).write_bytes(data)
                frames.append(dict(file=name,sha256=hashlib.sha256(data).hexdigest(),
                    capture_sequence=seq,host_receive_monotonic_ns=1000+seq,
                    configuration_generation=3))
            self.manifest['plateaus'].append(dict(name=f'plateau-{phase}',
                commanded_exposure_lines=lines,gain16=16,frames=frames))

    def test_response_never_promotes_association(self):
        result=analyze(self.manifest,self.root)
        self.assertTrue(result['screening_passed'])
        self.assertEqual(result['plateaus'][1]['observed_site_ratio_to_first'],[4]*4)
        self.assertFalse(result['effective_exposure_verified'])
        self.assertFalse(result['hdr_qualified'])

    def test_bad_identity_sequence_generation_or_automation_rejected(self):
        for kind in ['hash','sequence','time','generation','auto','alias','escape','gain']:
            m=copy.deepcopy(self.manifest);f=m['plateaus'][0]['frames'][1]
            if kind=='hash':f['sha256']='0'*64
            if kind=='sequence':f['capture_sequence']=8
            if kind=='time':f['host_receive_monotonic_ns']=1000
            if kind=='generation':f['configuration_generation']=4
            if kind=='auto':m['manual_exposure']=False
            if kind=='alias':f['file']=m['plateaus'][0]['frames'][0]['file']
            if kind=='escape':f['file']='../outside.raw'
            if kind=='gain':m['plateaus'][1]['gain16']=32
            with self.subTest(kind=kind),self.assertRaises(ValueError):analyze(m,self.root)

    def replace_frame(self, value):
        f=self.manifest['plateaus'][1]['frames'][1]
        data=struct.pack('<16H',*([value]*16));(self.root/f['file']).write_bytes(data)
        f['sha256']=hashlib.sha256(data).hexdigest()

    def test_flicker_and_clipping_fail_screening(self):
        for value in [500,1023,16]:
            self.replace_frame(value)
            with self.subTest(value=value):
                self.assertFalse(analyze(self.manifest,self.root)['screening_passed'])

    def test_out_of_range_samples_rejected_even_with_correct_hash(self):
        self.replace_frame(1024)
        with self.assertRaises(ValueError):analyze(self.manifest,self.root)

    def test_single_restart_trial_has_no_cross_restart_ratio(self):
        self.manifest['plateaus']=self.manifest['plateaus'][:1]
        result=analyze(self.manifest,self.root)
        self.assertTrue(result['screening_passed'])
        self.assertNotIn('observed_site_ratio_to_first',result['plateaus'][0])
        self.assertFalse(result['effective_exposure_verified'])

    def test_input_manifest_is_unchanged(self):
        before=json.dumps(self.manifest,sort_keys=True)
        analyze(self.manifest,self.root)
        self.assertEqual(json.dumps(self.manifest,sort_keys=True),before)


if __name__=='__main__':
    unittest.main()
