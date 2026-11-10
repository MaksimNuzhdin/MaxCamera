/* Independent MTU1500 classic-ID GVSP receiver. No resend or privileged sockets.
 * recvmmsg amortizes syscall overhead; validates every packet and complete frame. */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <linux/sock_diag.h>
static unsigned be16(const uint8_t *p){return (p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t *p){return ((uint32_t)be16(p)<<16)|be16(p+2);}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
enum { EVENT_RING=4096, EVENT_LIMIT=65536 };
typedef struct {double time;unsigned block,kind,id,bytes;} PacketEvent;
static PacketEvent events[EVENT_RING];
static unsigned event_count,event_written,event_follow;
static void event_write(FILE *file,const PacketEvent *event)
{if(event_written<EVENT_LIMIT){fprintf(file,"%.9f,%u,%u,%u,%u\n",event->time,event->block,event->kind,event->id,event->bytes);++event_written;}}
static void event_record(FILE *file,unsigned block,unsigned kind,unsigned id,unsigned bytes)
{
 PacketEvent event={now(),block,kind,id,bytes};events[event_count++%EVENT_RING]=event;
 if(event_follow){event_write(file,&event);--event_follow;}
}
static void event_failure(FILE *file)
{
 if(!file)return;
 unsigned count=event_count<EVENT_RING?event_count:EVENT_RING;
 for(unsigned i=0;i<count;++i)event_write(file,&events[(event_count-count+i)%EVENT_RING]);
 event_follow=EVENT_RING;fflush(file);
}
enum { FRAME_WINDOW=4 };
typedef struct {
 uint8_t *data,*seen;
 unsigned block,size,packets,count,bad,trailer,highest,hashed;
 uint64_t timestamp,sum,hash;
} Frame;
typedef struct {
 Frame frames[FRAME_WINDOW];unsigned head,used;
 uint8_t *last;unsigned last_size,maximum,capacity;
 unsigned good,bad,missing,duplicate,malformed,reordered,early,foreign,interleaved;
 unsigned previous_leader,block_gaps,sequence_errors;
 unsigned frame_limit,save_skip;const char *output;
 double first,latest;FILE *trace,*packet_trace;int failure_trace;
} Receiver;
static void fail_frame(Receiver *r,Frame *f)
{
 ++r->bad;r->missing+=f->packets-f->count;
 if(r->failure_trace)event_failure(r->packet_trace);
}
static int save_frame(Receiver *r,Frame *f)
{
 double done=now();if(!r->good)r->first=done;r->latest=done;++r->good;
 if((r->good==1 && f->size<=1000000) || (r->frame_limit && r->good>r->save_skip)){
  char *name=malloc(strlen(r->output)+32);if(!name)return 0;
  if(r->good==1 && f->size<=1000000){
   sprintf(name,"%s.first.bin",r->output);FILE *file=fopen(name,"wb");
   if(!file || fwrite(f->data,1,f->size,file)!=f->size){free(name);return 0;}fclose(file);
  }
  if(r->frame_limit && r->good>r->save_skip){
   sprintf(name,"%s.frame%06u.bin",r->output,r->good);FILE *file=fopen(name,"wb");
   if(!file || fwrite(f->data,1,f->size,file)!=f->size){free(name);return 0;}fclose(file);
  }
  free(name);
 }
 if(r->trace){
  fprintf(r->trace,"%.9f,%u,%llu,%.6f,%016llx\n",done,f->block,(unsigned long long)f->timestamp,(double)f->sum/f->size,(unsigned long long)f->hash);fflush(r->trace);
 }
 r->last_size=f->size;uint8_t *swap=r->last;r->last=f->data;f->data=swap;return 1;
}
/* Commit in leader order. A later complete frame cannot evict an earlier
 * delayed payload until the bounded four-frame receive window is exhausted. */
static int drain(Receiver *r)
{
 while(r->used){
  Frame *f=&r->frames[r->head];if(!f->trailer || f->count!=f->packets)break;
  if(f->bad)fail_frame(r,f);else if(!save_frame(r,f))return 0;
  r->head=(r->head+1)%FRAME_WINDOW;--r->used;
  if(r->frame_limit && r->good>=r->frame_limit)break;
 }
 return 1;
}
int main(int argc,char **argv)
{
 if(argc!=8 && argc!=9)return 2;
 Receiver r={0};r.output=argv[7];
 r.trace=argc==9?fopen(argv[8],"w"):0;if(argc==9 && !r.trace)return 2;
 if(r.trace){fputs("completed_host_seconds,block_id,timestamp_ticks,byte_mean,fnv64\n",r.trace);fflush(r.trace);}
 const char *packet_path=getenv("MAXCAMERA_PACKET_TRACE");
 r.packet_trace=packet_path?fopen(packet_path,"w"):0;unsigned packet_traced=0;
 r.failure_trace=getenv("MAXCAMERA_TRACE_FAILURES") && !strcmp(getenv("MAXCAMERA_TRACE_FAILURES"),"1");
 if(packet_path && !r.packet_trace)return 2;
 if(r.packet_trace)fputs("host_seconds,block_id,kind,packet_id,bytes\n",r.packet_trace);
 const char *cpu_env=getenv("MAXCAMERA_RECEIVER_CPU");
 if(cpu_env){int cpu=atoi(cpu_env);cpu_set_t chosen;CPU_ZERO(&chosen);if(cpu<0||cpu>=CPU_SETSIZE)return 2;CPU_SET(cpu,&chosen);if(sched_setaffinity(0,sizeof(chosen),&chosen))return 2;}
 unsigned width=strtoul(argv[2],0,0),height=strtoul(argv[3],0,0),pf=strtoul(argv[4],0,0),size=strtoul(argv[5],0,0);
 const char *limit_env=getenv("MAXCAMERA_FRAME_LIMIT"),*skip_env=getenv("MAXCAMERA_SAVE_SKIP");
 r.frame_limit=limit_env?strtoul(limit_env,0,0):0;r.save_skip=skip_env?strtoul(skip_env,0,0):0;
 const char *timeout_env=getenv("MAXCAMERA_FRAME_TIMEOUT");double timeout=timeout_env?strtod(timeout_env,0):0;
 int jpeg=getenv("MAXCAMERA_JPEG") && !strcmp(getenv("MAXCAMERA_JPEG"),"1");
 if(r.frame_limit>4096 || r.save_skip>4096 || (r.save_skip && r.save_skip>=r.frame_limit))return 2;
 double seconds=strtod(argv[6],0);if(!width||!height||!size||size>16000000||seconds<=0)return 2;
 int s=socket(AF_INET,SOCK_DGRAM,0),rcvbuf=32*1024*1024;socklen_t alen=sizeof(struct sockaddr_in),olen=sizeof(rcvbuf);
 if(s<0)return 2;
 setsockopt(s,SOL_SOCKET,SO_RCVBUF,&rcvbuf,sizeof(rcvbuf));getsockopt(s,SOL_SOCKET,SO_RCVBUF,&rcvbuf,&olen);
 struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=0};if(inet_pton(AF_INET,argv[1],&addr.sin_addr)!=1||bind(s,(void*)&addr,sizeof(addr)))return 2;
 getsockname(s,(void*)&addr,&alen);printf("{\"port\":%u,\"receive_buffer\":%d}\n",ntohs(addr.sin_port),rcvbuf);fflush(stdout);
 enum{BATCH=64,MAXPACKET=2048};uint8_t packets[BATCH][MAXPACKET];struct iovec iov[BATCH];struct mmsghdr msgs[BATCH];
 memset(msgs,0,sizeof(msgs));for(int i=0;i<BATCH;i++){iov[i].iov_base=packets[i];iov[i].iov_len=MAXPACKET;msgs[i].msg_hdr.msg_iov=&iov[i];msgs[i].msg_hdr.msg_iovlen=1;}
 r.maximum=size;r.capacity=(size+1463)/1464;r.last=calloc(1,size);if(!r.last)return 2;
 for(unsigned i=0;i<FRAME_WINDOW;++i){r.frames[i].data=calloc(1,size);r.frames[i].seen=calloc(r.capacity,1);if(!r.frames[i].data||!r.frames[i].seen)return 2;}
 double started=0,wait_end=now()+4;
 while(1){
  if(r.frame_limit && r.good>=r.frame_limit)break;
  double t=now();
  if(started && timeout>0 && t-(r.good?r.latest:started)>=timeout)break;
  if(started && t-started>=seconds && (!r.used || t-started>=seconds+.2))break;
  if(!started && t>wait_end)break;
  struct pollfd fd={.fd=s,.events=POLLIN};if(poll(&fd,1,20)<=0)continue;
  int n=recvmmsg(s,msgs,BATCH,MSG_DONTWAIT,0);if(n<0){if(errno==EAGAIN||errno==EINTR)continue;return 2;}
  for(int i=0;i<n;i++){
   uint8_t *p=packets[i];unsigned len=msgs[i].msg_len;if(!started)started=now();
   if(len<8||be16(p)){++r.malformed;continue;}
   unsigned kind=p[4],id=be32(p+4)&0xffffff,bid=be16(p+2);
   if(r.packet_trace){if(r.failure_trace)event_record(r.packet_trace,bid,kind,id,len);else if(packet_traced++<4096)fprintf(r.packet_trace,"%.9f,%u,%u,%u,%u\n",now(),bid,kind,id,len);}
   Frame *f=0;
   for(unsigned j=0;j<r.used;++j){Frame *candidate=&r.frames[(r.head+j)%FRAME_WINDOW];if(candidate->block==bid){f=candidate;if(j+1<r.used)++r.interleaved;break;}}
   if(kind==1){
    if(f){++r.duplicate;f->bad=1;continue;}
    /* Do not admit another frame beyond the measurement window. Existing
     * frames still receive their late payload/trailer for up to200ms. */
    if(now()-started>=seconds)continue;
    if(!bid){++r.sequence_errors;if(r.failure_trace)event_failure(r.packet_trace);}
    else if(r.previous_leader){unsigned delta=(bid+65535U-r.previous_leader)%65535U;
     if(!delta||delta>32767U){++r.sequence_errors;if(r.failure_trace)event_failure(r.packet_trace);}
     else if(delta>1){r.block_gaps+=delta-1;if(r.failure_trace)event_failure(r.packet_trace);}}
    r.previous_leader=bid;
    if(r.used==FRAME_WINDOW){fail_frame(&r,&r.frames[r.head]);r.head=(r.head+1)%FRAME_WINDOW;--r.used;}
    f=&r.frames[(r.head+r.used++)%FRAME_WINDOW];
    f->block=bid;f->size=size;f->packets=r.capacity;f->count=f->bad=f->trailer=f->highest=f->hashed=0;f->timestamp=f->sum=f->hash=0;memset(f->seen,0,r.capacity);
    if(jpeg){
     if(id||len!=40||be16(p+10)!=6||be32(p+20)||!be32(p+24)||be32(p+24)>size||be32(p+28)||be32(p+32)!=1000000||be32(p+36))f->bad=1;
     else f->size=be32(p+24);
    }else if(id||len!=44||be16(p+10)!=1||be32(p+20)!=pf||be32(p+24)!=width||be32(p+28)!=height)f->bad=1;
    f->packets=(f->size+1463)/1464;if(f->size<=1000000)f->hash=14695981039346656037ULL;if(!f->bad)f->timestamp=((uint64_t)be32(p+12)<<32)|be32(p+16);
   }else if(f && kind==3){
    if(!id||id>f->packets){f->bad=1;++r.malformed;continue;}
    if(id<f->highest)++r.reordered;else f->highest=id;
    unsigned offset=(id-1)*1464,bytes=f->size-offset;if(bytes>1464)bytes=1464;
    if(len!=bytes+8){f->bad=1;++r.malformed;continue;}
    if(f->seen[id-1]){++r.duplicate;f->bad=1;continue;}
    f->seen[id-1]=1;++f->count;memcpy(f->data+offset,p+8,bytes);
    /* Keep exact raster-order statistics, but distribute their CPU work over
     * received packets instead of blocking socket draining at frame end. */
    if(r.trace)while(f->hashed<f->packets && f->seen[f->hashed]){
     unsigned at=f->hashed*1464U,nbytes=f->size-at;if(nbytes>1464)nbytes=1464;
     if(f->size<=1000000){for(unsigned j=0;j<nbytes;++j){unsigned byte=f->data[at+j];f->sum+=byte;f->hash=(f->hash^byte)*1099511628211ULL;}}
     else for(unsigned j=0;j<nbytes;++j)f->sum+=f->data[at+j];
     ++f->hashed;
    }
   }else if(f && kind==2){
    if(jpeg){if(len!=12||id!=f->packets+1||be32(p+8)!=6)f->bad=1;}
    else if(len!=16||id!=f->packets+1||be32(p+8)!=1||be32(p+12)!=height)f->bad=1;
    f->trailer=1;if(f->count<f->packets)++r.early;
   }else if(kind==2||kind==3)++r.foreign;
   if(!drain(&r))return 2;
   if(r.frame_limit && r.good>=r.frame_limit)break;
  }
 }
 /* Only the final unterminated frame can be an intentional Stop boundary.
  * Earlier incomplete frames, or a received trailer with gaps, always fail. */
 while(r.used && !(r.frame_limit && r.good>=r.frame_limit)){Frame *f=&r.frames[r.head];if(r.used>1 || f->trailer)fail_frame(&r,f);r.head=(r.head+1)%FRAME_WINDOW;--r.used;if(!drain(&r))return 2;}
 uint32_t meminfo[SK_MEMINFO_VARS]={0};socklen_t memlen=sizeof(meminfo);int memok=getsockopt(s,SOL_SOCKET,SO_MEMINFO,meminfo,&memlen)==0;
 fprintf(stderr,"SOCKET_MEMINFO available=%d drops=%u rcvbuf=%u\n",memok,meminfo[SK_MEMINFO_DROPS],meminfo[SK_MEMINFO_RCVBUF]);close(s);
 if(r.good){FILE *out=fopen(argv[7],"wb");if(!out||fwrite(r.last,1,r.last_size,out)!=r.last_size)return 2;fclose(out);}
 printf("{\"good\":%u,\"bad\":%u,\"missing_packets\":%u,\"duplicate_packets\":%u,\"malformed_packets\":%u,\"reordered_packets\":%u,\"early_trailers\":%u,\"foreign_frame_packets\":%u,\"interleaved_packets\":%u,\"frame_window\":%u,\"block_gaps\":%u,\"sequence_errors\":%u,\"socket_meminfo_available\":%s,\"socket_drops\":%u,\"delivered_fps\":%.6f,\"receive_buffer\":%d,\"seconds\":%.6f}\n",r.good,r.bad,r.missing,r.duplicate,r.malformed,r.reordered,r.early,r.foreign,r.interleaved,FRAME_WINDOW,r.block_gaps,r.sequence_errors,memok?"true":"false",meminfo[SK_MEMINFO_DROPS],r.good>1?(r.good-1)/(r.latest-r.first):0,rcvbuf,started?now()-started:0);
 if(r.trace && fclose(r.trace))return 2;
 if(r.packet_trace && fclose(r.packet_trace))return 2;
 free(r.last);for(unsigned i=0;i<FRAME_WINDOW;++i){free(r.frames[i].data);free(r.frames[i].seen);}
 return r.good&&!r.bad&&!r.missing&&!r.duplicate&&!r.malformed&&!r.block_gaps&&!r.sequence_errors&&(!memok||!meminfo[SK_MEMINFO_DROPS])?0:1;
}
