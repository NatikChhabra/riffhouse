import json,subprocess,os,numpy as np,soundfile as sf
from scipy.signal import welch
T=os.path.expanduser('~/rh/build/tests/riffhouse_tests_artefacts/Release/riffhouse_tests'); sr=48000
OFF={"amp_on":True,"amp_model":"Brit 81","amp_gain":5,"amp_bass":5,"amp_mid":5,"amp_treble":5,"amp_presence":5,"amp_depth":5,"amp_master":5,"amp_level":0,"cab_on":False,"gate_on":False,"hum_filter":0,"input_fix":False,"rev_on":False,"dly_on":False,"mod_on":False,"comp_on":False,"eq_on":False,"drv_on":False,"wah_on":False,"auto_level":False,"out_level":0.0}
rng=np.random.default_rng(1); n=sr*20
w=rng.standard_normal(n); W=np.fft.rfft(w); f=np.fft.rfftfreq(n,1/sr); W[1:]/=np.sqrt(f[1:]); x=np.fft.irfft(W); x*=10**(-28/20)/np.sqrt(np.mean(x**2))
sf.write('in.wav',x,sr,subtype='FLOAT'); json.dump({"name":"c","category":"User","params":OFF},open('p.json','w'))
M='/root/oss/NAM_models/Phillipe P JVM-OD2-RD-NoDrive'
res={}
for tag,fn in [('nocab',M+'-ESR0,003.nam'),('cab',M+'-Cab-ESR0,006.nam')]:
    subprocess.run([T,'render','p.json',tag+'.wav','in.wav'],env=dict(os.environ,RH_CAPTURE=fn),capture_output=True)
    y,_=sf.read(tag+'.wav'); y=y[:,0] if y.ndim>1 else y; res[tag]=welch(y[sr*2:],sr,nperseg=16384)
fr=res['nocab'][0]; H=10*np.log10(res['cab'][1]/(res['nocab'][1]+1e-30)+1e-30)
np.save('H.npy',np.stack([fr,H]))
for c in [80,100,150,250,400,630,1000,2000,3000,4000,6000,8000,10000,14000]:
    m=(fr>c/1.12)&(fr<c*1.12); print(c,round(float(np.mean(H[m])),1))
