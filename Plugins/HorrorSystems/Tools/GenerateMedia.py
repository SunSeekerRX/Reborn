"""Generate original placeholder UI textures and short test audio (no external dependency other than Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math, random, struct, wave

ROOT = Path(__file__).resolve().parent / "MediaSource"
ROOT.mkdir(parents=True, exist_ok=True)
FONT = "C:/Windows/Fonts/msyh.ttc"
def font(size): return ImageFont.truetype(FONT, size)

def note():
    im=Image.new("RGB",(900,560),(213,205,178)); d=ImageDraw.Draw(im)
    d.rectangle((25,25,875,535),outline=(130,118,85),width=3)
    d.text((70,65),"值班记录  /  04 OCT",font=font(35),fill=(49,48,42))
    for i,t in enumerate(["灯熄灭以后，不要奔跑。", "它在远处追得很快，靠近时会放慢。", "钥匙留在走廊的尽头。", "穿过蓝色的门，记住你来时的方向。"]):
        y=165+i*70; d.line((65,y+48,830,y+48),fill=(175,166,143),width=1)
        d.text((70,y),t,font=font(27),fill=(57,55,47))
    d.text((680,465),"— 守夜人",font=font(24),fill=(100,77,59))
    im.save(ROOT/"T_Note.png")

def icon(name, kind, color):
    im=Image.new("RGBA",(256,256),(0,0,0,0)); d=ImageDraw.Draw(im)
    if kind=="key":
        d.ellipse((25,35,110,120),outline=color,width=15)
        d.line((94,102,215,222),fill=color,width=17)
        d.line((164,166,184,145),fill=color,width=15); d.line((190,192,210,171),fill=color,width=15)
    elif kind=="note":
        d.rounded_rectangle((55,22,205,232),radius=5,fill=color)
        for y in range(70,200,28): d.line((76,y,183,y),fill=(88,82,70),width=5)
    elif kind=="battery":
        d.rounded_rectangle((76,40,181,226),radius=12,fill=color); d.rectangle((107,22,148,45),fill=color)
        d.line((98,86,157,86),fill=(40,45,48),width=8); d.line((128,57,128,116),fill=(40,45,48),width=8)
    else:
        d.ellipse((62,62,196,196),fill=color,outline=(230,231,221),width=7)
        d.ellipse((99,99,159,159),fill=(28,36,43))
    im.save(ROOT/name)

def sound(name, duration, mode):
    sr=22050; random.seed(17); samples=[]
    for i in range(int(duration*sr)):
        t=i/sr
        if mode=="ambience":
            v=.12*math.sin(2*math.pi*55*t)+.065*math.sin(2*math.pi*82.5*t)+.015*random.uniform(-1,1)
            v*=.75+.25*math.sin(2*math.pi*t/duration)
        elif mode=="step":
            v=(.55*random.uniform(-1,1)+.25*math.sin(2*math.pi*85*t))*math.exp(-35*t)
        elif mode=="travel":
            v=.32*math.sin(2*math.pi*(180*t+160*t*t))*math.sin(math.pi*t/duration)**2
        else:
            freq=440 if mode=="pickup" else 260
            v=.28*(math.sin(2*math.pi*freq*t)+.3*math.sin(2*math.pi*freq*1.5*t))*math.exp(-9*t)
        samples.append(struct.pack("<h",int(max(-1,min(1,v))*32767)))
    with wave.open(str(ROOT/name),"wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr); w.writeframes(b"".join(samples))

note(); icon("T_NoteIcon.png","note",(210,201,169)); icon("T_Key.png","key",(201,161,73)); icon("T_Battery.png","battery",(128,169,158)); icon("T_Token.png","token",(163,113,113))
sound("S_Footstep.wav",.2,"step"); sound("S_Pickup.wav",.35,"pickup"); sound("S_Inspect.wav",.45,"inspect"); sound("S_Travel.wav",.8,"travel"); sound("S_Ambience.wav",8,"ambience")
print("Generated textures/audio:",ROOT)
