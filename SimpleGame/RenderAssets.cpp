#include "stdafx.h"
#include "RenderAssets.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    struct Pixel { unsigned char r,g,b,a; };
    Pixel MakePixel(int r,int g,int b) { return {static_cast<unsigned char>(r),static_cast<unsigned char>(g),static_cast<unsigned char>(b),255}; }
    unsigned int Hash(unsigned int x,unsigned int y)
    {
        unsigned int h=x*374761393u+y*668265263u+1337u;
        h=(h^(h>>13))*1274126177u;
        return h^(h>>16);
    }
    GLuint Upload(int w,int h,const std::vector<Pixel>& data,bool repeat)
    {
        GLuint id=0; glGenTextures(1,&id); glBindTexture(GL_TEXTURE_2D,id);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data.data());
        return id;
    }
}

GLuint RenderAssets::MakeMaterial(Material material)
{
    const int side=256;
    std::vector<Pixel> pixels(side*side);
    const int colors[][3]={{62,85,60},{122,105,77},{123,124,111},{162,153,126},{105,73,48},{91,91,92}};
    for(int y=0;y<side;++y) for(int x=0;x<side;++x)
    {
        int grain=static_cast<int>(Hash(x,y)%23)-11;
        float structure=0;
        if(material==Grass)
            structure=7.f*std::sin(x*.25f+y*.5f)+static_cast<float>(Hash(x/13,y/13)%13)-6;
        if(material==Dirt)
            structure=5.f*std::sin(x*.13f+y*.18f)+(Hash(x,y)%43==0?22.f:0.f);
        if(material==Plaster) structure=static_cast<float>(Hash(x/9,y/9)%13)-6;
        if(material==Stone || material==Shingles)
        {
            int row=y/32, sx=(x+(row%2)*32)%64, sy=y%32;
            structure=static_cast<float>(Hash((x+(row%2)*32)/64,row)%29)-14;
            if(sx<3 || sy<3) structure-=37;
            else if(sx<5 || sy<5) structure+=13;
            if(material==Shingles) structure+=std::sin(sx*.55f)*4.f-sy*.35f;
        }
        if(material==Timber)
        {
            structure=8.f*std::sin(x*.9f+std::sin(y*.05f)*2)+4.f*std::sin(x*1.7f+y*.035f);
            if(x%32<2) structure-=35;
        }
        int delta=grain+static_cast<int>(structure);
        pixels[y*side+x]=MakePixel(std::max(0,std::min(255,colors[material][0]+delta)),
            std::max(0,std::min(255,colors[material][1]+delta)),
            std::max(0,std::min(255,colors[material][2]+delta)));
    }
    return Upload(side,side,pixels,true);
}

GLuint RenderAssets::MakeCharacterAtlas()
{
    const int width=320, height=2048;
    std::vector<Pixel> pixels(width*height,Pixel{0,0,0,0});
    const Pixel palettes[]={MakePixel(102,93,85),MakePixel(135,103,70),MakePixel(91,110,104),MakePixel(98,81,66),
        MakePixel(91,104,125),MakePixel(139,107,100),MakePixel(100,115,85),MakePixel(151,162,135)};
    for(int palette=0;palette<8;++palette) for(int direction=0;direction<4;++direction)
        for(int frame=0;frame<8;++frame)
    {
        const int ox=frame*40, oy=(palette*4+direction)*64;
        auto put=[&](int x,int y,Pixel color)
        { if(x>=1 && x<39 && y>=1 && y<63) pixels[(oy+y)*width+ox+x]=color; };
        auto box=[&](int x,int y,int w,int h,Pixel color)
        { for(int j=y;j<y+h;++j) for(int i=x;i<x+w;++i) put(i,j,color); };
        auto oval=[&](int cx,int cy,int rx,int ry,Pixel color)
        {
            for(int y=cy-ry;y<=cy+ry;++y) for(int x=cx-rx;x<=cx+rx;++x)
            { float dx=static_cast<float>(x-cx)/rx,dy=static_cast<float>(y-cy)/ry;
              if(dx*dx+dy*dy<=1) put(x,y,color); }
        };
        float phase=frame==0?0.f:(frame-1)*6.2831853f/7.f;
        int step=frame==0?0:static_cast<int>(std::sin(phase)*4);
        int bob=frame==0?0:static_cast<int>(std::abs(std::sin(phase))*2);
        Pixel outline=MakePixel(28,35,36), leather=MakePixel(70,53,38), skin=MakePixel(187,151,115);
        Pixel cloth=palettes[palette];
        Pixel light=MakePixel(cloth.r+20,cloth.g+20,cloth.b+18);
        Pixel dark=MakePixel(cloth.r-28,cloth.g-26,cloth.b-24);
        // Boots, split legs, layered tunic, moving forearms and hood.
        box(13,46+step,6,12-step,outline); box(22,46-step,6,12+step,outline);
        box(11,56+step/2,8,4,leather); box(22,56-step/2,8,4,leather);
        for(int y=25;y<49;++y)
        {
            int half=7+(y-25)/5;
            box(20-half,y+bob,half*2,1,outline);
            box(21-half,y+bob,half*2-2,1,cloth);
            box(20,y+bob,half-1,1,dark);
            if(y%5==0) box(15,y+bob,2,2,light);
        }
        box(13,39+bob,15,3,leather); box(19,39+bob,3,3,MakePixel(192,159,89));
        oval(10,33-step/2+bob,4,9,dark); oval(30,33+step/2+bob,4,9,cloth);
        oval(10,42-step/2+bob,3,3,skin); oval(30,42+step/2+bob,3,3,skin);
        oval(20,19+bob,9,12,outline); oval(20,18+bob,8,10,cloth);
        int faceX=direction==1?24:direction==3?16:20;
        if(direction!=2)
        {
            oval(faceX,23+bob,5,6,skin);
            box(faceX-4,19+bob,9,3,dark);
            box(faceX-3,24+bob,2,1,outline);
            if(direction==0) box(faceX+2,24+bob,2,1,outline);
            box(faceX-2,28+bob,4,1,leather);
        }
        else { box(17,15+bob,2,13,light); box(22,16+bob,2,11,dark); }
        box(12,30+bob,17,3,light); // Scarf and shoulder seam.
        if(palette==1 || palette==3) box(15,33+bob,10,15,MakePixel(153,142,113));
        if(palette==4) { box(14,33+bob,13,6,MakePixel(121,132,139)); box(31,30,2,27,leather); }
    }
    return Upload(width,height,pixels,false);
}
