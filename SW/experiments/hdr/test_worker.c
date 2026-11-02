#include "bracket_merge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
 HdrFrame s={4,1,7,9,0,10,1000,1,16,35550,16,1000,1,1};
 HdrFrame l=s;l.frame_id=11;l.timestamp_us=1010;l.exposure_lines=4;
 uint16_t sr[]={116,316,1001,16},lr[]={416,1023,1023,16};
 uint32_t expected[4],out[4];uint8_t ef[4],flags[4];
 assert(!HdrBracket_Merge(&s,&l,sr,lr,4,50,expected,ef));
 for(unsigned budget=1;budget<=256;++budget) {
  HdrMergeJob j;HdrMerge_Init(&j);memset(out,0xa5,sizeof(out));memset(flags,0xa5,sizeof(flags));
  assert(!HdrMerge_Begin(&j,&s,&l,sr,lr,4,50,out,flags));
  assert(HdrMerge_Begin(&j,&s,&l,sr,lr,4,50,out,flags));
  HdrMergeJob saved=j;assert(HdrMerge_Step(&j,0)==-1&&HdrMerge_Step(&j,257)==-1);assert(!memcmp(&j,&saved,sizeof(j)));
  unsigned calls=0;int status;
  do {HdrMergeState before=j.state;size_t at=j.cursor;status=HdrMerge_Step(&j,budget);++calls;
   if(j.state==before)assert(j.cursor-at<=budget);
   if(before==HDR_MERGE_VALIDATE)for(unsigned i=0;i<4;++i)assert(out[i]==0xa5a5a5a5U&&flags[i]==0xa5);
  }while(status==0);
  assert(status==1&&calls==2*((4+budget-1)/budget));
  assert(!memcmp(out,expected,sizeof(out))&&!memcmp(flags,ef,sizeof(flags)));
  assert(HdrMerge_Step(&j,budget)==1);
 }
 /* Invalid last sample is found before any output is touched, even in tiny slices. */
 HdrMergeJob j;HdrMerge_Init(&j);memset(out,0xa5,sizeof(out));lr[3]=1024;
 assert(!HdrMerge_Begin(&j,&s,&l,sr,lr,4,50,out,flags));
 for(unsigned i=0;i<3;++i)assert(!HdrMerge_Step(&j,1));
 assert(HdrMerge_Step(&j,1)==-1&&j.state==HDR_MERGE_FAILED);
 for(unsigned i=0;i<4;++i)assert(out[i]==0xa5a5a5a5U);
 lr[3]=16;assert(!HdrMerge_Begin(&j,&s,&l,sr,lr,4,50,out,flags));
 assert(!HdrMerge_Step(&j,4));assert(!HdrMerge_Step(&j,1));
 HdrMerge_Cancel(&j);uint32_t partial[4];memcpy(partial,out,sizeof(out));
 assert(HdrMerge_Step(&j,1)==-1&&!memcmp(out,partial,sizeof(out)));
 puts("PASS bounded HDR worker budgets1..256, exact merge parity, immutable prevalidation output, active restart rejection and partial-output cancellation; offline only");
}
