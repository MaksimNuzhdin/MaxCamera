/* Bounded classic-ID GVSP receiver. Called with an inherited, prebound socket. */
#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
static unsigned be16(const uint8_t*p){return p[0]*256u+p[1];}
static unsigned be32(const uint8_t*p){return (be16(p)<<16)|be16(p+2);}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
/* 1 complete, 0 timeout, -1 invalid geometry. No partial frames leave this API. */
int receive_frame(int fd,uint8_t*out,unsigned size,unsigned width,unsigned height,unsigned pf,unsigned camera_ip,unsigned *bad){
 if(!size||size>16000000)return -1;
 enum {BATCH=64,MAXPACKET=10000,MAXIDS=16384};
 uint8_t packets[BATCH][MAXPACKET],seen[MAXIDS];
 struct iovec iov[BATCH];struct mmsghdr msgs[BATCH];struct sockaddr_in_dummy{uint16_t family,port;uint32_t ip;uint8_t zero[8];} peers[BATCH];
 memset(msgs,0,sizeof(msgs));
 for(int i=0;i<BATCH;i++){iov[i].iov_base=packets[i];iov[i].iov_len=MAXPACKET;msgs[i].msg_hdr.msg_iov=&iov[i];msgs[i].msg_hdr.msg_iovlen=1;msgs[i].msg_hdr.msg_name=&peers[i];}
 unsigned block=0,active=0,count=0,trailer=0,payload_bytes=0,npackets=0;double end=now()+.5;
 while(now()<end){
  struct pollfd pfd={.fd=fd,.events=POLLIN};if(poll(&pfd,1,30)<=0)continue;
  for(int i=0;i<BATCH;i++)msgs[i].msg_hdr.msg_namelen=sizeof(peers[i]);
  int n=recvmmsg(fd,msgs,BATCH,MSG_DONTWAIT,0);if(n<0){if(errno==EAGAIN||errno==EINTR)continue;return 0;}
  for(int i=0;i<n;i++){
   uint8_t*p=packets[i];unsigned len=msgs[i].msg_len;
   if(peers[i].ip!=camera_ip||len<8||be16(p))continue;
   unsigned bid=be16(p+2),kind=p[4],id=be32(p+4)&0xffffff;
   if(kind==1){
    if(active)(*bad)++;
    active=0;
    if(id||len!=44||be32(p+20)!=pf||be32(p+24)!=width||be32(p+28)!=height)continue;
    block=bid;active=1;count=0;trailer=0;payload_bytes=0;npackets=0;memset(seen,0,sizeof(seen));
   }else if(active&&bid==block&&kind==3){
    if(len<=8){active=0;(*bad)++;continue;}
    if(!payload_bytes){if(id!=1){active=0;(*bad)++;continue;}payload_bytes=len-8;npackets=(size+payload_bytes-1)/payload_bytes;}
    if(!id||id>npackets||npackets>=MAXIDS){active=0;(*bad)++;continue;}
    unsigned offset=(id-1)*payload_bytes,bytes=size-offset;if(bytes>payload_bytes)bytes=payload_bytes;
    if(len!=bytes+8){active=0;(*bad)++;continue;}
    if(!seen[id-1]){seen[id-1]=1;count++;memcpy(out+offset,p+8,bytes);}
   }else if(active&&bid==block&&kind==2){
    if(len!=16||be32(p+8)!=1||be32(p+12)!=height){active=0;(*bad)++;continue;}trailer=id;
   }
   if(active&&payload_bytes&&trailer==npackets+1&&count==npackets){
    /* Leave later packets in kernel on subsequent calls: this batch may contain a
       newer leader, so those frames are intentionally skipped for preview. */
    return 1;
   }
  }
 }
 if(active)(*bad)++;
 return 0;
}
