/* Microsoft Video 1 (MSVC/CRAM), 8-bit paletted and RGB555. */
#include "mr_msvideo1.h"
#include <stdlib.h>
#include <string.h>

typedef struct { int w,h,stride,bits,valid; uint8_t *fb; uint8_t pal[768]; } ms1_ctx;

static void rgb555(uint16_t v, uint8_t *d)
{
    unsigned r=(v>>10)&31, g=(v>>5)&31, b=v&31;
    d[0]=(uint8_t)((r<<3)|(r>>2)); d[1]=(uint8_t)((g<<3)|(g>>2));
    d[2]=(uint8_t)((b<<3)|(b>>2));
}
static void color(ms1_ctx *c, uint16_t v, uint8_t *d)
{
    if (c->bits==8) memcpy(d,c->pal+(v&255)*3,3); else rgb555(v,d);
}
static void pixel(ms1_ctx *c,int x,int y,uint16_t v)
{
    if(x<c->w && y<c->h) color(c,v,c->fb+(size_t)y*c->stride+x*3);
}
static mr_status ms1_open(mr_decoder *d)
{
    ms1_ctx *c=(ms1_ctx*)calloc(1,sizeof(*c)); size_t n;
    if(!c)return MR_ENOMEM;
    c->w=d->width;c->h=d->height;c->stride=c->w*3;
    c->bits=(d->config_len>=2)?mr_rl16(d->config):16;
    if(c->bits!=8 && c->bits!=16){free(c);return MR_EUNSUPPORTED;}
    if(c->bits==8 && d->config_len>2) {
        uint32_t z=d->config_len-2; if(z>sizeof(c->pal))z=sizeof(c->pal);
        memcpy(c->pal,d->config+2,z);
    }
    n=(size_t)c->stride*c->h;c->fb=(uint8_t*)calloc(1,n);
    if(!c->fb){free(c);return MR_ENOMEM;} d->priv=c;d->frame.data=c->fb;
    d->frame.width=c->w;d->frame.height=c->h;d->frame.stride=c->stride;
    d->frame.fmt=MR_PIX_RGB24; return MR_OK;
}
static mr_status ms1_decode(mr_decoder *d,const uint8_t *p,uint32_t len)
{
    ms1_ctx*c=(ms1_ctx*)d->priv;const uint8_t*e=p+len;int bx=0,by=(c->h-1)/4;
    int changed0=c->h,changed1=0, blocks=((c->w+3)/4)*((c->h+3)/4), done=0;
    if(!p)return MR_EFORMAT;
    while(done<blocks){uint16_t a,cols[8];int x,y,n=2,eight=0;
        if(e-p<2)return MR_EFORMAT;
        a=mr_rl16(p);p+=2;
        if((a&0xfc00)==0x8400){int skip=(a&0x3ff);if(!skip)return MR_EFORMAT;
            if(skip>blocks-done)return MR_EFORMAT;
            done+=skip;bx+=skip;
            while(bx>=(c->w+3)/4){bx-=(c->w+3)/4;by--;}continue;
        }
        if(a&0x8000){cols[0]=a; n=1;}
        else {int bytes=c->bits==8?2:4;if(e-p<bytes)return MR_EFORMAT;
            cols[0]=c->bits==8?p[0]:mr_rl16(p);cols[1]=c->bits==8?p[1]:mr_rl16(p+2);p+=bytes;
            if(cols[1]&0x8000 || (c->bits==8 && (cols[1]&0x80))){eight=1;cols[1]&=c->bits==8?0x7f:0x7fff;n=8;
                bytes=(n-2)*(c->bits==8?1:2);if(e-p<bytes)return MR_EFORMAT;
                for(x=2;x<n;x++){cols[x]=c->bits==8?*p:mr_rl16(p);p+=c->bits==8?1:2;}
            }
        }
        for(y=0;y<4;y++)for(x=0;x<4;x++){
            int i=0;if(n>1){i=a&1;a>>=1;if(eight)i+=((y>>1)*2+(x>>1))*2;}
            pixel(c,bx*4+x,by*4+y,cols[i]);
        }
        if(by*4<changed0)changed0=by*4;
        if(by*4+4>changed1)changed1=by*4+4;
        done++;if(++bx>=(c->w+3)/4){bx=0;by--;}
    }
    if(changed0<0)changed0=0;
    if(changed1>c->h)changed1=c->h;
    c->valid=1;
    d->frame.dirty_y0=changed0;d->frame.dirty_y1=changed1;return MR_OK;
}
static void ms1_close(mr_decoder*d){ms1_ctx*c=(ms1_ctx*)d->priv;if(c){free(c->fb);free(c);}d->priv=NULL;}
const mr_codec mr_codec_msvideo1={"Microsoft Video 1 (MSVC)",
 {MR_FOURCC('M','S','V','C'),MR_FOURCC('m','s','v','c'),MR_FOURCC('C','R','A','M'),MR_FOURCC('c','r','a','m'),MR_FOURCC('W','H','A','M'),0},ms1_open,ms1_decode,ms1_close,NULL};
