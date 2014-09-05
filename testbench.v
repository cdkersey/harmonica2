module top();
   wire[7:0] cout, cout_prelatch;
   wire cstrobe;
   reg phi;
   
   score h2(phi, cout_prelatch, cstrobe);
   
   initial begin
        $dumpfile("dump.vcd");
        $dumpvars(2, top);
	phi = 0;
        cout = 0;
	#100000 $finish;
     end

   always begin
	#1 phi = ~phi;
        if (cstrobe == 1) cout = cout_prelatch;
   end
   
endmodule // top
