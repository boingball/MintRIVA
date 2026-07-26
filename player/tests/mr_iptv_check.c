#include "../iptv/mr_iptv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
 mr_iptv_directory d;size_t ids[8];mr_iptv_filter f;
 const char*c="[{\"id\":\"BBCNews.uk\",\"name\":\"BBC News\",\"alt_names\":[\"BBC World\"],\"network\":\"BBC\",\"country\":\"GB\",\"categories\":[\"news\"],\"is_nsfw\":false,\"unknown\":{\"x\":[1]}},{\"id\":\"Fun.uk\",\"name\":\"Fun \\u2603\",\"alt_names\":[],\"network\":\"Net\",\"country\":\"GB\",\"categories\":[\"entertainment\"],\"is_nsfw\":false},{\"id\":\"US.us\",\"name\":\"US One\",\"country\":\"US\",\"categories\":[\"news\"],\"alt_names\":[],\"is_nsfw\":false},{\"id\":\"Adult.uk\",\"name\":\"Adult\",\"country\":\"GB\",\"categories\":[],\"alt_names\":[],\"is_nsfw\":true},{\"id\":\"Closed.uk\",\"name\":\"Closed\",\"country\":\"GB\",\"categories\":[],\"alt_names\":[],\"is_nsfw\":false,\"closed\":\"2020\"},{\"id\":\"Empty.uk\",\"name\":\"Empty\",\"country\":\"GB\",\"categories\":[],\"alt_names\":[],\"is_nsfw\":false}]";
 const char*s="[{\"channel\":\"BBCNews.uk\",\"url\":\"https://one.test/live.m3u8\",\"status\":\"online\"},{\"channel\":\"BBCNews.uk\",\"url\":\"http://two.test/live\",\"http_referrer\":\"https://ref.test/\"},{\"channel\":\"Fun.uk@East\",\"url\":\"https://fun.test/a.ts\"},{\"channel\":\"US.us\",\"url\":\"not a url\"},{\"channel\":\"Adult.uk\",\"url\":\"http://adult.test/a\"},{\"channel\":\"Closed.uk\",\"url\":\"http://closed.test/a\"}]";
 const char*m="#EXTM3U\n#EXTINF:-1 tvg-id=\"nasa.us\" tvg-name=\"NASA TV\" group-title=\"Science\",NASA\nhttps://nasa.test/live.m3u8\n";
 mr_iptv_init(&d);assert(mr_iptv_parse_channels(&d,c,strlen(c)));assert(d.channel_count==6);assert(mr_iptv_join_streams(&d,s,strlen(s)));assert(d.channels[0].stream_count==2);assert(d.channels[1].stream_count==1);assert(!strcmp(d.channels[1].name,"Fun ?"));
 f.country="GB";f.category="news";f.search="world";assert(mr_iptv_filter_channels(&d,&f,ids,8)==1&&ids[0]==0);f.category="All";f.search="net";assert(mr_iptv_filter_channels(&d,&f,ids,8)==1);assert(!mr_iptv_parse_channels(&d,"[{",2));assert(d.channel_count==6);assert(!mr_iptv_join_streams(&d,"[",1));mr_iptv_free(&d);
 mr_iptv_init(&d);assert(mr_iptv_parse_m3u(&d,m,strlen(m)));assert(d.channel_count==1);assert(!strcmp(d.channels[0].id,"nasa.us"));assert(!strcmp(d.channels[0].categories[0],"Science"));assert(!mr_iptv_parse_m3u(&d,"bad",3));assert(d.channel_count==1);mr_iptv_free(&d);
 puts("IPTV parser/filter checks passed");return 0;
}
