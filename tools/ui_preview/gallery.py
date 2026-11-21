"""Publish the native frames, RGB565 panel proofs, host composites and motion strips."""
from pathlib import Path
import os
from PIL import Image, ImageDraw, ImageFont

root=Path(__file__).resolve().parents[2]
frames=root/'tools/ui_preview/out/frames'
gallery=root/'assets/previews'
gallery.mkdir(exist_ok=True)
ref=Path(os.environ.get('MACHINE_CHROME',root.parent/'Plans/machine-screen-reference/melodee-chrome-edit.png'))
chrome=Image.open(ref).convert('RGB').crop((0,0,320,24))
face=str(root/'assets/fonts/departure/DepartureMono-Regular.otf')
labels=ImageFont.truetype(face,22)
title=ImageFont.truetype(str(root/'assets/fonts/michroma/Michroma-Regular.ttf'),38)
def panel(im):
    channels=[]
    for ch,bits in zip(im.convert('RGB').split(),(5,6,5)):
        lut=[(v>>(8-bits))<<(8-bits) | (v>>(8-bits))>>(2*bits-8) for v in range(256)]
        channels.append(ch.point(lut))
    return Image.merge('RGB',channels)
def composite(im):
    page=Image.new('RGB',(320,240))
    page.paste(im,(0,22));page.paste(chrome,(0,0))
    return page
def scale(im,n):
    return im.resize((im.width*n,im.height*n),Image.Resampling.NEAREST)
labels_list=['FM tone / algorithm','FM tone / ratio edit','Filter response','Amplitude envelope','LFO waveform',
    'BD / playing','SD / snare','CP / bursts','HH / metal tones','CY / CB','EFM drum','GND / source',
    'Kit / slot 3','Shift / controls','K2 / pickup','Value extremes','Track / coral','Track / mint','Track / violet','Track / yellow']
states=sorted(frames.glob('[0-9][0-9]-*.ppm'))
assert len(states)==len(labels_list)
sheet=Image.new('RGB',(24+4*664,112+5*536),(17,14,24))
draw=ImageDraw.Draw(sheet)
draw.text((24,14),'hdslave Machine',font=title,fill='#f1ebef')
draw.text((24,66),'PHASE FOLD  /  RGB565 PANEL PROOFS  /  2x',font=labels,fill='#bcabc9')
for i,(path,label) in enumerate(zip(states,labels_list)):
    name=path.stem
    im=Image.open(path).convert('RGB')
    dest=gallery/'states'/name
    dest.mkdir(exist_ok=True,parents=True)
    im.save(dest/'native.png')
    scale(im,3).save(dest/'3x.png')
    p=panel(im)
    p.save(dest/'panel.png')
    page=composite(p)
    page.save(dest/'melodee.png')
    x,y=24+i%4*664,110+i//4*536
    sheet.paste(scale(page,2),(x,y))
    draw.text((x,y+485),label,font=labels,fill='#cfc1dc')
sheet.save(gallery/'contact-sheet-2x.png',optimize=True)
# Keep the first and settled states and the motion between them at panel cadence.
for name,indices in [('sweep',(0,2,4,7,10,13,17)),('glide',(0,1,2,3,4,5,7)),('voices-scope',(0,2,4,6,8,10,12))]:
    def frame(i):
        return composite(panel(Image.open(frames/(name+'-'+str(i)+'.ppm'))))
    strip=Image.new('RGB',(320*len(indices),240))
    for j,i in enumerate(indices):strip.paste(frame(i),(j*320,0))
    strip.save(gallery/(name+'-strip.png'))
    all_frames=[frame(i) for i in range(18)]
    all_frames[0].save(gallery/(name+'.gif'),save_all=True,append_images=all_frames[1:],duration=16,loop=0,disposal=1)
