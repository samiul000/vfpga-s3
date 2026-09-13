# 02 2:1 multiplexer

`clock` is unused by the logic; the testbench uses it to step the sim.

From this directory:

```bash
vfpga verify design.v --tb design.tb --wave --open
```

From the repo root instead:

```bash
build/vfpga verify examples/tutorial/02_mux/design.v --tb examples/tutorial/02_mux/design.tb --wave --open
```
