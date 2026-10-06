"""Generate legible paper faces from story text; outputs remain outside the Git project."""
import os
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

out=Path(os.environ['REBORN_CLUE_TEXTURE_ROOT']);out.mkdir(parents=True,exist_ok=True)
font=Path(os.environ.get('REBORN_CHINESE_FONT',r'C:\Windows\Fonts\msyh.ttc'))
title_font=ImageFont.truetype(str(font),42)
body_font=ImageFont.truetype(str(font),33)
content={
 'Photo_A':('照片背面',['快逃！入口就是出口！','入口……我刚才醒来的地方？']),
 'Warning_B':('留下的信息',['房间 3 才是逃离的关键。','这里还有一个奇怪的人，','千万别让他抓到。']),
 'Testament_C':('遗言',['三个房间各留下一部分信息。','全部找到，才能知道逃生方法。','第三扇门后，','我第一次看见外面的光。']),
 'Clue_B':('第二条信息',['第二间房不是出口。']),
 'Clue_A':('最后一条信息',['我又回到了第一次醒来的地方。','A → B → C','门已经打开，别回头。']),
 'Introduction_A':('第二次醒来',['去房间 3。','那里留着真正离开的方法。']),
 'FinalMessage':('最后的纸条',['门已经打开，别回头。','A → B → C → 出口','这枚钥匙可以打开沿途所有门。']),
}
for item,(title,lines) in content.items():
 image=Image.new('RGB',(1024,640),(217,205,176));draw=ImageDraw.Draw(image)
 draw.rectangle((28,26,996,614),outline=(109,99,80),width=3)
 draw.text((65,62),title,font=title_font,fill=(55,45,39))
 draw.line((65,128,955,128),fill=(118,100,82),width=2)
 for n,line in enumerate(lines):draw.text((65,165+n*74),line,font=body_font,fill=(79,43,39) if item in ('Photo_A','FinalMessage') else (52,46,39))
 image.save(out/('T_'+item+'.png'))
print('Generated',len(content),'story textures in',out)
