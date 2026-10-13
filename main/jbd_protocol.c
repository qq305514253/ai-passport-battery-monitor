#include "jbd_protocol.h"
#include <string.h>
const uint8_t JBD_QUERY_BASIC[7]={0xDD,0xA5,0x03,0,0xFF,0xFD,0x77};
const uint8_t JBD_QUERY_CELLS[7]={0xDD,0xA5,0x04,0,0xFF,0xFC,0x77};
const uint8_t JBD_QUERY_VERSION[7]={0xDD,0xA5,0x05,0,0xFF,0xFB,0x77};
static uint16_t be16(const uint8_t *p){return ((uint16_t)p[0]<<8)|p[1];}
static bool checksum_ok(const uint8_t *f,size_t n){uint16_t s=0;for(size_t i=2;i+3<n;i++)s+=f[i];return (uint16_t)(0U-s)==be16(f+n-3);}
static bool parse_frame(const uint8_t *f,size_t n,jbd_battery_data_t *d){
 const uint8_t cmd=f[1],len=f[3],*p=f+4;
 if(f[2]!=0||n!=(size_t)len+7||f[n-1]!=0x77||!checksum_ok(f,n))return false;
 if(cmd==3){if(len<23)return false;d->voltage_mv=be16(p)*10U;d->current_ma=(int16_t)be16(p+2)*10;d->remaining_mah=be16(p+4)*10U;d->nominal_mah=be16(p+6)*10U;d->cycles=be16(p+8);d->soc=p[19];d->mos_state=p[20];d->cell_count=p[21];if(d->soc>100||!d->cell_count||d->cell_count>JBD_MAX_CELLS)return false;d->basic_valid=true;return true;}
 if(cmd==4){if(!len||(len&1U))return false;size_t cells=len/2U;if(cells>JBD_MAX_CELLS)return false;for(size_t i=0;i<cells;i++)d->cell_mv[i]=be16(p+i*2U);d->cells_received=(uint8_t)cells;d->cells_valid=true;return true;}
 return cmd==5;
}
void jbd_stream_init(jbd_stream_t *s){memset(s,0,sizeof(*s));}
jbd_feed_result_t jbd_stream_feed(jbd_stream_t *s,const uint8_t *b,size_t n,jbd_battery_data_t *d){
 jbd_feed_result_t r=JBD_FEED_NONE;if(!s||!b||!d)return JBD_FEED_BAD_FRAME;
 for(size_t i=0;i<n;i++){if(s->length==JBD_RX_CAPACITY){s->length=0;r=JBD_FEED_BAD_FRAME;}s->bytes[s->length++]=b[i];while(s->length&&s->bytes[0]!=0xDD)memmove(s->bytes,s->bytes+1,--s->length);if(s->length<4)continue;size_t fn=(size_t)s->bytes[3]+7U;if(fn>JBD_RX_CAPACITY){s->length=0;r=JBD_FEED_BAD_FRAME;continue;}if(s->length<fn)continue;r=parse_frame(s->bytes,fn,d)?JBD_FEED_UPDATED:JBD_FEED_BAD_FRAME;memmove(s->bytes,s->bytes+fn,s->length-fn);s->length-=fn;}
 return r;
}
