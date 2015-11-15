# Halide_DMM
Halide demo program.

The original code for the DMM assignment is in the folder "ori_code".
In the folder "ori_code_halide", it has been ported to Halide (all halide code is after the comment "HALIDE PORTION").

It can be seen that describing the algorithm takes roughly an equal amount of code (maybe slightly less) in Halide.
It can also be seen that SCHEDULING the code takes only several lines. "ori_code_halide" schedules it as follows:

gaussx.compute_root();
gauss.compute_root();
edge.compute_root();
roots.realize(...);

this instructs halide to calculate these pipeline stages step by step, without fusing them.
