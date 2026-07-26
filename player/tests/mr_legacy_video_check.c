#include "../core/mr_msvideo1.h"
#include "../core/mr_rle.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); failures++; } } while(0)
static mr_status start(mr_decoder*d,const mr_codec*c,int w,int h,const uint8_t*cfg,uint32_t n)
{ memset(d,0,sizeof(*d));d->codec=c;d->width=w;d->height=h;d->config=cfg;d->config_len=n;return c->open(d); }
static int eq(const uint8_t *p,int stride,int x,int y,int r,int g,int b)
{p+=y*stride+x*3;return p[0]==r&&p[1]==g&&p[2]==b;}

static void msvideo(void)
{
 uint8_t cfg16[]={16,0},cfg8[2+768]={8,0};mr_decoder d;mr_status s;int i;
 const uint8_t solid[]={0xe0,0x83}; /* RGB555 green, high bit denotes solid */
 const uint8_t two[]={0x55,0x55,0x00,0x7c,0x1f,0x00};
 const uint8_t eight[]={0x1b,0x1b,0x00,0x7c,0x1f,0x80,0xe0,0x03,0x00,0x7c,0x1f,0x00,0xe0,0x03,0xff,0x7f,0,0};
 const uint8_t skip[]={1,0x84};
 cfg8[2+1*3]=255;cfg8[2+2*3+1]=255;cfg8[2+3*3+2]=255;
 CHECK(start(&d,&mr_codec_msvideo1,4,4,cfg16,sizeof(cfg16))==MR_OK);
 s=d.codec->decode(&d,solid,sizeof(solid));CHECK(s==MR_OK);CHECK(eq(d.frame.data,d.frame.stride,2,2,0,255,0));
 s=d.codec->decode(&d,skip,sizeof(skip));CHECK(s==MR_OK);CHECK(eq(d.frame.data,d.frame.stride,2,2,0,255,0));CHECK(d.frame.dirty_y1<=d.frame.dirty_y0);
 s=d.codec->decode(&d,two,sizeof(two));CHECK(s==MR_OK);CHECK(eq(d.frame.data,d.frame.stride,0,0,0,0,255));CHECK(eq(d.frame.data,d.frame.stride,1,0,255,0,0));
 s=d.codec->decode(&d,eight,sizeof(eight));CHECK(s==MR_OK);
 s=d.codec->decode(&d,two,5);CHECK(s==MR_EFORMAT);d.codec->close(&d);
 CHECK(start(&d,&mr_codec_msvideo1,5,3,cfg16,sizeof(cfg16))==MR_OK);
 {const uint8_t edge[]={0x00,0xfc,0x1f,0x80};s=d.codec->decode(&d,edge,sizeof(edge));CHECK(s==MR_OK);CHECK(eq(d.frame.data,d.frame.stride,4,2,0,0,255));}
 d.codec->close(&d);
 CHECK(start(&d,&mr_codec_msvideo1,4,4,cfg8,sizeof(cfg8))==MR_OK);
 {const uint8_t p[]={0,0,1,2};s=d.codec->decode(&d,p,sizeof(p));CHECK(s==MR_OK);CHECK(eq(d.frame.data,d.frame.stride,0,0,255,0,0));}
 d.codec->close(&d);(void)i;
}
static void rle(void)
{
 uint8_t cfg[2+768]={8,0};mr_decoder d;mr_status s;
 /* red, green, blue */cfg[5]=255;cfg[2+2*3+1]=255;cfg[2+3*3+2]=255;
 CHECK(start(&d,&mr_codec_rle,5,3,cfg,sizeof(cfg))==MR_OK);
 {const uint8_t p[]={3,1,0,0, 0,3,2,3,1,0, 0,0, 0,2,1,0, 2,1,0,1};
  uint8_t guarded[sizeof(p)+2];guarded[0]=0xa5;memcpy(guarded+1,p,sizeof(p));guarded[sizeof(p)+1]=0x5a;
  s=d.codec->decode(&d,guarded+1,sizeof(p));CHECK(s==MR_OK);CHECK(guarded[0]==0xa5&&guarded[sizeof(p)+1]==0x5a);
  CHECK(eq(d.frame.data,d.frame.stride,0,2,255,0,0));CHECK(eq(d.frame.data,d.frame.stride,0,1,0,255,0));
  CHECK(eq(d.frame.data,d.frame.stride,1,1,0,0,255));CHECK(eq(d.frame.data,d.frame.stride,1,0,255,0,0));
 }
 {const uint8_t bad[]={6,1,0,1};s=d.codec->decode(&d,bad,sizeof(bad));CHECK(s==MR_EFORMAT);}
 d.codec->close(&d);
}
int main(void){msvideo();rle();if(failures)return 1;puts("legacy video decoder checks passed");return 0;}
