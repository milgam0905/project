#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Renderer.h"
#include "RenderAssets.h"
#include "WorldGeometry.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    using P = RenderPoint;
    using Col = RenderColor;
    const float W=1280.f,H=800.f,Pi=WorldGeometry::Pi;
    Col C(float r,float g,float b,float a=1) { return {r,g,b,a}; }
    void Color(Col c) { glColor4f(c.r,c.g,c.b,c.a); }
    void Quad(P a,P b,P c,P d,Col color)
    {
        Color(color); glBegin(GL_QUADS);
        glVertex2f(a.x,a.y); glVertex2f(b.x,b.y); glVertex2f(c.x,c.y); glVertex2f(d.x,d.y); glEnd();
    }
    void Rect(float x,float y,float w,float h,Col c)
    { Quad({x,y},{x+w,y},{x+w,y+h},{x,y+h},c); }
    void Ellipse(P p,float rx,float ry,Col color,bool feather=false)
    {
        glBegin(GL_TRIANGLE_FAN); Color(color); glVertex2f(p.x,p.y);
        if(feather) Color(C(color.r,color.g,color.b,0));
        for(int i=0;i<=48;++i)
        {
            float a=i*Pi/24.f; glVertex2f(p.x+std::cos(a)*rx,p.y+std::sin(a)*ry);
        }
        glEnd();
    }
    void Stroke(P a,P b,Col color)
    {
        Color(color); glBegin(GL_LINES); glVertex2f(a.x,a.y); glVertex2f(b.x,b.y); glEnd();
    }
    void TexturedQuad(GLuint texture,P a,P b,P c,P d,float repeatX,float repeatY,Col color)
    {
        glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texture); Color(color);
        glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(a.x,a.y);
        glTexCoord2f(repeatX,0); glVertex2f(b.x,b.y);
        glTexCoord2f(repeatX,repeatY); glVertex2f(c.x,c.y);
        glTexCoord2f(0,repeatY); glVertex2f(d.x,d.y);
        glEnd(); glDisable(GL_TEXTURE_2D);
    }
    void Canvas()
    {
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,W,H,0,-1,1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    }
    void Fullscreen()
    {
        Color(C(1,1,1));
        glBegin(GL_QUADS);
        glTexCoord2f(0,1); glVertex2f(0,0);
        glTexCoord2f(1,1); glVertex2f(W,0);
        glTexCoord2f(1,0); glVertex2f(W,H);
        glTexCoord2f(0,0); glVertex2f(0,H);
        glEnd();
    }
    GLuint Program(const char* fragment)
    {
        const char* vertex="#version 120\nvarying vec2 uv;\nvoid main(){gl_Position=ftransform(); uv=gl_MultiTexCoord0.xy;}\n";
        GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
        const char* sources[2]={vertex,fragment};
        for(int i=0;i<2;++i)
        {
            glShaderSource(shaders[i],1,&sources[i],nullptr); glCompileShader(shaders[i]);
            GLint ok=0; glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&ok);
            if(!ok)
            {
                char log[2048]={}; glGetShaderInfoLog(shaders[i],2048,nullptr,log);
                std::cerr<<"Renderer shader: "<<log<<"\n";
                glDeleteShader(shaders[0]); glDeleteShader(shaders[1]); return 0;
            }
        }
        GLuint result=glCreateProgram();
        for(GLuint s:shaders) glAttachShader(result,s);
        glLinkProgram(result);
        for(GLuint s:shaders) glDeleteShader(s);
        GLint ok=0; glGetProgramiv(result,GL_LINK_STATUS,&ok);
        if(!ok)
        {
            char log[2048]={}; glGetProgramInfoLog(result,2048,nullptr,log);
            std::cerr<<"Renderer link: "<<log<<"\n"; glDeleteProgram(result); return 0;
        }
        return result;
    }
    const char* BlurFragment=R"GLSL(#version 120
uniform sampler2D source;
uniform vec2 stepUV;
uniform float extractLight;
varying vec2 uv;
vec3 tap(vec2 p) {
    vec3 c=texture2D(source,p).rgb;
    return extractLight>0.5 ? max(c-vec3(0.55),vec3(0.0)) : c;
}
void main() {
    vec3 c=tap(uv)*0.227027;
    c+=(tap(uv+stepUV*1.384615)+tap(uv-stepUV*1.384615))*0.316216;
    c+=(tap(uv+stepUV*3.230769)+tap(uv-stepUV*3.230769))*0.070270;
    gl_FragColor=vec4(c,1.0);
})GLSL";
    const char* CompositeFragment=R"GLSL(#version 120
uniform sampler2D scene;
uniform sampler2D bloom;
uniform vec2 texel;
varying vec2 uv;
void main() {
    vec3 c=texture2D(scene,uv).rgb;
    vec3 n=texture2D(scene,uv+vec2(0.0,texel.y)).rgb;
    vec3 s=texture2D(scene,uv-vec2(0.0,texel.y)).rgb;
    vec3 e=texture2D(scene,uv+vec2(texel.x,0.0)).rgb;
    vec3 w=texture2D(scene,uv-vec2(texel.x,0.0)).rgb;
    float contrast=length(n-s)+length(e-w);
    c=mix(c,(n+s+e+w)*0.25,clamp(contrast*0.25,0.0,0.24));
    c+=texture2D(bloom,uv).rgb*0.7;
    float luminance=dot(c,vec3(0.2126,0.7152,0.0722));
    c=mix(vec3(luminance),c,0.88);
    c*=mix(vec3(0.88,1.03,1.06),vec3(1.08,1.02,0.93),smoothstep(0.18,0.70,luminance));
    c=pow(max(c,vec3(0.0)),vec3(0.94));
    vec2 p=(uv-0.5)*vec2(1.15,1.0);
    c*=1.0-smoothstep(0.20,0.76,length(p))*0.24;
    gl_FragColor=vec4(c,1.0);
})GLSL";
}

struct Renderer::Impl
{
    GLuint materials[RenderAssets::MaterialCount]={}, atlas=0;
    GLuint sceneFbo=0, sceneTexture=0, bloomFbo[2]={}, bloomTexture[2]={};
    GLuint blur=0, composite=0;
    bool post=false;
    int windowW=1280,windowH=800;
    P camera={0,0};
    float time=0;
    Impl()
    {
        for(int i=0;i<RenderAssets::MaterialCount;++i)
            materials[i]=RenderAssets::MakeMaterial(static_cast<RenderAssets::Material>(i));
        atlas=RenderAssets::MakeCharacterAtlas();
        if(GLEW_VERSION_3_0 || GLEW_ARB_framebuffer_object)
        {
            blur=Program(BlurFragment); composite=Program(CompositeFragment);
            post=blur && composite && Target(sceneFbo,sceneTexture,1280,800)
                && Target(bloomFbo[0],bloomTexture[0],640,400)
                && Target(bloomFbo[1],bloomTexture[1],640,400);
            glBindFramebuffer(GL_FRAMEBUFFER,0);
        }
        if(!post) std::cerr<<"Post-processing unavailable; direct world rendering enabled.\n";
    }
    ~Impl()
    {
        glDeleteTextures(RenderAssets::MaterialCount,materials); glDeleteTextures(1,&atlas);
        if(blur) glDeleteProgram(blur);
        if(composite) glDeleteProgram(composite);
        if(sceneFbo) glDeleteFramebuffers(1,&sceneFbo);
        for(int i=0;i<2;++i) if(bloomFbo[i]) glDeleteFramebuffers(1,&bloomFbo[i]);
        glDeleteTextures(1,&sceneTexture); glDeleteTextures(2,bloomTexture);
    }
    bool Target(GLuint& fbo,GLuint& texture,int w,int h)
    {
        glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        return glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    }
    void Viewport()
    {
        float scale=std::min(windowW/W,windowH/H);
        int w=static_cast<int>(W*scale),h=static_cast<int>(H*scale);
        glViewport((windowW-w)/2,(windowH-h)/2,w,h);
    }
    P Screen(P p) const
    { P v=WorldGeometry::Project(p); return {v.x-camera.x+640.f,v.y-camera.y+420.f}; }
    void Path(P a,P b,P c,P d,float width)
    {
        // Curved, feathered ribbon. UVs follow arc length rather than tile edges.
        glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,materials[RenderAssets::Dirt]);
        const int segments=80;
        for(int band=0;band<3;++band)
        {
            glBegin(GL_TRIANGLE_STRIP);
            float distance=0; P previous=a;
            for(int i=0;i<=segments;++i)
            {
                float t=i/static_cast<float>(segments);
                P q=WorldGeometry::Bezier(a,b,c,d,t);
                P q0=WorldGeometry::Bezier(a,b,c,d,std::max(0.f,t-.005f));
                P q1=WorldGeometry::Bezier(a,b,c,d,std::min(1.f,t+.005f));
                float dx=q1.x-q0.x,dy=q1.y-q0.y;
                float len=std::sqrt(dx*dx+dy*dy); if(len<.0001f) len=1;
                distance+=std::sqrt((q.x-previous.x)*(q.x-previous.x)+(q.y-previous.y)*(q.y-previous.y));
                previous=q;
                const float offsets[4]={-width-.3f,-width,width,width+.3f};
                const float alpha[4]={0,1,1,0};
                for(int k=band;k<=band+1;++k)
                {
                    P p=Screen({q.x-dy/len*offsets[k],q.y+dx/len*offsets[k]});
                    Color(C(.78f,.76f,.68f,alpha[k]));
                    glTexCoord2f(distance*.5f,(offsets[k]+width)*.65f); glVertex2f(p.x,p.y);
                }
            }
            glEnd();
        }
        glDisable(GL_TEXTURE_2D);
    }
    void Lake(float expansion,Col center,Col edge)
    {
        glBegin(GL_TRIANGLE_FAN);
        P p=Screen({11,-6}); Color(center); glVertex2f(p.x,p.y);
        Color(edge);
        for(int i=0;i<=192;++i)
        {
            p=Screen(WorldGeometry::LakeEdge(i*2*Pi/192.f,expansion));
            glVertex2f(p.x,p.y);
        }
        glEnd();
    }
};

Renderer::Renderer(int width,int height):m(new Impl) { Resize(width,height); }
Renderer::~Renderer()=default;
bool Renderer::IsInitialized() const { return m->atlas!=0; }
bool Renderer::HasPostProcessing() const { return m->post; }
void Renderer::Resize(int w,int h) { m->windowW=std::max(1,w);m->windowH=std::max(1,h);m->Viewport(); }
void Renderer::BeginScene(P camera,float time)
{
    m->camera=camera; m->time=time;
    if(m->post) { glBindFramebuffer(GL_FRAMEBUFFER,m->sceneFbo); glViewport(0,0,1280,800); }
    else m->Viewport();
    glUseProgram(0); glActiveTexture(GL_TEXTURE0); glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(.035f,.065f,.070f,1); glClear(GL_COLOR_BUFFER_BIT); Canvas();
}
void Renderer::DrawTerrain()
{
    TexturedQuad(m->materials[RenderAssets::Grass],m->Screen({-28,-26}),m->Screen({28,-26}),
        m->Screen({28,26}),m->Screen({-28,26}),28,26,C(.68f,.78f,.72f));
    // Broad soft patches hide repetition without visible square/diamond terrain cells.
    for(int i=0;i<120;++i)
    {
        float x=-27.f+static_cast<float>((i*131)%540)/10.f;
        float y=-25.f+static_cast<float>((i*83)%500)/10.f;
        P p=m->Screen({x,y});
        if(p.x< -200 || p.x>1480 || p.y< -100 || p.y>900) continue;
        Ellipse(p,110,45,C(.24f,.28f,.15f,.09f),true);
    }
    for(const WorldGeometry::Trail& trail:WorldGeometry::Trails())
        m->Path(trail.a,trail.b,trail.c,trail.d,trail.width);
    // Worn, irregular cobbles in the village square.
    for(int i=0;i<130;++i)
    {
        float x=static_cast<float>((i*31)%65)/10.f-3;
        float y=static_cast<float>((i*43)%64)/10.f-1;
        if((x*x+(y-2)*(y-2))>8) continue;
        P p=m->Screen({x,y});
        float s=3.f+static_cast<float>(i%4);
        Ellipse({p.x+1,p.y+1},s+1,s*.5f,C(.09f,.12f,.11f,.5f));
        Ellipse(p,s,s*.5f,C(.37f+i%3*.025f,.38f,.33f,.6f));
    }
    m->Lake(.13f,C(.20f,.23f,.18f),C(.25f,.27f,.19f));
    m->Lake(.055f,C(.12f,.18f,.16f),C(.16f,.23f,.21f));
    m->Lake(0,C(.035f,.115f,.15f),C(.12f,.29f,.29f));
    // Continuous shore ripple ribbons, driven by the same contour as collision.
    for(int ring=0;ring<3;++ring)
    {
        float wave=std::fmod(m->time*.14f+ring*.33f,1.f);
        Color(C(.40f,.65f,.59f,(1-wave)*.24f)); glBegin(GL_LINE_LOOP);
        for(int i=0;i<192;++i)
        {
            P p=m->Screen(WorldGeometry::LakeEdge(i*2*Pi/192.f,-wave*.16f));
            glVertex2f(p.x,p.y);
        }
        glEnd();
    }
    for(int i=0;i<100;++i)
    {
        P world={7.f+static_cast<float>((i*37)%80)/10.f,-9.5f+static_cast<float>((i*53)%70)/10.f};
        if(!WorldGeometry::Water(world,-.08f)) continue;
        P p=m->Screen(world);
        float t=m->time*1.2f+i;
        float s=std::sin(t);
        Stroke({p.x-8+s*3,p.y},{p.x+12+s*3,p.y},C(.38f,.62f,.62f,.10f+.09f*s));
        if(i%7==0)
        {
            Ellipse({p.x,p.y-5-std::sin(t*.3f)*8},8,8,C(.41f,.84f,.75f,.20f),true);
            Ellipse({p.x,p.y-5-std::sin(t*.3f)*8},1.3f,1.3f,C(.70f,.90f,.72f,.7f));
        }
    }
    for(int i=0;i<75;++i)
    {
        P world=WorldGeometry::LakeEdge(i*2*Pi/75.f,.11f);
        P p=m->Screen(world);
        for(int j=0;j<3;++j)
        {
            float sway=std::sin(m->time+i)*2;
            Stroke({p.x+j*3.f,p.y},{p.x+j*3.f+sway,p.y-12-j*4.f},C(.36f,.40f,.24f));
        }
    }
}
void Renderer::DrawShadow(P p,float width,float height)
{
    // Draw all shadows on ground before any actor/building, with contact + penumbra.
    if(height>120.f)
    {
        // The house's projected footprint casts a tapered, feathered directional shadow.
        for(int layer=5;layer>=0;--layer)
        {
            float spread=layer*2.f;
            Quad({p.x-width-spread,p.y-35},{p.x+width+spread,p.y-35},
                {p.x+width+height*.50f+spread,p.y+height*.23f+spread},
                {p.x-width+height*.50f-spread,p.y+height*.23f+spread},C(.01f,.025f,.03f,.035f));
        }
    }
    Ellipse({p.x+height*.28f,p.y+height*.12f},width+height*.38f,width*.34f+height*.10f,
        C(.015f,.027f,.034f,.40f),true);
    Ellipse(p,width,width*.30f,C(.008f,.018f,.02f,.50f),true);
}
void Renderer::DrawHouse(P p,int variant,float a)
{
    using namespace RenderAssets;
    auto material=[&](Material id,P x,P y,P z,P w,float u,float v,Col tint)
    { TexturedQuad(m->materials[id],x,y,z,w,u,v,tint); };
    // Stone foundation, lime plaster walls, timber framing and weathered roof shingles.
    material(Stone,{p.x-88,p.y-36},{p.x,p.y+3},{p.x,p.y-22},{p.x-88,p.y-61},1,.3f,C(.62f,.68f,.65f,a));
    material(Stone,{p.x,p.y+3},{p.x+76,p.y-34},{p.x+76,p.y-59},{p.x,p.y-22},1,.3f,C(.45f,.54f,.52f,a));
    material(Plaster,{p.x-88,p.y-115},{p.x,p.y-77},{p.x,p.y-22},{p.x-88,p.y-61},1,1,C(.68f,.67f,.58f,a));
    material(Plaster,{p.x,p.y-77},{p.x+76,p.y-113},{p.x+76,p.y-59},{p.x,p.y-22},1,1,C(.43f,.49f,.45f,a));
    material(Shingles,{p.x-102,p.y-109},{p.x-17,p.y-171},{p.x+89,p.y-121},{p.x,p.y-68},1.6f,1.4f,
        variant%2?C(.74f,.63f,.53f,a):C(.59f,.67f,.68f,a));
    // Structural beams follow projected wall edges, never screen-aligned rectangles.
    Quad({p.x-88,p.y-115},{p.x-82,p.y-112},{p.x-82,p.y-59},{p.x-88,p.y-61},C(.19f,.14f,.10f,a));
    Quad({p.x-5,p.y-80},{p.x+1,p.y-77},{p.x+1,p.y-19},{p.x-5,p.y-22},C(.19f,.14f,.10f,a));
    Quad({p.x-88,p.y-91},{p.x,p.y-53},{p.x,p.y-48},{p.x-88,p.y-86},C(.23f,.18f,.12f,a));
    for(int i=1;i<4;++i)
    {
        float x=p.x-88+i*22.f,y=p.y-115+i*9.5f;
        Quad({x,y},{x+4,y+2},{x+4,y+55},{x,y+53},C(.22f,.17f,.12f,a));
    }
    material(Timber,{p.x+20,p.y-69},{p.x+43,p.y-80},{p.x+43,p.y-17},{p.x+20,p.y-7},.45f,1,
        C(.55f,.51f,.42f,a));
    Ellipse({p.x+37,p.y-42},2,2,C(.77f,.61f,.27f,a));
    Quad({p.x-68,p.y-88},{p.x-42,p.y-77},{p.x-42,p.y-51},{p.x-68,p.y-62},C(.96f,.63f,.24f,a));
    Quad({p.x-57,p.y-83},{p.x-53,p.y-82},{p.x-53,p.y-55},{p.x-57,p.y-57},C(.24f,.18f,.11f,a));
    Stroke({p.x-68,p.y-75},{p.x-42,p.y-64},C(.24f,.18f,.11f,a));
    Ellipse({p.x-55,p.y-69},65,50,C(.98f,.54f,.18f,a*.20f),true);
    material(Stone,{p.x+25,p.y-172},{p.x+42,p.y-172},{p.x+42,p.y-128},{p.x+25,p.y-128},.25f,.6f,C(.60f,.60f,.52f,a));
    Rect(p.x+22,p.y-174,23,6,C(.24f,.26f,.24f,a));
    for(int i=0;i<9;++i)
    {
        float life=std::fmod(m->time*.24f+i/9.f,1.f);
        Ellipse({p.x+33+life*28+std::sin(life*9)*8,p.y-181-life*70},
            7+life*17,5+life*12,C(.58f,.61f,.55f,(1-life)*.10f*a),true);
    }
    // Wooden sill and a leaning barrel ground the building in its period.
    TexturedQuad(m->materials[Timber],{p.x+57,p.y-23},{p.x+74,p.y-23},{p.x+76,p.y+1},{p.x+55,p.y+1},
        .4f,.45f,C(.66f,.58f,.43f,a));
    Ellipse({p.x+65,p.y-23},9,4,C(.34f,.26f,.16f,a));
    Stroke({p.x+56,p.y-15},{p.x+75,p.y-15},C(.18f,.21f,.21f,a));
}
void Renderer::DrawTree(P p,int variant,float a)
{
    TexturedQuad(m->materials[RenderAssets::Timber],{p.x-7,p.y-70},{p.x+7,p.y-70},
        {p.x+6,p.y+2},{p.x-7,p.y+2},.25f,1.4f,C(.56f,.56f,.45f,a));
    float sway=std::sin(m->time*.8f+variant)*2;
    for(int layer=0;layer<4;++layer)
    {
        float radius=43-layer*8.f, y=p.y-35-layer*25.f;
        // Lobed crowns replace rigid triangular silhouettes.
        for(int lobe=0;lobe<7;++lobe)
        {
            float angle=lobe*2*Pi/7;
            Ellipse({p.x+sway+std::cos(angle)*radius*.50f,y+std::sin(angle)*radius*.27f},
                radius*.64f,radius*.53f,C(.065f+layer*.012f,.17f+layer*.019f,.13f+layer*.015f,a));
        }
        Ellipse({p.x-10+sway,y-8},radius*.55f,radius*.36f,C(.23f,.34f,.20f,a*.20f),true);
        for(int needle=0;needle<9;++needle)
        {
            float x=p.x-radius*.7f+needle*radius*.16f;
            Stroke({x,y+3},{x+3+sway,y-4},C(.25f,.35f,.22f,a*.4f));
        }
    }
}
void Renderer::DrawCharacter(P p,int variant,int direction,int frame,bool player)
{
    int palette=player?7:variant%7;
    direction=std::max(0,std::min(3,direction)); frame=std::max(0,std::min(7,frame));
    float u0=(frame*40+.5f)/320.f,u1=(frame*40+39.5f)/320.f;
    float v0=((palette*4+direction)*64+.5f)/2048.f,v1=((palette*4+direction)*64+63.5f)/2048.f;
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,m->atlas); Color(C(1,1,1));
    glBegin(GL_QUADS);
    glTexCoord2f(u0,v0); glVertex2f(p.x-25,p.y-76);
    glTexCoord2f(u1,v0); glVertex2f(p.x+25,p.y-76);
    glTexCoord2f(u1,v1); glVertex2f(p.x+25,p.y+4);
    glTexCoord2f(u0,v1); glVertex2f(p.x-25,p.y+4);
    glEnd(); glDisable(GL_TEXTURE_2D);
    if(player)
    {
        Rect(p.x+17,p.y-29,7,11,C(.24f,.20f,.12f));
        DrawFire({p.x+20,p.y-20},.42f);
    }
}
void Renderer::DrawAnimal(P p,int species,float phase,float facing)
{
    float sign=facing<0?-1.f:1.f;
    float scale=species==2?.65f:1.f;
    float step=std::sin(phase)*5;
    Col fur=species==0?C(.49f,.36f,.23f):species==1?C(.48f,.25f,.12f):C(.53f,.52f,.44f);
    for(int leg=0;leg<4;++leg)
    {
        float x=p.x+(leg<2?-13.f:12.f)*scale, offset=(leg%2==0?step:-step)*scale;
        Stroke({x,p.y-18*scale},{x+offset,p.y-1},C(.22f,.20f,.16f));
        Stroke({x+1,p.y-18*scale},{x+offset+1,p.y-1},C(.27f,.24f,.19f));
    }
    Ellipse({p.x,p.y-21*scale},23*scale,11*scale,fur);
    Ellipse({p.x-5*scale,p.y-24*scale},16*scale,5*scale,C(.68f,.54f,.35f,.35f),true);
    float headX=p.x+sign*23*scale;
    Ellipse({headX,p.y-33*scale},8*scale,11*scale,fur);
    Ellipse({headX+sign*5*scale,p.y-29*scale},7*scale,4*scale,fur);
    Ellipse({headX+sign*4*scale,p.y-36*scale},1.4f,1.4f,C(.06f,.09f,.08f));
    Ellipse({headX-sign*4*scale,p.y-45*scale},3*scale,(species==2?12.f:7.f)*scale,fur);
    if(species==0)
    {
        for(int side=0;side<2;++side)
        {
            float x=headX+(side?5.f:-5.f);
            Stroke({x,p.y-41},{x-3,p.y-59},C(.54f,.48f,.35f));
            Stroke({x-2,p.y-53},{x-8,p.y-56},C(.54f,.48f,.35f));
            Stroke({x-2,p.y-51},{x+4,p.y-56},C(.54f,.48f,.35f));
        }
    }
    else if(species==1)
    {
        Ellipse({p.x-sign*28,p.y-23+std::sin(phase)*2},14,5,fur);
        Ellipse({p.x-sign*38,p.y-23+std::sin(phase)*2},5,4,C(.78f,.73f,.59f));
    }
    else Ellipse({p.x-sign*17,p.y-16},5,5,C(.74f,.73f,.66f));
}
void Renderer::DrawFire(P p,float scale,bool mystical)
{
    float t=m->time;
    Col outer=mystical?C(.20f,.72f,.64f):C(1,.43f,.10f);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    Ellipse({p.x,p.y-12*scale},65*scale,55*scale,C(outer.r,outer.g,outer.b,.17f),true);
    for(int i=0;i<11;++i)
    {
        float life=std::fmod(t*.75f+i/11.f,1.f);
        float drift=std::sin(i*3.f+t*4)*7*scale*life;
        Ellipse({p.x+drift,p.y-life*40*scale},(7-5*life)*scale,(11-7*life)*scale,
            C(outer.r,outer.g+life*.15f,outer.b,.55f*(1-life)),true);
    }
    Ellipse({p.x,p.y-5*scale},3*scale,6*scale,C(.96f,.89f,.56f,.9f),true);
    for(int i=0;i<8;++i)
    {
        float life=std::fmod(t*.36f+i/8.f,1.f);
        Ellipse({p.x+std::sin(i*9.f+life*4)*18*scale*life,p.y-life*70*scale},
            1.1f,1.4f,C(outer.r,.72f,outer.b,(1-life)*.75f));
    }
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::Present()
{
    if(m->post)
    {
        glDisable(GL_BLEND); glEnable(GL_TEXTURE_2D);
        glUseProgram(m->blur); glUniform1i(glGetUniformLocation(m->blur,"source"),0);
        for(int pass=0;pass<2;++pass)
        {
            glBindFramebuffer(GL_FRAMEBUFFER,m->bloomFbo[pass]); glViewport(0,0,640,400);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,pass==0?m->sceneTexture:m->bloomTexture[0]);
            glUniform2f(glGetUniformLocation(m->blur,"stepUV"),pass==0?2.f/1280.f:0.f,pass==1?1.f/400.f:0.f);
            glUniform1f(glGetUniformLocation(m->blur,"extractLight"),pass==0?1.f:0.f);
            Fullscreen();
        }
        glBindFramebuffer(GL_FRAMEBUFFER,0); glViewport(0,0,m->windowW,m->windowH);
        glClearColor(.018f,.025f,.028f,1); glClear(GL_COLOR_BUFFER_BIT); m->Viewport();
        glUseProgram(m->composite);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,m->sceneTexture);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,m->bloomTexture[1]);
        glUniform1i(glGetUniformLocation(m->composite,"scene"),0);
        glUniform1i(glGetUniformLocation(m->composite,"bloom"),1);
        glUniform2f(glGetUniformLocation(m->composite,"texel"),1.f/1280.f,1.f/800.f);
        Fullscreen();
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,0); glDisable(GL_TEXTURE_2D);
        glActiveTexture(GL_TEXTURE0); glUseProgram(0); glDisable(GL_TEXTURE_2D);
    }
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); Canvas();
}
void Renderer::DrawSolidRect(float x,float y,float z,float size,float r,float g,float b,float a)
{
    Rect(W*.5f+x-size*.5f,H*.5f-y-z-size*.5f,size,size,C(r,g,b,a));
}
