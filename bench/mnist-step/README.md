# mnist-step

Warm per-step (forward + backward + SGD) wall time and memory for a small
LeNet-style head, comparing the THVMLink tensor surface against tinygrad on
the CPU. Backs the "Time and memory against tinygrad" table in the
[Train tutorial](../../wl/THVMLink/docs/Tutorials/Train.md).

Net (identical both sides): `Conv2d[1 -> 8, 3x3]` -> ReLU -> 2x2 max-pool ->
`Linear[1352 -> 10]`, batch 64, softmax cross-entropy, eager (no JIT).

```
wolframscript -file thvm_step.wls     # thvm: ms/step + RSS delta + WL heap
python3 tinygrad_step.py              # tinygrad: ms/step + peak RSS (DEV=CPU forced)
```

Representative (Apple-silicon CPU, eager):

| | per-step (warm) | memory |
|---|---|---|
| thvm | ~32 ms | ~60 MB training delta (WL-heap peak ~166 MB) |
| tinygrad | ~15 ms | ~117 MB whole process |

thvm is ~2x slower for this small eager step. Absolute process RSS is not
comparable: thvm runs inside a WolframKernel whose runtime baseline (~2.7 GB)
dwarfs the training's own tens of megabytes, while tinygrad's lean Python
process peaks near 120 MB. The per-step working set is comparable either way.
The gap closes on the larger fused `beautiful_mnist` pipeline with the JIT and
the faithful realize point (see the cross-backend perf notes).

`tinygrad_step.py` forces `DEV=CPU`: tinygrad on Metal can over-fuse the
backward into one watchdog-busting kernel and orphan the GPU on this box.
Point it at a tinygrad checkout other than `../../../tinygrad` with
`TINYGRAD=/path/to/tinygrad`.
