# 08 Toggle FSM

Two states: `state` flips whenever `go` is high at a rising edge.
Watch `state` (next_state equals `state ^ go`) in the waveform.

From this directory:

```bash
vfpga verify design.v --tb design.tb --wave --open
```

From the repo root instead:

```bash
build/vfpga verify examples/tutorial/08_fsm/design.v --tb examples/tutorial/08_fsm/design.tb --wave --open
```
