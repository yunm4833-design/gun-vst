"""Optional impulse tuning/decay checks (NumPy/SciPy), not build dependencies.
Usage: python Tools/analyze_modal.py /path/to/BreachModalTests /tmp/results
"""
import sys,subprocess
from pathlib import Path
import numpy as np
from scipy.signal import butter,sosfilt
exe=sys.argv[1];folder=Path(sys.argv[2]);folder.mkdir(parents=True,exist_ok=True)
results=[]
for sr in [44100,48000,96000,192000,384000]:
    for pitch in [110,220,880]:
        for profile in [0,1]:
            path=folder/'impulse.raw'
            subprocess.run([exe,str(sr),str(pitch),str(profile),'.6','.4','2',str(path)],check=True)
            y=np.fromfile(path,dtype=np.float32)
            assert np.isfinite(y).all()
            start=int(sr*.02);end=int(sr*.7)
            nfft=2**int(np.ceil(np.log2((end-start)*16)))
            spectrum=np.abs(np.fft.rfft(y[start:end]*np.hanning(end-start),nfft));freqs=np.fft.rfftfreq(nfft,1/sr)
            indices=np.where((freqs>pitch*.97)&(freqs<pitch*1.03))[0]
            k=indices[np.argmax(spectrum[indices])];logs=np.log(spectrum[k-1:k+2])
            delta=.5*(logs[0]-logs[2])/(logs[0]-2*logs[1]+logs[2]);measured=(k+delta)*sr/nfft
            cents=1200*np.log2(measured/pitch)
            assert abs(cents)<1,(sr,pitch,profile,cents)
            filtered=sosfilt(butter(2,[pitch*.85,pitch*1.15],btype='bandpass',fs=sr,output='sos'),y)
            window=int(sr*.03);times=[];levels=[]
            lo,hi=(.3,1.2) if profile else (.12,.5)
            for offset in range(int(sr*lo),int(sr*hi),window):
                rms=np.sqrt(np.mean(filtered[offset:offset+window]**2))
                times.append((offset+window*.5)/sr);levels.append(20*np.log10(max(rms,1e-30)))
            slope=np.polyfit(times,levels,1)[0];rt60=-60/slope
            target=(.16 if profile else .05)*(22 if profile else 20)**.6
            assert abs(rt60/target-1)<.05,(sr,pitch,profile,rt60,target)
            results.append((sr,pitch,profile,cents,rt60))
            print(f'{sr:6} Hz f0={pitch:4} {"REVOLVER" if profile else "PISTOL":8} error={cents:+.4f} cents T60={rt60:.4f}s target={target:.4f}s',flush=True)
for pitch in [110,220,880]:
    for profile in [0,1]:
        times=[x[4] for x in results if x[1]==pitch and x[2]==profile]
        assert max(times)/min(times)<1.03
print('PASS: sampled impulse tuning within 1 cent; fundamental RT60 within 5% of target and cross-rate variation under 3%')
