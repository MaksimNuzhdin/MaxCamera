#include "GigeRemapReader.h"
#include <string.h>
static uint32_t read_reg(const GigeRemapReader *s,uint32_t offset)
{return s->io.read(s->io.user,GIGE_REMAP_READER_BASE+offset);}
static void write_reg(GigeRemapReader *s,uint32_t offset,uint32_t value)
{s->io.write(s->io.user,GIGE_REMAP_READER_BASE+offset,value);}
void GigeRemapReader_Init(GigeRemapReader *s,const GigeFpgaIO *io,int present)
{
    memset(s,0,sizeof(*s));if(io)s->io=*io;
    s->present=present && io && io->read && io->write;
}
int GigeRemapReader_CanReset(const GigeRemapReader *s)
{return s && (!s->present || !s->last_start_used_map || (read_reg(s,0)&4U));}
int GigeRemapReader_ResetObserved(GigeRemapReader *s)
{
    if(!GigeRemapReader_CanReset(s))return -1;
    s->configured=0;s->map_mode=0;s->last_start_used_map=0;return 0;
}
int GigeRemapReader_Configure(GigeRemapReader *s,uint32_t rows,uint32_t cols,
                             uint64_t address,int enabled)
{
    if(!s || !s->present || enabled<0 || enabled>1 || !rows || rows>1944 || !cols || cols>2592 ||
       (enabled && (rows>480 || cols>640 || !address || (address&63U))) ||
       !GigeRemapReader_CanReset(s) || !(read_reg(s,0)&4U))return -1;
    write_reg(s,0,0); /* Auto-restart stays disabled. */
    write_reg(s,0x10,enabled?(uint32_t)address:0);
    write_reg(s,0x14,enabled?(uint32_t)(address>>32):0);
    write_reg(s,0x1c,rows);write_reg(s,0x24,cols);write_reg(s,0x2c,(uint32_t)enabled);
    s->configured=1;s->map_mode=enabled;return 0;
}
int GigeRemapReader_Start(GigeRemapReader *s)
{
    if(!s || !s->present || !s->configured || !(read_reg(s,0)&4U))return -1;
    s->last_start_used_map=s->map_mode;write_reg(s,0,1);return 0;
}
int GigeRemapReader_Completed(const GigeRemapReader *s,uint32_t *status)
{
    if(!s || !s->present || !s->configured || !status)return -1;
    if(!(read_reg(s,0)&4U))return 0;
    *status=read_reg(s,0x34);return 1;
}
