"""Assemble Blender-rendered frames; requires Pillow (bundled Codex runtime)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',22)
labels=['Idle','Walk','Run','Turn_L90','Attack','Jump']
for family in ['human','wolf']:
    sheet=Image.new('RGB',(1800,1480),(24,29,29))
    for i,clip in enumerate(labels):
        im=Image.open(ROOT/f'previews/poses/{family}_{clip}.png').convert('RGB')
        at=((i%3)*600,(i//3)*740)
        sheet.paste(im,at)
        draw=ImageDraw.Draw(sheet)
        draw.text((at[0]+20,at[1]+710),f'{family.upper()} / {clip}',fill=(217,210,186),font=font)
    sheet.save(ROOT/f'previews/{family}_animation_poses.jpg',quality=92)
    files=sorted((ROOT/f'previews/walk_frames/{family}').glob('*.png'))
    frames=[Image.open(f).convert('RGB').quantize(colors=192,method=Image.Quantize.MEDIANCUT) for f in files]
    duration=[70 if i%3 else 60 for i in range(len(frames))]
    frames[0].save(ROOT/f'previews/{family}_walk.gif',save_all=True,append_images=frames[1:],duration=duration,loop=0,disposal=2,optimize=False)

overview=Image.new('RGB',(1600,1100),(30,36,35))
draw=ImageDraw.Draw(overview)
title=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',28)
draw.text((28,18),'ARCANA / MODULAR LOW POLY ASSETS',font=title,fill=(223,215,190))
human=Image.open(ROOT/'previews/human_lineup.png').convert('RGB').resize((800,500),Image.Resampling.LANCZOS)
house=Image.open(ROOT/'previews/cottage.png').convert('RGB').resize((800,600),Image.Resampling.LANCZOS)
overview.paste(human,(0,110));overview.paste(house,(800,60))
draw.text((25,76),'MODULAR HUMANS / 7 HEAD PROPORTIONS',font=font,fill=(205,203,183))
draw.text((825,672),'SNAP BUILDING / HOUSEHOLD / NATURE',font=font,fill=(205,203,183))
for i,name in enumerate(['deer','wolf','boar','goat']):
    im=Image.open(ROOT/f'previews/{name}.png').convert('RGB').resize((400,314),Image.Resampling.LANCZOS)
    overview.paste(im,(i*400,730))
    draw.text((i*400+20,1060),name.upper()+' / SKINNED',font=font,fill=(205,203,183))
overview.save(ROOT/'previews/overview.jpg',quality=94)
