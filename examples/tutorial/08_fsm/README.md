# 08 Toggle FSM

Two states: `state` flips whenever `go` is high at a rising edge.
Watch `state` (next_state equals `state ^ go`) in the waveform.

```bash
vfpga verify design.vhdl --tb design.tb --wave --open
```
