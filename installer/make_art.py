# Generates the BOBA installer artwork (macOS background, Windows wizard images). Needs Pillow.
from PIL import Image, ImageDraw, ImageFont
import os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
F = os.path.join(ROOT, 'Resources/fonts/')
BG=(20,15,13); CAR=(236,180,124); CAR2=(246,207,163); PEARL=(11,8,7); TEXT=(246,236,226); DIM=(168,150,136); RAISED=(42,33,29)
def cup(d, cx, cy, s):
    w=s*0.78; top=cy-s/2+s*0.12; bot=cy+s/2; x0=cx-w/2; x1=cx+w/2
    d.polygon([(x0,top),(x1,top),(x1-s*0.1,bot),(x0+s*0.1,bot)], fill=CAR)
    d.rectangle([x0-1, top+s*0.14, x1+1, top+s*0.19], fill=BG)
    d.rounded_rectangle([cx+s*0.04, cy-s/2-s*0.08, cx+s*0.12, cy-s/2+s*0.28], radius=s*0.03, fill=CAR2)
    pr=s*0.085; py=bot-s*0.2
    for i in (-1,0,1):
        px=cx+0.18*i*s; yy=py-(s*0.07 if i==0 else 0)
        d.ellipse([px-pr,yy-pr,px+pr,yy+pr], fill=PEARL)
def font(w,size): return ImageFont.truetype(F+{800:'Figtree-ExtraBold.ttf',700:'Figtree-Bold.ttf',600:'Figtree-SemiBold.ttf'}[w], size)
def tracked(d, xy, text, f, fill, track):
    x,y=xy
    for ch in text:
        d.text((x,y),ch,font=f,fill=fill); x+=d.textlength(ch,font=f)+track
# mac background 620x418, content at bottom-left (left strip visible in Installer)
for name,scale in (('background.png',1),):
    W,H=620,418
    im=Image.new('RGBA',(W,H),BG+(255,)); d=ImageDraw.Draw(im)
    for i,(x,y,r) in enumerate([(40,375,8),(68,392,6),(96,372,7),(124,396,5),(150,380,8)]):
        d.ellipse([x-r,y-r,x+r,y+r], fill=RAISED)
    cup(d, 72, 290, 64)
    tracked(d,(40,140),'BOBA',font(800,15),CAR,5)
    d.text((38,160),'SAFE-',font=font(800,30),fill=TEXT)
    d.text((38,192),'SIDE',font=font(800,30),fill=TEXT)
    im.save(os.path.join(ROOT, 'installer/mac/resources/background.png'))
# windows wizard images
def wizard(w,h,path):
    im=Image.new('RGB',(w,h),BG); d=ImageDraw.Draw(im); s=w/164
    cup(d, w/2, h*0.62, 70*s)
    tracked(d,(22*s,40*s),'BOBA',font(800,int(14*s)),CAR,int(4*s))
    d.text((20*s,60*s),'SAFE-',font=font(800,int(26*s)),fill=TEXT)
    d.text((20*s,88*s),'SIDE',font=font(800,int(26*s)),fill=TEXT)
    for x,y,r in [(30,270,7),(56,286,5),(84,268,6),(112,290,5),(136,274,7)]:
        d.ellipse([x*s-r*s,y*s-r*s,x*s+r*s,y*s+r*s], fill=RAISED)
    im.save(path)
def small(w,h,path):
    im=Image.new('RGB',(w,h),BG); d=ImageDraw.Draw(im)
    cup(d,w/2,h/2+h*0.03,min(w,h)*0.7); im.save(path)
base=os.path.join(ROOT, 'installer/windows/')
wizard(164,314,base+'wizard.bmp'); wizard(328,628,base+'wizard@2x.bmp')
small(55,58,base+'wizard-small.bmp'); small(110,116,base+'wizard-small@2x.bmp')
