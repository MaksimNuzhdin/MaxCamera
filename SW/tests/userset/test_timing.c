#include "GigeTiming.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 assert(GigeTiming_CanServiceSensor(14293,65000));
 assert(!GigeTiming_CanServiceSensor(14293,66000));
 assert(!GigeTiming_CanServiceSensor(14293,70000));
 assert(!GigeTiming_CanServiceSensor(0,0));
 assert(!GigeTiming_CanServiceSensor(14293,UINT64_MAX));
 assert(GigeTiming_MaxVts(50000000,1600)==32767);
 assert(GigeTiming_MaxVts(50000000,2844)==32767);
 assert(GigeTiming_MinRate(50000000,1600)==954);
 assert(GigeTiming_MinRate(50000000,2844)==537);
 assert(GigeTiming_Vts(50000000,1600,1042,5000)==6250);
 assert(GigeTiming_Vts(50000000,1600,1042,15000)==2084);
 assert(!GigeTiming_Vts(0,1600,1042,5000) && !GigeTiming_Vts(50000000,0,1042,5000));
 for(unsigned hts=1600;hts<=2844;hts+=1244) {
  unsigned max=GigeTiming_MaxVts(50000000,hts),min=GigeTiming_MinRate(50000000,hts);
  for(unsigned rate=min;rate<=30000;rate+=97) {
   unsigned vts=GigeTiming_Vts(50000000,hts,3375,rate);
   assert(vts>=3375 && vts<=max && (vts-4)/(50000000/hts/120)<=255);
  }
 }
 puts("PASS: FPS-to-VTS rounding, dynamic bounds, eight-bit AEC band counts, invalid inputs");
}
