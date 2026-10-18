#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
bool Near(float a,float b){return std::abs(a-b)<.002f;}
struct GdiFont{
    HDC dc{};HFONT font{};HGDIOBJ previous{};
    GdiFont(){dc=CreateCompatibleDC(nullptr);font=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
        CHINESEBIG5_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Microsoft JhengHei UI");
        Need(dc&&font,"independent GDI font unavailable");previous=SelectObject(dc,font);}
    ~GdiFont(){if(dc&&previous)SelectObject(dc,previous);if(font)DeleteObject(font);if(dc)DeleteDC(dc);}
};
}
int main(){try{
    GdiFont font;TEXTMETRICW metrics{};Need(GetTextMetricsW(font.dc,&metrics)!=0,"independent font metrics");
    Aevrynd::DrawList draw;
    // 空白字形可有字距而沒有點陣；不得讀取零長度 buffer。
    for(const auto blank:std::array<const char*,3>{" ","   ","　"}){
        draw.Clear();const auto size=draw.MeasureSystemText(blank,18);
        Need(size.x>0&&size.y>0,"blank glyph lost real advance");
        draw.SystemText({0,0},blank,18,{1,1,1,1});
        Need(draw.vertices.empty()&&draw.Stats2D().textSourcePixels==0,"blank glyph invented bitmap ink");
    }
    for(const auto bad:std::array<const char*,5>{"\xc0\xaf","\xe0\x80\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe4\xb8"}){
        draw.Clear();bool rejected=false;try{draw.SystemText({0,0},bad,18,{1,1,1,1});}catch(const std::exception&){rejected=true;}
        Need(rejected&&draw.vertices.empty(),"invalid UTF8 left partial system text");
    }
    for(const auto& item:std::array<std::pair<wchar_t,const char*>,7>{{{L'H',"H"},{L'g',"g"},{L'p',"p"},{L'y',"y"},{L'j',"j"},{L'霧',"霧"},{L'守',"守"}}}){
        GLYPHMETRICS glyph{};MAT2 matrix{};matrix.eM11.value=1;matrix.eM22.value=1;
        const auto size=GetGlyphOutlineW(font.dc,unsigned(item.first),GGO_GRAY8_BITMAP,&glyph,0,nullptr,&matrix);
        Need(size!=GDI_ERROR&&size>0,"independent glyph bitmap");std::vector<unsigned char> bytes(size);
        Need(GetGlyphOutlineW(font.dc,unsigned(item.first),GGO_GRAY8_BITMAP,&glyph,size,bytes.data(),&matrix)!=GDI_ERROR,"independent glyph pixels");
        const unsigned pitch=(glyph.gmBlackBoxX+3u)&~3u;
        float left=10000,top=10000,right=-10000,bottom=-10000;
        const float originX=float(glyph.gmptGlyphOrigin.x),originY=float(metrics.tmAscent-glyph.gmptGlyphOrigin.y);
        const float dx=20+originX-(std::min)(0.f,originX),dy=30+originY-(std::min)(0.f,originY);
        size_t sourcePixels{};
        for(unsigned y=0;y<glyph.gmBlackBoxY;++y)for(unsigned x=0;x<glyph.gmBlackBoxX;++x)
            if(bytes[size_t(y)*pitch+x]){++sourcePixels;left=(std::min)(left,dx+float(x));right=(std::max)(right,dx+float(x+1));
                top=(std::min)(top,dy+float(y));bottom=(std::max)(bottom,dy+float(y+1));}
        draw.Clear();draw.SystemText({20,30},item.second,18,{1,1,1,1});
        Need(!draw.vertices.empty()&&draw.Stats2D().textSourcePixels==sourcePixels,"independent bitmap coverage count differs");
        float actualLeft=10000,actualTop=10000,actualRight=-10000,actualBottom=-10000;
        for(const auto& v:draw.vertices){actualLeft=(std::min)(actualLeft,v.x);actualRight=(std::max)(actualRight,v.x);
            actualTop=(std::min)(actualTop,v.y);actualBottom=(std::max)(actualBottom,v.y);}
        Need(Near(left,actualLeft)&&Near(right,actualRight)&&Near(top,actualTop)&&Near(bottom,actualBottom),
             "glyph bearing or common baseline differs from independent GDI");
    }
    draw.Clear();draw.SystemText({0,0},"Hg\nHg",18,{1,1,1,1});
    const float actualLine=float(metrics.tmHeight+metrics.tmExternalLeading);
    const auto single=draw.MeasureSystemText("Hg",18),two=draw.MeasureSystemText("Hg\nHg",18);
    Need(Near(single.x,two.x)&&Near(two.y,single.y+actualLine),"actual line advance differs from font metrics");
    // 暖快取後重複顯示不新增字元，也不繞過原快取記憶體上限。
    draw.Clear();draw.SystemText({0,0},"霧境守望 Shift Ctrl gypj",18.2f,{1,1,1,1});
    const auto before=Aevrynd::DrawList::SystemGlyphCacheStats();
    for(unsigned n=0;n<120;++n){draw.Clear();draw.SystemText({0,0},"霧境守望 Shift Ctrl gypj",18.2f,{1,1,1,1});}
    const auto after=Aevrynd::DrawList::SystemGlyphCacheStats();
    Need(after.misses==before.misses&&after.residentGlyphs==before.residentGlyphs&&after.residentBytes==before.residentBytes&&after.hits>before.hits,
         "warmed system HUD repeatedly allocates glyph cache entries");
    Aevrynd::DrawList::TrimSystemGlyphCache(0);Need(Aevrynd::DrawList::SystemGlyphCacheStats().residentGlyphs==0,"new font cache not released by original trim API");
    std::cout<<"INDEPENDENT_GDI_BASELINE_PASS checks="<<checks<<" warmed_frames=120 GPU=not-run\n";return 0;
}catch(const std::exception& e){std::cerr<<"FONT_BASELINE_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
