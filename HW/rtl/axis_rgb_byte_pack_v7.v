`timescale 1ns/1ps
// Pack RGB or CSC output words into the byte-oriented video writer without row padding.
// Configuration changes require the receiver to be stopped and this path reset.
module axis_rgb_byte_pack_v7 (
 input wire aclk, input wire aresetn, input wire enable, input wire bgr,
 input wire [23:0] s_data, input wire [2:0] s_keep, input wire s_sof, input wire s_last,
 input wire s_valid, output wire s_ready,
 output wire [31:0] m_data, output wire [3:0] m_keep,
 output wire m_sof, output wire m_last, output wire m_valid,
 input wire m_ready
);
 reg [63:0] bytes_fifo, bytes_next;
 reg [3:0] count, count_next;
 reg line_end, end_next, sof, sof_next;
 wire emit = count >= 4 || (line_end && count != 0);
 wire last_word = line_end && count <= 4;
 wire two_bytes = s_keep == 3'b011;
 wire [23:0] pixel = two_bytes ? {8'b0,s_data[15:0]} : bgr ? {s_data[7:0],s_data[15:8],s_data[23:16]} : s_data;
 // A simultaneous dequeue makes room for the next RGB pixel.
 assign s_ready = enable && !line_end && (count <= 4 || (emit && m_ready));
 assign m_valid = enable && emit;
 assign m_data = bytes_fifo[31:0];
 assign m_keep = count >= 4 ? 4'b1111 : (4'b0001 << count)-1;
 assign m_sof = sof;
 assign m_last = last_word;
 always @* begin
   bytes_next=bytes_fifo; count_next=count; end_next=line_end; sof_next=sof;
   if(m_valid && m_ready) begin
     bytes_next=bytes_fifo >> 32; count_next=count>=4 ? count-4 : 0; sof_next=0;
     if(last_word) begin bytes_next=0; count_next=0; end_next=0; end
   end
   if(s_valid && s_ready) begin
     bytes_next=bytes_next | ({40'b0,pixel} << (count_next*8));
     count_next=count_next+(two_bytes ? 2 : 3);
     if(s_sof) sof_next=1;
     if(s_last) end_next=1;
   end
 end
 always @(posedge aclk) begin
   if(!aresetn || !enable) begin bytes_fifo<=0; count<=0; line_end<=0; sof<=0; end
   else begin bytes_fifo<=bytes_next; count<=count_next; line_end<=end_next; sof<=sof_next; end
 end
endmodule
