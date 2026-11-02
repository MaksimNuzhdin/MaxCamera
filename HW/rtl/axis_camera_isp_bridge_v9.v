`timescale 1ns/1ps
// RAW correction is shared by RAW output and the downstream color pipeline.
module axis_camera_isp_bridge_v9 (
 (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS:M_AXIS:TO_ISP:FROM_ISP:TO_RGB:FROM_RGB, ASSOCIATED_RESET aresetn" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 aclk CLK" *) input wire aclk,
 (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 aresetn RST" *) input wire aresetn,
 input wire [1:0] mode,
 input wire bgr,
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
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_ISP TREADY" *) output wire from_isp_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TDATA" *) output wire [15:0] to_rgb_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TKEEP" *) output wire [1:0] to_rgb_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TSTRB" *) output wire [1:0] to_rgb_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TUSER" *) output wire to_rgb_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TLAST" *) output wire to_rgb_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TID" *) output wire to_rgb_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TDEST" *) output wire to_rgb_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TVALID" *) output wire to_rgb_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_RGB TREADY" *) input wire to_rgb_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TDATA" *) input wire [23:0] from_rgb_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TKEEP" *) input wire [2:0] from_rgb_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TSTRB" *) input wire [2:0] from_rgb_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TUSER" *) input wire from_rgb_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TLAST" *) input wire from_rgb_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TID" *) input wire from_rgb_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TDEST" *) input wire from_rgb_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TVALID" *) input wire from_rgb_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_RGB TREADY" *) output wire from_rgb_tready
);

 wire active = mode == 1 || mode == 2;
 wire color = mode == 2;
 wire raw_corrected_ready;
 wire [15:0] unpack_data;
 wire [1:0] unpack_keep, unpack_strb;
 wire unpack_user, unpack_last, unpack_id, unpack_dest, unpack_valid, unpack_ready;
 wire [31:0] raw_data, rgb_data;
 wire [3:0] raw_keep, raw_user, rgb_keep;
 wire [9:0] raw_dest;
 wire raw_last, raw_valid, raw_ready, rgb_sof, rgb_last, rgb_valid;
 reg [9:0] destination;
 always @(posedge aclk) begin
   if(!aresetn || !active) destination<=0;
   else if(s_axis_tvalid && s_axis_tready && s_axis_tuser[0]) destination<=s_axis_tdest;
 end
 assign unpack_ready=to_isp_tready;
 assign from_isp_tready=color?to_rgb_tready:raw_corrected_ready;
 assign to_isp_tdata=unpack_data;
 assign to_rgb_tdata=from_isp_tdata;
 assign to_isp_tkeep=unpack_keep;
 assign to_rgb_tkeep=from_isp_tkeep;
 assign to_isp_tstrb=unpack_strb;
 assign to_rgb_tstrb=from_isp_tstrb;
 assign to_isp_tuser=unpack_user;
 assign to_rgb_tuser=from_isp_tuser;
 assign to_isp_tlast=unpack_last;
 assign to_rgb_tlast=from_isp_tlast;
 assign to_isp_tid=unpack_id;
 assign to_rgb_tid=from_isp_tid;
 assign to_isp_tdest=unpack_dest;
 assign to_rgb_tdest=from_isp_tdest;
 assign to_isp_tvalid=unpack_valid;
 assign to_rgb_tvalid=from_isp_tvalid && color;
 assign m_axis_tdata=color?rgb_data:raw_data;
 assign m_axis_tkeep=color?rgb_keep:raw_keep;
 assign m_axis_tuser=color?{3'b0,rgb_sof}:raw_user;
 assign m_axis_tdest=color?destination:raw_dest;
 assign m_axis_tlast=color?rgb_last:raw_last;
 assign m_axis_tvalid=color?rgb_valid:raw_valid;
 assign raw_ready=!color && m_axis_tready;
 axis_raw10_isp_bridge raw_adapter (
 .aclk(aclk),.aresetn(aresetn),.enable(active),
.s_axis_tdata(s_axis_tdata),
 .s_axis_tkeep(s_axis_tkeep),
 .s_axis_tuser(s_axis_tuser),
 .s_axis_tdest(s_axis_tdest),
 .s_axis_tlast(s_axis_tlast),
 .s_axis_tvalid(s_axis_tvalid),
 .s_axis_tready(s_axis_tready),
 .m_axis_tdata(raw_data),
 .m_axis_tkeep(raw_keep),
 .m_axis_tuser(raw_user),
 .m_axis_tdest(raw_dest),
 .m_axis_tlast(raw_last),
 .m_axis_tvalid(raw_valid),
 .m_axis_tready(raw_ready),
 .to_isp_tdata(unpack_data),
 .to_isp_tkeep(unpack_keep),
 .to_isp_tstrb(unpack_strb),
 .to_isp_tuser(unpack_user),
 .to_isp_tlast(unpack_last),
 .to_isp_tid(unpack_id),
 .to_isp_tdest(unpack_dest),
 .to_isp_tvalid(unpack_valid),
 .to_isp_tready(unpack_ready),
 .from_isp_tdata(from_isp_tdata),
 .from_isp_tkeep(from_isp_tkeep),
 .from_isp_tstrb(from_isp_tstrb),
 .from_isp_tuser(from_isp_tuser),
 .from_isp_tlast(from_isp_tlast),
 .from_isp_tid(from_isp_tid),
 .from_isp_tdest(from_isp_tdest),
 .from_isp_tvalid(from_isp_tvalid && !color),
 .from_isp_tready(raw_corrected_ready)
 );
 axis_rgb_byte_pack_v7 rgb_adapter (
 .aclk(aclk),.aresetn(aresetn),.enable(color),.bgr(bgr),
 .s_data(from_rgb_tdata),.s_keep(from_rgb_tkeep),.s_sof(from_rgb_tuser),.s_last(from_rgb_tlast),
 .s_valid(from_rgb_tvalid),.s_ready(from_rgb_tready),
 .m_data(rgb_data),.m_keep(rgb_keep),.m_sof(rgb_sof),.m_last(rgb_last),
 .m_valid(rgb_valid),.m_ready(m_axis_tready)
 );
endmodule
