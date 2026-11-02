`timescale 1ns/1ps
// Byte-stream to four-pixel Y8 writer adapter; configuration changes at idle.
// Firmware supplies HActive=ceil(true_row_bytes/4)*4 and a 32-byte stride.
// Every writer pixel is defined: absent source lanes become zero padding.
module axis_byte_to_writer_ppc4 (
 (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS:M_AXIS, ASSOCIATED_RESET aresetn" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 aclk CLK" *) input wire aclk,
 (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 aresetn RST" *) input wire aresetn,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDATA" *) input wire [31:0] s_axis_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TKEEP" *) input wire [3:0] s_axis_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TUSER" *) input wire [3:0] s_axis_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDEST" *) input wire [9:0] s_axis_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TLAST" *) input wire  s_axis_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TVALID" *) input wire  s_axis_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TREADY" *) output wire  s_axis_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDATA" *) output wire [95:0] m_axis_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TKEEP" *) output wire [11:0] m_axis_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TSTRB" *) output wire [11:0] m_axis_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TUSER" *) output wire  m_axis_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TID" *) output wire  m_axis_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDEST" *) output wire  m_axis_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TLAST" *) output wire  m_axis_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TVALID" *) output wire  m_axis_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TREADY" *) input wire  m_axis_tready
);
 genvar lane;
 generate for(lane=0;lane<4;lane=lane+1) begin: lanes
  assign m_axis_tdata[lane*24+:24]={16'b0,(s_axis_tkeep[lane]?s_axis_tdata[lane*8+:8]:8'b0)};
 end endgenerate
 assign m_axis_tkeep=12'hfff;
 assign m_axis_tstrb=12'hfff;
 assign m_axis_tuser=s_axis_tuser[0];
 assign m_axis_tid=1'b0;
 assign m_axis_tdest=s_axis_tdest[0];
 assign m_axis_tlast=s_axis_tlast;
 assign m_axis_tvalid=aresetn && s_axis_tvalid;
 assign s_axis_tready=aresetn && m_axis_tready;
endmodule
