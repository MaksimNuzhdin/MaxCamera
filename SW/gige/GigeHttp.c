/* Bounded raw-lwIP HTTP transport. R5 owns GEM; Linux never touches its MMIO. */
#include "GigeHttp.h"
#if MAX_CAMERA_WEB_HTTP
#include "lwip/tcp.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "GigeWebRom.h"
#if defined(__arm__) || defined(__aarch64__)
#define HTTP_BARRIER() __asm__ volatile("dmb sy" ::: "memory")
#else
#define HTTP_BARRIER() __sync_synchronize()
#endif
#define CLIENTS 4
#define HEADER_MAX 8192U
#define ERROR_REPLY(code,text) "HTTP/1.0 " code "\r\nConnection: close\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\nContent-Length: " text
static const unsigned char unavailable[]=ERROR_REPLY("503 Service Unavailable","20\r\n\r\nLinux console busy.\n");
static const unsigned char bad[]=ERROR_REPLY("400 Bad Request","18\r\n\r\nInvalid request.\r\n");
static const unsigned char too_large[]=ERROR_REPLY("413 Content Too Large","19\r\n\r\nRequest too large.\n");
static const unsigned char busy[]=ERROR_REPLY("409 Conflict","28\r\n\r\nStop acquisition to modify.\n");
static const unsigned char missing[]=ERROR_REPLY("404 Not Found","10\r\n\r\nNot found\n");
static volatile uint32_t *ctl;
static volatile uint8_t *req_mem,*resp_mem;
static uint32_t device_ip,sequence;
static int acquiring,slot=-1,slot_published;
static uint64_t tick,next_send,slot_started;
static struct tcp_pcb *listener;
struct client {
    struct tcp_pcb *pcb;
    char header[HEADER_MAX+1];
    uint32_t received,header_bytes,expected,sent,response_bytes,acked;
    const unsigned char *response;
    int shared,finished,waiting;
    uint64_t started;
};
static struct client clients[CLIENTS];
static void detach(unsigned index)
{
    struct client *c=clients+index;
    if(slot==(int)index){
        if(!slot_published)slot=-1;
        else slot=-2; /* Quarantine until the exact Linux completion arrives. */
    }
    memset(c,0,sizeof *c);
}
static void abort_client(unsigned index)
{
    struct tcp_pcb *pcb=clients[index].pcb;
    if(pcb){tcp_arg(pcb,0);tcp_err(pcb,0);tcp_abort(pcb);}
    detach(index);
}
static void error_callback(void *arg,err_t error)
{(void)error;if(arg)detach((unsigned)((struct client *)arg-clients));}
static void reply(struct client *c,const unsigned char *data,uint32_t bytes)
{c->response=data;c->response_bytes=bytes;c->sent=c->acked=0;c->finished=1;}
#define REPLY(c,s) reply(c,s,sizeof(s)-1)
static int equal_header(const char *line,size_t length,const char *key)
{
    size_t n=strlen(key);if(length<=n || line[n]!=':')return 0;
    for(size_t i=0;i<n;++i)if(tolower((unsigned char)line[i])!=(unsigned char)key[i])return 0;
    return 1;
}
/* No chunking, pipelining, ambiguous lengths, absolute URLs or proxy headers. */
static int parse_header(struct client *c)
{
    char method[8],path[96],version[16],extra;
    const char *line=strstr(c->header,"\r\n");
    if(!line)return -1;
    size_t first=(size_t)(line-c->header);char first_line[128];
    if(first>=sizeof first_line)return -1;
    memcpy(first_line,c->header,first);first_line[first]=0;
    if(sscanf(first_line,"%7s %95s %15s %c",method,path,version,&extra)!=3 ||
       (strcmp(version,"HTTP/1.1") && strcmp(version,"HTTP/1.0")))return -1;
    int post=!strcmp(method,"POST");
    if(strcmp(method,"GET") && !post)return -1;
    if(path[0]!='/' || strchr(path,'?') || strchr(path,'#'))return -1;
    unsigned hosts=0,lengths=0;uint32_t length=0;char expected_host[32];
    snprintf(expected_host,sizeof expected_host,"%u.%u.%u.%u",device_ip>>24,(device_ip>>16)&255,(device_ip>>8)&255,device_ip&255);
    for(line+=2;*line && strncmp(line,"\r\n",2);){
        const char *end=strstr(line,"\r\n");if(!end)return -1;
        size_t n=(size_t)(end-line);if(!n || *line==' ' || *line=='\t' || !memchr(line,':',n))return -1;
        const char *value=memchr(line,':',n);++value;
        while(value<end && (*value==' ' || *value=='\t'))++value;
        const char *value_end=end;while(value_end>value && (value_end[-1]==' ' || value_end[-1]=='\t'))--value_end;
        size_t size=(size_t)(value_end-value);
        if(equal_header(line,n,"host")){
            char host[40];if(++hosts!=1 || size>=sizeof host)return -1;
            memcpy(host,value,size);host[size]=0;
            char with_port[40];snprintf(with_port,sizeof with_port,"%s:80",expected_host);
            if(strcmp(host,expected_host) && strcmp(host,with_port))return -1;
        }
        if(equal_header(line,n,"content-length")){
            if(++lengths!=1 || !size)return -1;
            for(size_t i=0;i<size;++i){if(value[i]<'0'||value[i]>'9'||length>(GIGE_HTTP_REQUEST_MAX-(value[i]-'0'))/10)return -1;length=length*10+(value[i]-'0');}
        }
        if(equal_header(line,n,"transfer-encoding") || equal_header(line,n,"expect"))return -1;
        line=end+2;
    }
    if(hosts!=1 || (!post && length) || (post && lengths!=1))return -1;
    if(length>GIGE_HTTP_REQUEST_MAX-c->header_bytes)return -2;
    c->expected=c->header_bytes+length;
    if(!post){
        if(!strcmp(path,"/")){reply(c,web_index,sizeof web_index);return 1;}
        if(!strcmp(path,"/style.css")){reply(c,web_style,sizeof web_style);return 1;}
        if(!strcmp(path,"/app.js")){reply(c,web_app,sizeof web_app);return 1;}
        if(strcmp(path,"/api/status")){REPLY(c,missing);return 1;}
    }else{
        if(strcmp(path,"/api/config") && strcmp(path,"/api/profiles") && strcmp(path,"/api/gallery")){REPLY(c,missing);return 1;}
        if(acquiring){REPLY(c,busy);return 1;}
    }
    if(!ctl || slot==-2 || sequence==UINT32_MAX){REPLY(c,unavailable);return 1;}
    if(slot!=-1){
        /* Read-only status requests fit in each client's existing header.
         * Queue them without touching the active Linux request/response lease.
         * POST bodies still need the one shared upload slot. */
        if(!post){c->waiting=1;return 0;}
        REPLY(c,unavailable);return 1;
    }
    slot=(int)(c-clients);slot_published=0;slot_started=tick;
    for(uint32_t i=0;i<c->header_bytes;++i)req_mem[i]=(uint8_t)c->header[i];
    return 0;
}
static void publish(struct client *c)
{
    if(!c->finished && c->header_bytes && c->received==c->expected && slot==(int)(c-clients)){
        ctl[4]=c->received;HTTP_BARRIER();ctl[2]=++sequence;HTTP_BARRIER();slot_published=1;slot_started=tick;
    }
}
static err_t receive_callback(void *arg,struct tcp_pcb *pcb,struct pbuf *p,err_t error)
{
    struct client *c=arg;
    if(!p){
        /* FIN can mean a half-close after the request: retain its response. */
        if(!c->header_bytes || (!c->finished && c->received!=c->expected)){abort_client((unsigned)(c-clients));return ERR_ABRT;}
        return ERR_OK;
    }
    if(error!=ERR_OK){pbuf_free(p);return ERR_OK;}
    if(!c->finished && !(slot==(int)(c-clients) && slot_published)){
        for(struct pbuf *q=p;q;q=q->next){
            const unsigned char *data=q->payload;
            for(unsigned i=0;i<q->len && !c->finished;++i){
                if(!c->header_bytes){
                    if(c->received==HEADER_MAX){REPLY(c,too_large);break;}
                    unsigned char byte=data[i];
                    if(!byte || (byte<32 && byte!='\r' && byte!='\n' && byte!='\t')){REPLY(c,bad);break;}
                    c->header[c->received++]=(char)byte;c->header[c->received]=0;
                    if(c->received>=4 && !memcmp(c->header+c->received-4,"\r\n\r\n",4)){
                        c->header_bytes=c->received;int parsed=parse_header(c);
                        if(parsed<0){if(parsed==-2)REPLY(c,too_large);else REPLY(c,bad);}
                    }
                }else if(slot==(int)(c-clients) && c->received<c->expected){req_mem[c->received++]=data[i];}
                else {REPLY(c,bad);break;}
            }
        }
        publish(c);
    }
    tcp_recved(pcb,p->tot_len);pbuf_free(p);return ERR_OK;
}
static err_t sent_callback(void *arg,struct tcp_pcb *pcb,u16_t length)
{
    struct client *c=arg;c->acked+=length;
    if(c->finished && c->sent==c->response_bytes && c->acked==c->sent){
        tcp_arg(pcb,0);tcp_err(pcb,0);tcp_recv(pcb,0);tcp_sent(pcb,0);
        unsigned index=(unsigned)(c-clients);detach(index);
        if(tcp_close(pcb)!=ERR_OK){tcp_abort(pcb);return ERR_ABRT;}
    }
    return ERR_OK;
}
static err_t accept_callback(void *arg,struct tcp_pcb *pcb,err_t error)
{
    (void)arg;if(error!=ERR_OK)return error;
    for(unsigned i=0;i<CLIENTS;++i)if(!clients[i].pcb){
        struct client *c=clients+i;memset(c,0,sizeof *c);c->pcb=pcb;c->started=tick;
        tcp_arg(pcb,c);tcp_recv(pcb,receive_callback);tcp_sent(pcb,sent_callback);tcp_err(pcb,error_callback);
        tcp_nagle_disable(pcb);return ERR_OK;
    }
    tcp_abort(pcb);return ERR_ABRT;
}
void GigeHttp_Init(volatile uint32_t *control,volatile uint8_t *request,volatile uint8_t *response)
{
    ctl=control;req_mem=request;resp_mem=response;slot=-1;slot_published=0;sequence=0;
    memset(clients,0,sizeof clients);
    if(!ctl)return;
    for(unsigned i=0;i<16;++i)ctl[i]=0;
    ctl[1]=1;HTTP_BARRIER();ctl[0]=GIGE_HTTP_MAGIC;
    struct tcp_pcb *pcb=tcp_new_ip_type(IPADDR_TYPE_V4);
    if(!pcb)return;
    if(tcp_bind(pcb,IP_ANY_TYPE,80)!=ERR_OK){tcp_abort(pcb);return;}
    listener=tcp_listen_with_backlog(pcb,CLIENTS);
    if(!listener){tcp_abort(pcb);return;}
    tcp_accept(listener,accept_callback);
}
void GigeHttp_Poll(uint64_t now_us,uint32_t ip,int streaming,unsigned tx_free)
{
    tick=now_us;device_ip=ip;acquiring=streaming;
    if(!ctl)return;
    ctl[6]=ip;ctl[7]=(uint32_t)streaming;HTTP_BARRIER();
    if(streaming && slot>=0 && !slot_published){
        REPLY(clients+slot,busy);slot=-1;
    }
    if(slot_published){
        uint32_t done=ctl[3];HTTP_BARRIER();
        if(done==sequence){
            if(slot==-2){slot=-1;slot_published=0;}
            else if(slot>=0){
                struct client *c=clients+slot;uint32_t bytes=ctl[5];HTTP_BARRIER();
                if(!bytes || bytes>GIGE_HTTP_RESPONSE_MAX){REPLY(c,unavailable);slot=-1;slot_published=0;}
                else {c->shared=1;c->finished=1;c->response_bytes=bytes;}
            }
        }else if(slot>=0 && now_us-slot_started>10000000U){
            struct client *c=clients+slot;REPLY(c,unavailable);slot=-2;
        }
    }
    for(unsigned i=0;i<CLIENTS;++i)if(clients[i].pcb && now_us-clients[i].started>60000000U)abort_client(i);
    int pending=-1;
    for(unsigned i=0;i<CLIENTS;++i){
        struct client *c=clients+i;
        if(!c->pcb || !c->waiting || c->finished)continue;
        if(slot==-2 || sequence==UINT32_MAX){c->waiting=0;REPLY(c,unavailable);continue;}
        if(pending<0 || c->started<clients[pending].started)pending=(int)i;
    }
    if(slot==-1 && pending>=0){
        struct client *c=clients+pending;c->waiting=0;slot=pending;slot_published=0;slot_started=tick;
        for(uint32_t i=0;i<c->header_bytes;++i)req_mem[i]=(uint8_t)c->header[i];
        publish(c);
    }
    if(now_us<next_send || tx_free<16)return;
    for(unsigned offset=0;offset<CLIENTS;++offset){
        static unsigned turn;unsigned i=(turn+offset)%CLIENTS;struct client *c=clients+i;
        if(!c->pcb || !c->finished || c->sent>=c->response_bytes)continue;
        unsigned length=c->response_bytes-c->sent;if(length>512)length=512;
        if(length>tcp_sndbuf(c->pcb))continue;
        unsigned char data[512];
        if(c->shared){for(unsigned j=0;j<length;++j)data[j]=resp_mem[c->sent+j];}
        else memcpy(data,c->response+c->sent,length);
        if(tcp_write(c->pcb,data,(u16_t)length,TCP_WRITE_FLAG_COPY)!=ERR_OK)continue;
        c->sent+=length;(void)tcp_output(c->pcb);
        if(c->shared && c->sent==c->response_bytes){c->shared=0;slot=-1;slot_published=0;}
        next_send=now_us+(streaming?2000U:500U);turn=(i+1)%CLIENTS;break;
    }
}
#else
void GigeHttp_Init(volatile uint32_t *c,volatile uint8_t *q,volatile uint8_t *r){(void)c;(void)q;(void)r;}
void GigeHttp_Poll(uint64_t t,uint32_t ip,int s,unsigned f){(void)t;(void)ip;(void)s;(void)f;}
#endif
