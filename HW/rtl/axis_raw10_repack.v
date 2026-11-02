`timescale 1ns/1ps
// CSI-2 RAW10 to PFNC Bayer10p (mode1) or little-endian Bayer10 (mode2).
// Other modes preserve every AXIS signal. Configuration changes only at idle.
module axis_raw10_repack (
 (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS:M_AXIS, ASSOCIATED_RESET aresetn" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 aclk CLK" *) input wire aclk,
 (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 aresetn RST" *) input wire aresetn,
 input wire [1:0] mode,
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
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TREADY" *) input wire m_axis_tready
);
 (* ASYNC_REG = "TRUE" *) reg [1:0] mode_meta,mode_sync;
 reg [1:0] active_mode;
 // At most 12 input bytes: ready permits <=8, then accepts <=4.
 // The output can hold 16 bytes after appending an eight-byte group.
 reg [95:0] raw_fifo,raw_next;
 reg [127:0] out_fifo,out_next;
 reg [3:0] raw_count,raw_n;
 reg [4:0] out_count,out_n;
 reg raw_end,out_end,sof,line_active,raw_end_n,out_end_n,sof_n,line_n;
 reg [9:0] dest,dest_n;
 reg [9:0] p0,p1,p2,p3;
 reg [63:0] group_data;
 integer i,group_bytes;
 wire bypass = active_mode==0 || active_mode==3;
 wire valid_out = out_count>=4 || (out_end && out_count!=0);
 wire last_out = out_end && out_count<=4;
 assign s_axis_tready = bypass ? m_axis_tready : raw_count<=8 && !raw_end;
 assign m_axis_tvalid = bypass ? s_axis_tvalid : valid_out;
 assign m_axis_tdata = bypass ? s_axis_tdata : out_fifo[31:0];
 assign m_axis_tkeep = bypass ? s_axis_tkeep : out_count>=4 ? 4'b1111 : (4'b0001<<out_count)-1;
 assign m_axis_tuser = bypass ? s_axis_tuser : {3'b000,sof};
 assign m_axis_tdest = bypass ? s_axis_tdest : dest;
 assign m_axis_tlast = bypass ? s_axis_tlast : last_out;
 always @* begin
   raw_next=raw_fifo;out_next=out_fifo;raw_n=raw_count;out_n=out_count;
   raw_end_n=raw_end;out_end_n=out_end;sof_n=sof;line_n=line_active;dest_n=dest;
   p0=0;p1=0;p2=0;p3=0;group_data=0;group_bytes=0;
   if (!bypass) begin
     if(valid_out && m_axis_tready) begin
       if(last_out) begin
         out_next=0;out_n=0;raw_end_n=0;out_end_n=0;line_n=0;
       end else begin out_next=out_fifo>>32;out_n=out_count-4;end
       sof_n=0;
     end
     if(raw_count>=5 && out_n<=8 && !out_end) begin
       p0={raw_fifo[7:0],raw_fifo[33:32]};
       p1={raw_fifo[15:8],raw_fifo[35:34]};
       p2={raw_fifo[23:16],raw_fifo[37:36]};
       p3={raw_fifo[31:24],raw_fifo[39:38]};
       if(active_mode==1) begin group_data={24'b0,p3,p2,p1,p0};group_bytes=5;end
       else begin group_data={6'b0,p3,6'b0,p2,6'b0,p1,6'b0,p0};group_bytes=8;end
       out_next=out_next | ({64'b0,group_data} << (out_n*8));out_n=out_n+group_bytes;
       raw_next=raw_fifo>>40;raw_n=raw_count-5;
       if(raw_end && raw_count==5)out_end_n=1;
     end
     if(s_axis_tvalid && s_axis_tready) begin
       if(!line_active) begin line_n=1;dest_n=s_axis_tdest;sof_n=s_axis_tuser[0];end
       // TKEEP may be sparse. Compact valid bytes in stream order.
       for(i=0;i<4;i=i+1) if(s_axis_tkeep[i]) begin
         raw_next=raw_next | ({88'b0,s_axis_tdata[i*8+:8]} << (raw_n*8));raw_n=raw_n+1;
       end
       if(s_axis_tlast)raw_end_n=1;
     end
   end
 end
 always @(posedge aclk) begin
   // Configuration remains synchronized while stream reset is asserted.
   // Software holds mode stable before the10us reset pulse. On release,
   // even a receiver with TVALID already high must use the selected packing.
   mode_meta<=mode;mode_sync<=mode_meta;
   if(!aresetn) begin
     active_mode<=mode_sync;
     raw_fifo<=0;out_fifo<=0;raw_count<=0;out_count<=0;
     raw_end<=0;out_end<=0;sof<=0;line_active<=0;dest<=0;
   end else begin
     if(!line_active && !s_axis_tvalid && !out_count && !raw_count)active_mode<=mode_sync;
     raw_fifo<=raw_next;out_fifo<=out_next;raw_count<=raw_n;out_count<=out_n;
     raw_end<=raw_end_n;out_end<=out_end_n;sof<=sof_n;line_active<=line_n;dest<=dest_n;
   end
 end
endmodule
