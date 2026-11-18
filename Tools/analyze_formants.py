"""Optional STK formant response checks; NumPy/SciPy are not build dependencies.
Usage: python Tools/analyze_formants.py /path/to/BreachFormantTests /tmp/results
"""
import sys,subprocess
from pathlib import Path
import numpy as np
from scipy.signal import find_peaks
exe=sys.argv[1];folder=Path(sys.argv[2]);folder.mkdir(parents=True,exist_ok=True)
reference={}
for sr in [44100,48000,96000,192000,384000]:
    for vowel in [0,.25,.5,.75,1]:
        path=folder/'body.raw'
        subprocess.run([exe,str(sr),'220',str(vowel),'.5','1',str(path)],check=True)
        x=np.fromfile(path,dtype=np.float32)
        # Zero-padding gives a common 1 Hz frequency grid at every sample rate.
        spectrum=np.abs(np.fft.rfft(x,n=sr));freqs=np.fft.rfftfreq(sr,1/sr)
        assert np.isfinite(spectrum).all() and spectrum.max()<1.0001
        assert abs(spectrum[0])<1e-6
        peak=freqs[np.argmax(spectrum)]
        if sr==44100:reference[vowel]=(spectrum,peak)
        else:
            # Compare the strongest formant peak in Hz, not pole-angle metadata.
            assert abs(peak-reference[vowel][1])<10,(sr,vowel,peak,reference[vowel][1])
        print(f'{sr:6} Hz vowel={vowel:.2f} strongest body peak={peak:.1f} Hz gain={spectrum.max():.6f}',flush=True)
assert abs(reference[0][1]-reference[.5][1])>300
print('PASS: vowel spectra differ; sampled dominant peaks stable within 10 Hz across processing rates; DC rejected and static body gain bounded')
