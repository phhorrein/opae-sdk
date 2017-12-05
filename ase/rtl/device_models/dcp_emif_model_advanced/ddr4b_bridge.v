// ddr4b_bridge.v

// Generated using ACDS version 17.0 290

`timescale 1 ps / 1 ps
module ddr4b_bridge (
		input  wire         clk,              //   clk.clk
		input  wire         m0_waitrequest,   //    m0.waitrequest
		input  wire [511:0] m0_readdata,      //      .readdata
		input  wire         m0_readdatavalid, //      .readdatavalid
		output wire [6:0]   m0_burstcount,    //      .burstcount
		output wire [511:0] m0_writedata,     //      .writedata
		output wire [25:0]  m0_address,       //      .address
		output wire         m0_write,         //      .write
		output wire         m0_read,          //      .read
		output wire [63:0]  m0_byteenable,    //      .byteenable
		output wire         m0_debugaccess,   //      .debugaccess
		input  wire         reset,            // reset.reset
		output wire         s0_waitrequest,   //    s0.waitrequest
		output wire [511:0] s0_readdata,      //      .readdata
		output wire         s0_readdatavalid, //      .readdatavalid
		input  wire [6:0]   s0_burstcount,    //      .burstcount
		input  wire [511:0] s0_writedata,     //      .writedata
		input  wire [25:0]  s0_address,       //      .address
		input  wire         s0_write,         //      .write
		input  wire         s0_read,          //      .read
		input  wire [63:0]  s0_byteenable,    //      .byteenable
		input  wire         s0_debugaccess    //      .debugaccess
	);

	altera_avalon_mm_bridge #(
		.DATA_WIDTH        (512),
		.SYMBOL_WIDTH      (8),
		.HDL_ADDR_WIDTH    (26),
		.BURSTCOUNT_WIDTH  (7),
		.PIPELINE_COMMAND  (1),
		.PIPELINE_RESPONSE (1)
	) ddr4b_bridge (
		.clk              (clk),              //   clk.clk
		.reset            (reset),            // reset.reset
		.s0_waitrequest   (s0_waitrequest),   //    s0.waitrequest
		.s0_readdata      (s0_readdata),      //      .readdata
		.s0_readdatavalid (s0_readdatavalid), //      .readdatavalid
		.s0_burstcount    (s0_burstcount),    //      .burstcount
		.s0_writedata     (s0_writedata),     //      .writedata
		.s0_address       (s0_address),       //      .address
		.s0_write         (s0_write),         //      .write
		.s0_read          (s0_read),          //      .read
		.s0_byteenable    (s0_byteenable),    //      .byteenable
		.s0_debugaccess   (s0_debugaccess),   //      .debugaccess
		.m0_waitrequest   (m0_waitrequest),   //    m0.waitrequest
		.m0_readdata      (m0_readdata),      //      .readdata
		.m0_readdatavalid (m0_readdatavalid), //      .readdatavalid
		.m0_burstcount    (m0_burstcount),    //      .burstcount
		.m0_writedata     (m0_writedata),     //      .writedata
		.m0_address       (m0_address),       //      .address
		.m0_write         (m0_write),         //      .write
		.m0_read          (m0_read),          //      .read
		.m0_byteenable    (m0_byteenable),    //      .byteenable
		.m0_debugaccess   (m0_debugaccess),   //      .debugaccess
		.s0_response      (),                 // (terminated)
		.m0_response      (2'b00)             // (terminated)
	);

endmodule