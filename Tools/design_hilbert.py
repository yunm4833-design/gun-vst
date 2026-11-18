import numpy as np
from scipy.optimize import least_squares
f=np.unique(np.r_[np.geomspace(0.00005,0.25,1200),.5-np.geomspace(.00005,.25,1200)])
z=np.exp(-2j*np.pi*f);zz=z*z
# Real poles of second-order allpasses, interleaved between the two branches.
def response(t):
 c=np.tanh(t)
 h=(c[:,None]-zz)/(1-c[:,None]*zz)
 return z*np.prod(h[1::2],axis=0)/np.prod(h[::2],axis=0)
def fun(t):
 h=response(t)
 return np.r_[h.real,(h.imag+1)]
t=np.linspace(.22,5.3,16)
r=least_squares(fun,t,bounds=(.00001,12),max_nfev=1200,ftol=1e-13,xtol=1e-13,gtol=1e-13)
c=np.tanh(r.x)
print('B',list(c[::2]));print('A',list(c[1::2]));print('max phase error deg',np.max(np.abs(np.angle(response(r.x)*1j)))*180/np.pi)
print('worst sideband dB',20*np.log10(np.max(np.abs((1-1j*response(r.x))/(1+1j*response(r.x))))))
# This optional analysis tool requires NumPy/SciPy; plugin builds do not.
