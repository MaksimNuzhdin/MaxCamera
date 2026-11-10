/* Exercise raw TCP callbacks with a controlled lwIP boundary; no camera needed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/tcp.h"
#include "GigeHttp.h"
static struct tcp_pcb pcbs[32];
static unsigned count,writes,aborts,fail_write,fail_close;
static tcp_accept_fn accepted;
static unsigned char output[32][GIGE_HTTP_RESPONSE_MAX];
static unsigned sizes[32],acked[32];
struct tcp_pcb *tcp_new_ip_type(u8_t type)
{(void)type;assert(count<32);struct tcp_pcb *p=pcbs+count++;memset(p,0,sizeof *p);p->snd_buf=60000;return p;}
err_t tcp_bind(struct tcp_pcb *p,const ip_addr_t *ip,u16_t port)
{(void)p;(void)ip;assert(port==80);return ERR_OK;}
struct tcp_pcb *tcp_listen_with_backlog(struct tcp_pcb *p,u8_t backlog)
{assert(backlog==4);return p;}
struct tcp_pcb *tcp_listen_with_backlog_and_err(struct tcp_pcb *p,u8_t backlog,err_t *error)
{(void)error;assert(backlog==4);return p;}
void tcp_accept(struct tcp_pcb *p,tcp_accept_fn fn){(void)p;accepted=fn;}
void tcp_arg(struct tcp_pcb *p,void *arg){p->callback_arg=arg;}
void tcp_recv(struct tcp_pcb *p,tcp_recv_fn fn){p->recv=fn;}
void tcp_sent(struct tcp_pcb *p,tcp_sent_fn fn){p->sent=fn;}
void tcp_err(struct tcp_pcb *p,tcp_err_fn fn){p->errf=fn;}
void tcp_abort(struct tcp_pcb *p){(void)p;++aborts;}
err_t tcp_close(struct tcp_pcb *p){(void)p;return fail_close?ERR_MEM:ERR_OK;}
err_t tcp_write(struct tcp_pcb *p,const void *data,u16_t length,u8_t flags)
{
    assert(length<=512 && flags==TCP_WRITE_FLAG_COPY);if(fail_write)return ERR_MEM;
    unsigned i=(unsigned)(p-pcbs);assert(sizes[i]+length<=sizeof output[i]);
    memcpy(output[i]+sizes[i],data,length);sizes[i]+=length;++writes;return ERR_OK;
}
err_t tcp_output(struct tcp_pcb *p){(void)p;return ERR_OK;}
void tcp_recved(struct tcp_pcb *p,u16_t length){(void)p;assert(length<=1460);}
u8_t pbuf_free(struct pbuf *p){(void)p;return 1;}
const ip_addr_t ip_addr_any={0};
static volatile uint32_t control[1024];
static unsigned char request[GIGE_HTTP_REQUEST_MAX],response[GIGE_HTTP_RESPONSE_MAX];
static uint64_t now=1;
static unsigned received(struct tcp_pcb *p,const char *text)
{
    struct pbuf packet;memset(&packet,0,sizeof packet);packet.payload=(void *)text;
    packet.len=packet.tot_len=(u16_t)strlen(text);assert(packet.len<=1460);
    return p->recv(p->callback_arg,p,&packet,ERR_OK);
}
static struct tcp_pcb *connect_client(void)
{struct tcp_pcb *p=tcp_new_ip_type(0);assert(accepted(0,p,ERR_OK)==ERR_OK);return p;}
static void poll(int active,unsigned free_descriptors)
{GigeHttp_Poll(now,0x0a000008,active,free_descriptors);now+=2001;}
static void finish(struct tcp_pcb *p)
{
    unsigned index=(unsigned)(p-pcbs);
    for(unsigned i=0;i<300 && p->callback_arg;++i){
        poll(0,64);
        if(sizes[index]>acked[index]){unsigned length=sizes[index]-acked[index];acked[index]=sizes[index];assert(p->sent(p->callback_arg,p,(u16_t)length)==ERR_OK);}
    }
    if(p->callback_arg)fprintf(stderr,"unfinished client=%u sent=%u prefix=%.80s\n",index,sizes[index],output[index]);
    assert(!p->callback_arg);
    if(!memcmp(output[index],"HTTP/",5)){
        char *body=strstr((char *)output[index],"\r\n\r\n");
        char *field=strstr((char *)output[index],"Content-Length: ");unsigned length=0;
        assert(body && field && sscanf(field,"Content-Length: %u",&length)==1);
        if(length!=sizes[index]-(unsigned)(body+4-(char *)output[index]))fprintf(stderr,"length mismatch client=%u declared=%u actual=%u response=%.160s\n",index,length,sizes[index]-(unsigned)(body+4-(char *)output[index]),output[index]);
        assert(length==sizes[index]-(unsigned)(body+4-(char *)output[index]));
    }
}
int main(void)
{
    GigeHttp_Init(control,request,response);poll(0,64);
    struct tcp_pcb *p=connect_client();received(p,"GET / HTTP/1.1\r\nHo");
    assert(!writes && !control[2]);received(p,"st: 10.0.0.8\r\n\r\n");
    poll(0,15);assert(!writes);fail_write=1;poll(0,64);assert(!writes);fail_write=0;
    finish(p);assert(strstr((char *)output[p-pcbs],"MaxCamera"));
    p=connect_client();received(p,"GET /api/status HTTP/1.1\r\nHost: evil.example\r\n\r\n");finish(p);
    assert(strstr((char *)output[p-pcbs],"400 Bad Request"));
    p=connect_client();received(p,"POST /api/config HTTP/1.1\r\nHost: 10.0.0.8\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx");finish(p);assert(!control[2]);
    p=connect_client();received(p,"POST /api/config HTTP/1.1\r\nHost: 10.0.0.8\r\nTransfer-Encoding: chunked\r\n\r\n");finish(p);assert(!control[2]);
    poll(1,64);p=connect_client();received(p,"POST /api/config HTTP/1.1\r\nHost: 10.0.0.8\r\nContent-Length: 2\r\n\r\n{}");finish(p);
    assert(strstr((char *)output[p-pcbs],"409 Conflict") && !control[2]);
    poll(0,64);p=connect_client();const char *q="POST /api/config HTTP/1.1\r\nHost: 10.0.0.8\r\nContent-Length: 2\r\n\r\n{";
    received(p,q);assert(!control[2]);poll(1,64);finish(p);assert(!control[2]);
    poll(0,64);p=connect_client();received(p,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");
    assert(control[2]==1 && control[4]==strlen("GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n"));
    /* Losing the browser cannot revoke Linux's read lease or overwrite data. */
    p->errf(p->callback_arg,ERR_RST);
    struct tcp_pcb *other=connect_client();received(other,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");finish(other);
    assert(strstr((char *)output[other-pcbs],"503 Service Unavailable") && control[2]==1);
    control[5]=1;control[3]=1;poll(0,64);
    p=connect_client();received(p,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");assert(control[2]==2);
    memset(response,'Z',1200);control[5]=1200;control[3]=2;
    unsigned before=writes;poll(1,64);assert(writes==before+1);
    GigeHttp_Poll(now-2000,0x0a000008,1,64);assert(writes==before+1);
    finish(p);assert(sizes[p-pcbs]==1200 && output[p-pcbs][1199]=='Z');
    /* A timed-out backend slot remains quarantined until its completion. */
    p=connect_client();received(p,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");assert(control[2]==3);
    now+=10000001;poll(0,64);finish(p);assert(strstr((char *)output[p-pcbs],"503 Service Unavailable"));
    other=connect_client();received(other,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");finish(other);assert(control[2]==3);
    control[3]=3;poll(0,64);
    /* Concurrent status readers wait for the immutable backend lease. */
    p=connect_client();received(p,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\nX-Test: first\r\n\r\n");assert(control[2]==4);
    other=connect_client();received(other,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\nX-Test: second\r\n\r\n");
    assert(control[2]==4 && strstr((char *)request,"X-Test: first"));
    memset(response,'A',1200);control[5]=1200;control[3]=4;
    for(unsigned i=0;i<3;++i)poll(1,64);
    assert(control[2]==4);poll(1,64);assert(control[2]==5 && strstr((char *)request,"X-Test: second"));
    memset(response,'B',1200);control[5]=1200;control[3]=5;
    finish(p);finish(other);
    assert(sizes[p-pcbs]==1200 && output[p-pcbs][1199]=='A');
    assert(sizes[other-pcbs]==1200 && output[other-pcbs][1199]=='B');
    /* A waiting disconnect cannot revoke the current backend request. */
    p=connect_client();received(p,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");assert(control[2]==6);
    other=connect_client();received(other,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");
    other->errf(other->callback_arg,ERR_RST);assert(control[2]==6);
    memset(response,'C',1);control[5]=1;control[3]=6;finish(p);
    /* A stalled backend quarantines its request and fails queued readers. */
    p=connect_client();received(p,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");assert(control[2]==7);
    other=connect_client();received(other,"GET /api/status HTTP/1.1\r\nHost: 10.0.0.8\r\n\r\n");
    struct tcp_pcb *writer=connect_client();received(writer,"POST /api/profiles HTTP/1.1\r\nHost: 10.0.0.8\r\nContent-Length: 2\r\n\r\n{}");finish(writer);
    assert(strstr((char *)output[writer-pcbs],"503 Service Unavailable") && control[2]==7);
    now+=10000001;poll(0,64);finish(p);finish(other);
    assert(strstr((char *)output[p-pcbs],"503 Service Unavailable"));
    assert(strstr((char *)output[other-pcbs],"503 Service Unavailable") && control[2]==7);
    control[3]=7;poll(0,64);
    puts("Ethernet HTTP: PASS (fragmentation, framing, ownership, streaming gate, pacing, backpressure, timeout, disconnect lease, queued readers)");
    return 0;
}
