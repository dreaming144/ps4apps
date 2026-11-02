from PIL import Image, ImageDraw
im=Image.new('RGB',(512,512)); d=ImageDraw.Draw(im)
for y in range(512):
    t=y/512; d.line([(0,y),(512,y)],fill=(int(255-t*60),int(170+t*20),int(90+t*40)) if y>360 else (int(110+t*150),int(180+t*40),int(250-t*60)))
def portal(cx,c):
    for i in range(14,0,-1):
        k=i/14; d.ellipse([cx-50*k-10,256-150*k-20,cx+50*k+10,256+150*k+20],fill=tuple(int(c[j]*(0.5+0.5*(1-k))+255*(1-k)*0.4) for j in range(3)))
    d.ellipse([cx-28,140,cx+28,372],fill=(255,255,255)); d.ellipse([cx-20,150,cx+20,362],fill=c)
portal(80,(40,120,255)); portal(432,(255,140,30))
S=16; ox,oy=176,120
def r(x,y,w,h,c): d.rectangle([ox+x*S,oy+y*S,ox+(x+w)*S-1,oy+(y+h)*S-1],fill=c)
r(1,0,6,1,(60,170,70)); r(0,1,8,15,(60,170,70)); r(3,1,2,15,(40,130,55))
for (x,y) in [(-1,3),(8,5),(-1,9),(8,11),(-1,14),(8,2)]: r(x,y,1,1,(255,255,225))
r(5,3,1,2,(0,0,0)); r(6,3,1,2,(0,0,0)); r(4,7,3,1,(30,80,30))
r(-2,8,3,5,(140,85,40)); r(1,1,1,8,(110,65,30))
r(6,9,7,3,(180,180,195)); r(13,9,2,3,(255,140,30)); r(7,12,2,2,(120,120,130))
r(1,16,2,2,(120,80,40)); r(5,16,2,2,(120,80,40))
im.save('sce_sys/icon0.png')
