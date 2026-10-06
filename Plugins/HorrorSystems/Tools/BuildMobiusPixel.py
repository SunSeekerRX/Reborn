"""Rasterize an original half-twist ribbon at native pixel resolution; no external dependencies."""
from pathlib import Path
import math, struct, zlib

out=Path(__file__).resolve().parent/'ArtSources/MobiusPixel.png';out.parent.mkdir(parents=True,exist_ok=True)
w,h=160,110
pixels=[(0,0,0,0)]*(w*h);depth=[-1e9]*(w*h)
palette=[(47,67,73,255),(66,91,96,255),(91,119,117,255),(121,148,138,255),(156,179,157,255),(192,209,179,255),(222,230,199,255)]
def point(t,v):
    a=1+v*math.cos(t/2);x=a*math.cos(t);y=a*math.sin(t);z=v*math.sin(t/2)
    # Oblique view exposes the half-twist and the single continuous band.
    xx=.866*x-.5*y;yy=.5*x+.866*y
    return xx,.60*yy-.80*z,.80*yy+.60*z
for i in range(2200):
    t=i*2*math.pi/2200
    for j in range(120):
        v=-.30+.60*j/119;x,y,z=point(t,v)
        px,py=round(80+x*48),round(54+y*48)
        if not(0<=px<w and 0<=py<h):continue
        n=py*w+px
        if z<depth[n]:continue
        depth[n]=z
        a=point(t+.002,v);b=point(t,v+.002)
        du=[(a[k]-(x,y,z)[k])/.002 for k in range(3)];dv=[(b[k]-(x,y,z)[k])/.002 for k in range(3)]
        nx=du[1]*dv[2]-du[2]*dv[1];ny=du[2]*dv[0]-du[0]*dv[2];nz=du[0]*dv[1]-du[1]*dv[0]
        norm=math.sqrt(nx*nx+ny*ny+nz*nz)
        light=abs((-.3*nx-.6*ny+.74*nz)/max(.001,norm))
        shade=max(0,min(6,int(light*6)))
        pixels[n]=palette[shade] if 3<j<116 else palette[min(6,shade+1)]
def chunk(k,v):return struct.pack('>I',len(v))+k+v+struct.pack('>I',zlib.crc32(k+v)&0xffffffff)
data=b''.join(b'\0'+bytes(channel for pixel in pixels[y*w:(y+1)*w] for channel in pixel) for y in range(h))
out.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(data,9))+chunk(b'IEND',b''))
print(out)
