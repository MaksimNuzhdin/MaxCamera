#include "AxiGpioSccb.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <array>

struct Slave {
    unsigned tri=3,position=0,index=0,send_bits=0,starts=0,stops=0;
    unsigned bytes=0,fail_byte=0,recovery=0,release_after=0,writes=0;
    uint8_t value=0,transmit=0;
    uint16_t pointer=0;
    bool low=false,pending_read=false,nack=false,master_nack=false;
    bool stuck_scl=false,stuck_sda=false,stretch_read_stop=false;
    enum Phase {Idle,Receive,Send,MasterAck} phase=Idle;
    uint64_t elapsed=0;
    std::vector<uint8_t> sent;
    std::array<uint8_t,65536> regs{};
    unsigned levels() const {return ((tri&1)&&!stuck_scl?1U:0U)|((tri&2)&&!low&&!stuck_sda?2U:0U);}
    void byte() {
        sent.push_back(value);++bytes;
        nack=fail_byte==bytes;
        if(index==0) {
            nack=nack || (value!=0x78 && value!=0x79);
            pending_read=value==0x79;
        } else if(index==1)pointer=uint16_t(value)<<8;
        else if(index==2)pointer|=value;
        else if(!nack){regs[pointer]=value;++writes;}
        ++index;
    }
    void rising() {
        if(stuck_sda && release_after && ++recovery>=release_after)stuck_sda=false;
        if(phase==Receive) {
            if(position<8) {value=uint8_t((value<<1)|((tri>>1)&1));if(++position==8)byte();}
            else if(position==8)position=9;
        } else if(phase==Send) {
            ++send_bits;
        } else if(phase==MasterAck)master_nack=(tri&2)!=0;
    }
    void falling() {
        if(phase==Receive) {
            if(position==8)low=!nack;
            else if(position==9) {
                low=false;position=0;value=0;
                if(nack)phase=Idle;
                else if(pending_read) {
                    pending_read=false;phase=Send;send_bits=0;transmit=regs[pointer];
                    low=!(transmit&0x80);
                }
            }
        } else if(phase==Send) {
            if(send_bits==8){low=false;phase=MasterAck;}
            else low=!((transmit>>(7-send_bits))&1);
        } else if(phase==MasterAck){phase=Idle;low=false;if(stretch_read_stop)stuck_scl=true;}
    }
    void drive(unsigned next) {
        assert((next&~3U)==0);
        bool old_clock=(levels()&1)!=0;
        bool old_data=(levels()&2)!=0;
        tri=next;
        bool clock=(levels()&1)!=0,data=(levels()&2)!=0;
        if(old_clock && clock && old_data!=data) {
            if(!data) {++starts;phase=Receive;position=index=0;value=0;pending_read=false;low=false;}
            else {++stops;phase=Idle;low=false;}
        }
        if(!old_clock && clock)rising();
        if(old_clock && !clock)falling();
    }
} slave;
extern "C" uint32_t Xil_In32(uint32_t address)
{assert(address==0xa00d0000U);return slave.levels();}
extern "C" void Xil_Out32(uint32_t address,uint32_t value)
{
    if(address==0xa00d0000U)assert(value==0); // Open drain: never drive1.
    else {assert(address==0xa00d0004U);slave.drive(value);}
}
extern "C" int usleep(unsigned value){slave.elapsed+=value;return 0;}
static AXI_GPIO_SCCB setup()
{
    slave=Slave{};slave.regs[0x300a]=0x56;slave.regs[0x300b]=0x40;
    AXI_GPIO_SCCB bus{};assert(AxiGpioSccb_Init(&bus,0xa00d0000,50000)==0);return bus;
}
int main()
{
    AXI_GPIO_SCCB bus=setup();uint8_t id[2]={0};
    assert(AxiGpioSccb_Read(&bus,0x3c,0x300a,id,2)==0);
    assert(id[0]==0x56 && id[1]==0x40 && slave.master_nack);
    assert(slave.starts==4 && slave.stops==4);
    assert((slave.sent==std::vector<uint8_t>{0x78,0x30,0x0a,0x79,0x78,0x30,0x0b,0x79}));
    bus=setup();uint8_t values[2]={0x80,0x11};
    assert(AxiGpioSccb_Write(&bus,0x3c,0x503d,values,2)==0);
    assert(slave.regs[0x503d]==0x80 && slave.regs[0x503e]==0x11 && slave.writes==2);
    for(unsigned fail=1;fail<=4;++fail) {
        bus=setup();slave.fail_byte=fail;
        assert(AxiGpioSccb_Write(&bus,0x3c,0x503d,values,2)==-5);
        assert(slave.bytes==fail && slave.writes==0 && slave.stops==1);
        assert(slave.levels()==3); // Failure releases both wires.
    }
    bus=setup();slave.fail_byte=4;id[0]=0xa5;
    assert(AxiGpioSccb_Read(&bus,0x3c,0x300a,id,1)==-5);
    assert(id[0]==0xa5 && slave.bytes==4 && slave.writes==0 && slave.stops==2);
    bus=setup();slave.stretch_read_stop=true;id[0]=0xa5;
    assert(AxiGpioSccb_Read(&bus,0x3c,0x300a,id,1)==-4);
    assert(id[0]==0xa5 && slave.master_nack && slave.tri==3);
    bus=setup();unsigned before=slave.bytes;
    assert(AxiGpioSccb_Write(&bus,0x3c,0xffff,values,2)==-1);
    assert(AxiGpioSccb_Read(&bus,0x3c,0,id,0)==-1);
    assert(AxiGpioSccb_Read(&bus,0x78,0,id,1)==-1);
    assert(AxiGpioSccb_Read(nullptr,0x3c,0,id,1)==-1 && slave.bytes==before);
    bus=setup();slave.stuck_scl=true;uint64_t start=slave.elapsed;
    assert(AxiGpioSccb_Write(&bus,0x3c,0x503d,values,1)==-4);
    assert(slave.elapsed-start<2500 && slave.tri==3);
    slave=Slave{};slave.stuck_sda=true;slave.release_after=9;
    assert(AxiGpioSccb_Init(&bus,0xa00d0000,50000)==0 && slave.recovery==9);
    slave=Slave{};slave.stuck_sda=true;
    assert(AxiGpioSccb_Init(&bus,0xa00d0000,50000)==-3 && !bus.ready);
    assert(AxiGpioSccb_Init(&bus,0xa0020000,50000)==-1);
    std::puts("PASS: GPIO SCCB waveform, ACK/NACK, ID reads, STOP, early abort, open drain and recovery");
    return 0;
}
