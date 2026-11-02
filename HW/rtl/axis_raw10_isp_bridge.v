`timescale 1ns/1ps
// Lossless CSI RAW10 pixel adapter around the HLS correction core.
// Enable changes only with the receiver stopped and the video path reset.
// RAW lines must contain a multiple of four pixels. Bypass preserves all bits.
module axis_raw10_isp_bridge (
 (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS:M_AXIS:TO_ISP:FROM_ISP, ASSOCIATED_RESET aresetn" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 aclk CLK" *) input wire aclk,
 (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 aresetn RST" *) input wire aresetn,
 input wire enable,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDATA" *) input wire [31:0] s_axis_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TKEEP" *) input wire [3:0] s_axis_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TUSER" *) input wire [3:0] s_axis_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDEST" *) input wire [9:0] s_axis_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TLAST" *) input wire s_axis_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TVALID" *) input wire s_axis_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TREADY" *) output wire s_axis_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDATA" *) output wire [31:0] m_axis_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TKEEP" *) output wire [3:0] m_axis_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TUSER" *) output wire [3:0] m_axis_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDEST" *) output wire [9:0] m_axis_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TLAST" *) output wire m_axis_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TVALID" *) output wire m_axis_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TREADY" *) input wire m_axis_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TDATA" *) output wire [15:0] to_isp_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TKEEP" *) output wire [1:0] to_isp_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TSTRB" *) output wire [1:0] to_isp_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TUSER" *) output wire to_isp_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TLAST" *) output wire to_isp_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TID" *) output wire to_isp_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TDEST" *) output wire to_isp_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TVALID" *) output wire to_isp_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_ISP TREADY" *) input wire to_isp_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TDATA" *) input wire [15:0] from_isp_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TKEEP" *) input wire [1:0] from_isp_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TSTRB" *) input wire [1:0] from_isp_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TUSER" *) input wire from_isp_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TLAST" *) input wire from_isp_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TID" *) input wire from_isp_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TDEST" *) input wire from_isp_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TVALID" *) input wire from_isp_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TREADY" *) output wire from_isp_tready
);
 reg [95:0] bytes_fifo,bytes_next;
 reg [3:0] bytes_count,bytes_n;
 reg bytes_end,bytes_end_n,bytes_sof,bytes_sof_n;
 reg [39:0] pixels,pixels_n;
 reg [1:0] pixel_index,pixel_index_n;
 reg pixel_valid,pixel_valid_n,pixel_last,pixel_last_n,pixel_sof,pixel_sof_n;
 reg [9:0] destination;
 integer b;
 wire pixel_fire=to_isp_tvalid && to_isp_tready;
 wire group_free=!pixel_valid || (pixel_fire && pixel_index==3);
 assign s_axis_tready=!enable?m_axis_tready:bytes_count<=8 && !bytes_end;
 assign to_isp_tvalid=enable && pixel_valid;
 assign to_isp_tdata={6'b0,pixels[pixel_index*8+:8],pixels[32+pixel_index*2+:2]};
 assign to_isp_tkeep=2'b11;
 assign to_isp_tstrb=2'b11;
 assign to_isp_tuser=pixel_sof && pixel_index==0;
 assign to_isp_tlast=pixel_last && pixel_index==3;
 assign to_isp_tid=0;assign to_isp_tdest=0;
 always @* begin
   bytes_next=bytes_fifo;bytes_n=bytes_count;bytes_end_n=bytes_end;bytes_sof_n=bytes_sof;
   pixels_n=pixels;pixel_index_n=pixel_index;pixel_valid_n=pixel_valid;
   pixel_last_n=pixel_last;pixel_sof_n=pixel_sof;
   if(pixel_fire)begin
     if(pixel_index==3)begin
       pixel_valid_n=0;pixel_index_n=0;
       if(pixel_last)bytes_end_n=0;
     end else pixel_index_n=pixel_index+1;
   end
   if(group_free && bytes_count>=5)begin
     pixels_n=bytes_fifo[39:0];pixel_index_n=0;pixel_valid_n=1;
     pixel_last_n=bytes_end && bytes_count==5;pixel_sof_n=bytes_sof;
     bytes_sof_n=0;bytes_next=bytes_fifo>>40;bytes_n=bytes_count-5;
   end
   if(enable && s_axis_tvalid && s_axis_tready)begin
     if(s_axis_tuser[0])bytes_sof_n=1;
     for(b=0;b<4;b=b+1)if(s_axis_tkeep[b])begin
       bytes_next=bytes_next | ({88'b0,s_axis_tdata[b*8+:8]}<<(bytes_n*8));bytes_n=bytes_n+1;
     end
     if(s_axis_tlast)bytes_end_n=1;
   end
 end
 reg [127:0] packed_fifo,packed_next;
 reg [4:0] packed_count,packed_n;
 reg packed_end,packed_end_n,packed_sof,packed_sof_n;
 reg [39:0] group_bytes,group_next;
 reg [1:0] group_index,group_index_n;
 reg [39:0] completed_group;
 wire packed_valid=packed_count>=4 || (packed_end && packed_count!=0);
 wire packed_last=packed_end && packed_count<=4;
 assign from_isp_tready=enable && packed_count<=8 && !packed_end;
 assign m_axis_tvalid=!enable?s_axis_tvalid:packed_valid;
 assign m_axis_tdata=!enable?s_axis_tdata:packed_fifo[31:0];
 assign m_axis_tkeep=!enable?s_axis_tkeep:packed_count>=4?4'b1111:(4'b0001<<packed_count)-1;
 assign m_axis_tuser=!enable?s_axis_tuser:{3'b0,packed_sof};
 assign m_axis_tlast=!enable?s_axis_tlast:packed_last;
 assign m_axis_tdest=!enable?s_axis_tdest:destination;
 always @* begin
   packed_next=packed_fifo;packed_n=packed_count;packed_end_n=packed_end;packed_sof_n=packed_sof;
   group_next=group_bytes;group_index_n=group_index;completed_group=group_bytes;
   if(enable && packed_valid && m_axis_tready)begin
     packed_next=packed_fifo>>32;packed_n=packed_count>=4?packed_count-4:0;packed_sof_n=0;
     if(packed_last)begin packed_end_n=0;packed_next=0;packed_n=0;end
   end
   if(from_isp_tvalid && from_isp_tready)begin
     completed_group[group_index*8+:8]=from_isp_tdata[9:2];
     completed_group[32+group_index*2+:2]=from_isp_tdata[1:0];
     group_next=completed_group;
     if(from_isp_tuser)packed_sof_n=1;
     if(group_index==3)begin
       packed_next=packed_next | ({88'b0,completed_group}<<(packed_n*8));packed_n=packed_n+5;
       group_index_n=0;group_next=0;
     end else group_index_n=group_index+1;
     if(from_isp_tlast)packed_end_n=1;
   end
 end
 always @(posedge aclk)begin
   if(!aresetn || !enable)begin
     bytes_fifo<=0;bytes_count<=0;bytes_end<=0;bytes_sof<=0;
     pixels<=0;pixel_index<=0;pixel_valid<=0;pixel_last<=0;pixel_sof<=0;destination<=0;
     packed_fifo<=0;packed_count<=0;packed_end<=0;packed_sof<=0;group_bytes<=0;group_index<=0;
   end else begin
     bytes_fifo<=bytes_next;bytes_count<=bytes_n;bytes_end<=bytes_end_n;bytes_sof<=bytes_sof_n;
     pixels<=pixels_n;pixel_index<=pixel_index_n;pixel_valid<=pixel_valid_n;
     pixel_last<=pixel_last_n;pixel_sof<=pixel_sof_n;
     if(s_axis_tvalid && s_axis_tready && s_axis_tuser[0])destination<=s_axis_tdest;
     packed_fifo<=packed_next;packed_count<=packed_n;packed_end<=packed_end_n;packed_sof<=packed_sof_n;
     group_bytes<=group_next;group_index<=group_index_n;
   end
 end
endmodule
