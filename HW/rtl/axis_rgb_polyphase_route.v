`timescale 1ns/1ps
// Select the Xilinx RGB polyphase path only while acquisition is stopped.
module axis_rgb_polyphase_route (
 (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXIS:M_AXIS:TO_SCALER:FROM_SCALER, ASSOCIATED_RESET aresetn" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 aclk CLK" *) input wire aclk,
 (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
 (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 aresetn RST" *) input wire aresetn,
 input wire enable,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDATA" *) input wire [23:0] s_axis_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TKEEP" *) input wire [2:0] s_axis_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TSTRB" *) input wire [2:0] s_axis_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TUSER" *) input wire s_axis_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TLAST" *) input wire s_axis_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TID" *) input wire s_axis_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TDEST" *) input wire s_axis_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TVALID" *) input wire s_axis_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 S_AXIS TREADY" *) output wire s_axis_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDATA" *) output wire [23:0] m_axis_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TKEEP" *) output wire [2:0] m_axis_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TSTRB" *) output wire [2:0] m_axis_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TUSER" *) output wire m_axis_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TLAST" *) output wire m_axis_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TID" *) output wire m_axis_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TDEST" *) output wire m_axis_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TVALID" *) output wire m_axis_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 M_AXIS TREADY" *) input wire m_axis_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TDATA" *) output wire [23:0] to_scaler_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TKEEP" *) output wire [2:0] to_scaler_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TSTRB" *) output wire [2:0] to_scaler_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TUSER" *) output wire to_scaler_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TLAST" *) output wire to_scaler_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TID" *) output wire to_scaler_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TDEST" *) output wire to_scaler_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TVALID" *) output wire to_scaler_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 TO_SCALER TREADY" *) input wire to_scaler_tready,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TDATA" *) input wire [23:0] from_scaler_tdata,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TKEEP" *) input wire [2:0] from_scaler_tkeep,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TSTRB" *) input wire [2:0] from_scaler_tstrb,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TUSER" *) input wire from_scaler_tuser,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TLAST" *) input wire from_scaler_tlast,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TID" *) input wire from_scaler_tid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TDEST" *) input wire from_scaler_tdest,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TVALID" *) input wire from_scaler_tvalid,
 (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 FROM_SCALER TREADY" *) output wire from_scaler_tready
);
 assign to_scaler_tdata=s_axis_tdata;
 assign m_axis_tdata=enable?from_scaler_tdata:s_axis_tdata;
 assign to_scaler_tkeep=s_axis_tkeep;
 assign m_axis_tkeep=enable?from_scaler_tkeep:s_axis_tkeep;
 assign to_scaler_tstrb=s_axis_tstrb;
 // Xilinx AXI video does not define TSTRB (VPSS drives zero or X).
 // Every kept RGB byte is data; preserve TKEEP so malformed pixels still fail.
 assign m_axis_tstrb=enable?from_scaler_tkeep:s_axis_tstrb;
 assign to_scaler_tuser=s_axis_tuser;
 assign m_axis_tuser=enable?from_scaler_tuser:s_axis_tuser;
 assign to_scaler_tlast=s_axis_tlast;
 assign m_axis_tlast=enable?from_scaler_tlast:s_axis_tlast;
 assign to_scaler_tid=s_axis_tid;
 assign m_axis_tid=enable?from_scaler_tid:s_axis_tid;
 assign to_scaler_tdest=s_axis_tdest;
 assign m_axis_tdest=enable?from_scaler_tdest:s_axis_tdest;
 assign s_axis_tready=aresetn && (enable?to_scaler_tready:m_axis_tready);
 assign to_scaler_tvalid=aresetn && enable && s_axis_tvalid;
 assign from_scaler_tready=aresetn && enable && m_axis_tready;
 assign m_axis_tvalid=aresetn && (enable?from_scaler_tvalid:s_axis_tvalid);
endmodule
