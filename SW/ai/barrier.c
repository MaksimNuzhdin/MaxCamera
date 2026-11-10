/* Native aligned accesses for reserved, potentially Device-mapped memory. */
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
void ai_barrier(void)
{
#if defined(__aarch64__) || defined(__arm__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __sync_synchronize();
#endif
}
uint32_t ai_read32(const volatile uint32_t *address)
{
    uint32_t value=*address;
    ai_barrier();
    return value;
}
void ai_write32(volatile uint32_t *address,uint32_t value)
{
    *address=value;
}
int ai_copy_physical(int fd,uint64_t address,void *destination,size_t length)
{
    if(address<0x100000U || (address&3U) || !length || length>15200000U ||
       address>=0x10000000U || length>0x10000000U-address) {errno=EINVAL;return -1;}
    long page_size=sysconf(_SC_PAGESIZE);
    if(page_size<=0)return -1;
    uint64_t page=address&~((uint64_t)page_size-1U);
    size_t delta=(size_t)(address-page),mapped=delta+length;
    void *mapping=mmap(0,mapped,PROT_READ,MAP_SHARED,fd,(off_t)page);
    if(mapping==MAP_FAILED)return -1;
    const volatile uint32_t *source=(const volatile uint32_t *)((const uint8_t *)mapping+delta);
    uint8_t *output=(uint8_t *)destination;
    ai_barrier();
    size_t i;
    for(i=0;i<length/4;++i) {
        /* Volatile forces one aligned32-bit load; never vectorize Device reads.
         * memcpy targets ordinary heap RAM and preserves native little endian. */
        uint32_t value=source[i];
        memcpy(output+4*i,&value,4);
    }
    const volatile uint8_t *tail=(const volatile uint8_t *)source;
    for(i=length&~(size_t)3;i<length;++i)output[i]=tail[i];
    ai_barrier();
    return munmap(mapping,mapped);
}

/* HTTP payloads occupy a separate reserved region. Device memory must use
 * aligned scalar accesses, including a padded final word. */
void ai_http_read(const volatile uint32_t *source,void *destination,size_t length)
{
    uint8_t *out=destination;ai_barrier();
    for(size_t i=0;i<length;i+=4){uint32_t value=source[i/4];size_t n=length-i;if(n>4)n=4;memcpy(out+i,&value,n);}
    ai_barrier();
}
void ai_http_write(volatile uint32_t *destination,const void *source,size_t length)
{
    const uint8_t *in=source;
    for(size_t i=0;i<length;i+=4){uint32_t value=0;size_t n=length-i;if(n>4)n=4;memcpy(&value,in+i,n);destination[i/4]=value;}
    ai_barrier();
}
