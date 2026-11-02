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
static unsigned be16(const uint8_t *p){return (p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t *p){return ((uint32_t)be16(p)<<16)|be16(p+2);}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char **argv)
{
 if(argc!=8 && argc!=9)return 2;
 /* Optional completed-frame trace for reset/rollover checks; default unchanged. */
 FILE *trace=argc==9?fopen(argv[8],"w"):0;
 if(argc==9 && !trace)return 2;
 if(trace){fputs("completed_host_seconds,block_id,timestamp_ticks\n",trace);fflush(trace);}
 const char *packet_path=getenv("MAXCAMERA_PACKET_TRACE");
 FILE *packet_trace=packet_path?fopen(packet_path,"w"):0;unsigned packet_traced=0;
 if(packet_path && !packet_trace)return 2;
 if(packet_trace)fputs("host_seconds,block_id,kind,packet_id,bytes\n",packet_trace);
 const char *cpu_env=getenv("MAXCAMERA_RECEIVER_CPU");
 if(cpu_env){int cpu=atoi(cpu_env);cpu_set_t chosen;CPU_ZERO(&chosen);if(cpu<0||cpu>=CPU_SETSIZE)return 2;CPU_SET(cpu,&chosen);if(sched_setaffinity(0,sizeof(chosen),&chosen))return 2;}

 unsigned width=strtoul(argv[2],0,0),height=strtoul(argv[3],0,0),pf=strtoul(argv[4],0,0),size=strtoul(argv[5],0,0);
 double seconds=strtod(argv[6],0);if(!width||!height||!size||size>16000000||seconds<=0)return 2;
 int s=socket(AF_INET,SOCK_DGRAM,0),rcvbuf=32*1024*1024;socklen_t alen=sizeof(struct sockaddr_in),olen=sizeof(rcvbuf);
 if(s<0)return 2;
 setsockopt(s,SOL_SOCKET,SO_RCVBUF,&rcvbuf,sizeof(rcvbuf));getsockopt(s,SOL_SOCKET,SO_RCVBUF,&rcvbuf,&olen);
 struct sockaddr_in addr={.sin_family=AF_INET,.sin_port=0};if(inet_pton(AF_INET,argv[1],&addr.sin_addr)!=1||bind(s,(void*)&addr,sizeof(addr)))return 2;
 getsockname(s,(void*)&addr,&alen);printf("{\"port\":%u,\"receive_buffer\":%d}\n",ntohs(addr.sin_port),rcvbuf);fflush(stdout);
 enum{BATCH=64,MAXPACKET=2048};uint8_t packets[BATCH][MAXPACKET];struct iovec iov[BATCH];struct mmsghdr msgs[BATCH];
 memset(msgs,0,sizeof(msgs));for(int i=0;i<BATCH;i++){iov[i].iov_base=packets[i];iov[i].iov_len=MAXPACKET;msgs[i].msg_hdr.msg_iov=&iov[i];msgs[i].msg_hdr.msg_iovlen=1;}
 uint8_t *current=calloc(1,size),*last=calloc(1,size);unsigned npackets=(size+1463)/1464;
 uint8_t *seen=calloc(npackets,1);if(!current||!last||!seen)return 2;
 unsigned good=0,bad=0,missing=0,duplicate=0,malformed=0,block=0,active=0,count=0,frame_bad=0;
 unsigned trailer_seen=0,highest_packet=0,reordered=0,early_trailers=0;
 double started=0,first=0,latest=0,wait_end=now()+20;uint64_t timestamp=0;
 while(1){
  double t=now();
  if(started && t-started>=seconds && (!active || !trailer_seen || t-started>=seconds+.1))break;
  if(!started && t>wait_end)break;
  struct pollfd fd={.fd=s,.events=POLLIN};if(poll(&fd,1,50)<=0)continue;
  int n=recvmmsg(s,msgs,BATCH,MSG_DONTWAIT,0);if(n<0){if(errno==EAGAIN||errno==EINTR)continue;return 2;}
  for(int i=0;i<n;i++){
   uint8_t *p=packets[i];unsigned len=msgs[i].msg_len;
   if(!started)started=now();
   if(len<8||be16(p)){malformed++;continue;}
   unsigned kind=p[4],id=be32(p+4)&0xffffff,bid=be16(p+2);
   if(packet_trace && packet_traced++<4096)fprintf(packet_trace,"%.9f,%u,%u,%u,%u\n",now(),bid,kind,id,len);
   if(active && bid==block && id){if(id<highest_packet)reordered++;else highest_packet=id;}
   if(kind==1){
    if(active){bad++;missing+=npackets-count;}
    block=bid;active=1;count=0;frame_bad=0;trailer_seen=0;highest_packet=0;memset(seen,0,npackets);
    if(id||len!=44||be16(p+10)!=1||be32(p+20)!=pf||be32(p+24)!=width||be32(p+28)!=height)frame_bad=1;
    if(!frame_bad)timestamp=((uint64_t)be32(p+12)<<32)|be32(p+16);
   }else if(active&&bid==block&&kind==3){
    if(!id||id>npackets){frame_bad=1;malformed++;continue;}
    unsigned offset=(id-1)*1464,bytes=size-offset;if(bytes>1464)bytes=1464;
    if(len!=bytes+8){frame_bad=1;malformed++;continue;}
    if(seen[id-1]){duplicate++;frame_bad=1;continue;}
    seen[id-1]=1;count++;memcpy(current+offset,p+8,bytes);
   }else if(active&&bid==block&&kind==2){
    if(len!=16||id!=npackets+1||be32(p+8)!=1||be32(p+12)!=height)frame_bad=1;
    trailer_seen=1;if(count<npackets)early_trailers++;
   }
   // UDP may deliver the trailer before earlier payload packets. Keep the
   // frame until every payload arrives or a subsequent leader proves a gap.
   if(active && trailer_seen && count==npackets){
    if(frame_bad){bad++;}
    else{double done=now();if(!good)first=done;latest=done;good++;
     if(trace){fprintf(trace,"%.9f,%u,%llu\n",done,block,(unsigned long long)timestamp);fflush(trace);}
     uint8_t *swap=last;last=current;current=swap;}
    active=0;
   }
  }
 }
 if(active && trailer_seen && count<npackets){bad++;missing+=npackets-count;}
 close(s);if(good){FILE *out=fopen(argv[7],"wb");if(!out||fwrite(last,1,size,out)!=size)return 2;fclose(out);}
 printf("{\"good\":%u,\"bad\":%u,\"missing_packets\":%u,\"duplicate_packets\":%u,\"malformed_packets\":%u,\"reordered_packets\":%u,\"early_trailers\":%u,\"delivered_fps\":%.6f,\"receive_buffer\":%d,\"seconds\":%.6f}\n",good,bad,missing,duplicate,malformed,reordered,early_trailers,good>1?(good-1)/(latest-first):0,rcvbuf,started?now()-started:0);
 if(trace && fclose(trace))return 2;
 if(packet_trace && fclose(packet_trace))return 2;
 free(current);free(last);free(seen);return good&&!bad&&!missing&&!duplicate&&!malformed?0:1;
}
