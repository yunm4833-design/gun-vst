"""Optional physical-model measurements. Requires NumPy/SciPy, not for builds.
Usage: python Tools/analyze_rings.py /path/to/BreachRingsTests /tmp/results
"""
import sys,subprocess
from pathlib import Path
import numpy as np
from scipy.signal import butter,sosfilt
executable=sys.argv[1];root=Path(sys.argv[2]);root.mkdir(parents=True,exist_ok=True)
results=[]
for sr in [44100,48000,96000,192000,384000]:
    for frequency in [110,220,880]:
        for stiffness in [0,.7]:
            path=root/'measurement.raw'
            subprocess.run([executable,str(sr),str(frequency),str(stiffness),'.3','.7','2',str(path)],check=True)
            y=np.fromfile(path,dtype=np.float32);start=int(sr*.03);end=int(sr*.7)
            nfft=2**int(np.ceil(np.log2((end-start)*16)))
            spectrum=np.abs(np.fft.rfft(y[start:end]*np.hanning(end-start),nfft));freqs=np.fft.rfftfreq(nfft,1/sr)
            indices=np.where((freqs>frequency*.97)&(freqs<frequency*1.03))[0]
            k=indices[np.argmax(spectrum[indices])]
            logs=np.log(spectrum[k-1:k+2]);delta=.5*(logs[0]-logs[2])/(logs[0]-2*logs[1]+logs[2])
            measured=(k+delta)*sr/nfft;cents=1200*np.log2(measured/frequency)
            assert abs(cents)<1, (sr,frequency,stiffness,cents)
            # Isolate the fundamental before estimating decay in dB/second.
            filtered=sosfilt(butter(2,[frequency*.9,frequency*1.1],btype='bandpass',fs=sr,output='sos'),y)
            window=int(sr*.05);times=[];levels=[]
            for offset in range(int(sr*.3),int(sr*1.2),window):
                rms=np.sqrt(np.mean(filtered[offset:offset+window]**2))
                times.append((offset+window*.5)/sr);levels.append(20*np.log10(max(rms,1e-30)))
            slope=np.polyfit(times,levels,1)[0];rt60=-60/slope
            results.append((sr,frequency,stiffness,cents,rt60))
            print(f'{sr:6} Hz  f0={frequency:4} stiffness={stiffness:.1f} error={cents:+.4f} cents  T60={rt60:.4f} s',flush=True)
for frequency in [110,220,880]:
    for stiffness in [0,.7]:
        times=[r[4] for r in results if r[1]==frequency and r[2]==stiffness]
        assert max(times)/min(times)<1.05,(frequency,stiffness,times)
print('PASS: sampled tuning within 1 cent; fundamental T60 varies less than 5% across processing rates')
