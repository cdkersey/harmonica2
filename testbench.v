`timescale 1 ns / 1 ns

module top();
   wire[7:0] cout_prelatch;
   wire cstrobe;
   reg phi;
   reg [7:0] cout;
   
   h2 h20(phi, cout_prelatch, cstrobe);
   
   initial begin
        $dumpfile("dump.vcd");
        $dumpvars(2, top);
	phi = 0;
        cout = 0;
	#1000000 $finish;
     end

   always begin
	#5 phi = ~phi;
        if (cstrobe == 1) cout = cout_prelatch;
   end
   
endmodule // top
