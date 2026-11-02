#include "GigeCameraDiagnostics.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    GigeCameraFaultEvidence e={0};
    GigeCameraEvidence_Begin(&e,1280,960,0x02180014);
    assert(e.current.acquisition_generation==1 && e.current.width==1280 && e.current.height==960);
    assert(e.current.pixel_format==0x02180014 && !e.current.first_error.valid && !e.current.timeout.valid);
    GigeCameraEvidence_Record(&e.current.first_error,120,100,0x40000,0x40000,1,0x40000,17,2);
    GigeCameraEvidence_Record(&e.current.first_error,150,100,0x80000,0x80000,2,0xc0000,18,3);
    assert(e.current.first_error.poll_us==120 && e.current.first_error.irq==0x40000);
    assert(e.current.first_error.dma_arm_us==100 && e.current.first_error.dma_control==1);
    assert(e.current.first_error.completed_count==17 && e.current.first_error.capture_index==2);
    GigeCameraEvidence_Record(&e.current.timeout,3000,100,0,0,1,0xc0000,17,2);
    assert(e.current.timeout.valid && e.current.timeout.irq==0 && e.current.timeout.frame_error_union==0xc0000);
    assert(e.current.first_error.irq==0x40000); /* timeout never overwrites first IRQ */
    GigeCameraEvidence_Begin(&e,640,480,1);
    assert(e.current.acquisition_generation==2 && e.current.width==640 && !e.current.first_error.valid && !e.current.timeout.valid);
    assert(e.previous_fault.acquisition_generation==1 && e.previous_fault.first_error.irq==0x40000);
    assert(e.previous_fault.timeout.valid && e.previous_fault.width==1280);
    GigeCameraEvidence_Begin(&e,320,240,2);
    assert(e.previous_fault.acquisition_generation==1); /* clean recovery does not erase fault */
    puts("PASS fault evidence first observation retention, independent timeout, acquisition reset and software timestamps");
}
