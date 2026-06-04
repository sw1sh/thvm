# mnist-step

Warm per-step (forward + backward + SGD) wall time and memory for a small
LeNet-style head, comparing the THVMLink tensor surface against tinygrad on
the CPU. Backs the "Time and memory against tinygrad" table in the
[Train tutorial](../../wl/THVMLink/docs/Tutorials/Train.md).

Net (identical both sides): `Conv2d[1 -> 8, 3x3]` -> ReLU -> 2x2 max-pool ->
`Linear[1352 -> 10]`, batch 64, softmax cross-entropy, eager (no JIT).

```
              wolframscript -file thvm_step.wls   # thvm CPU
DEV=metal     wolframscript -file thvm_step.wls   # thvm Metal GPU
              python3 tinygrad_step.py            # tinygrad CPU (DEV=CPU forced)
DEV=METAL     python3 tinygrad_step.py            # tinygrad Metal GPU
```

Representative (Apple-silicon, eager warm ms/step):

| | CPU | Metal GPU |
|---|---|---|
| thvm | ~32 ms | ~29 ms |
| tinygrad | ~15 ms | ~17 ms |

thvm is ~1.7-2x slower than tinygrad for this small eager step on both
backends. Metal barely beats CPU here: the conv8 head is tiny, so per-kernel
GPU dispatch overhead roughly equals the compute (it's dispatch-bound, not
flop-bound). The gap and the GPU win both grow on the larger fused
`beautiful_mnist` pipeline with the JIT and the faithful realize point (see
the cross-backend perf notes).

Memory (training working set): thvm ~60 MB delta (WL-heap peak ~166 MB),
tinygrad ~117-260 MB whole process. Absolute process RSS is not comparable -
thvm runs inside a WolframKernel whose runtime baseline (~2.7 GB) dwarfs the
training's own tens of megabytes; the per-step working set is comparable.

**Metal safety:** this conv8 step schedules to only ~40 kernels, so it runs
clean on the GPU. A *larger* training graph (e.g. the 2-conv beautiful_mnist
net) over-fuses the backward into 1300+ kernels and can orphan the Metal GPU
on dispatch (reboot to clear) - on both thvm and tinygrad (tinygrad's
watchdog-busting reduce). Prescreen `TKernelCount[]` on `DEV=cpu` first; keep
it well under a few hundred before dispatching a train step on Metal.
`tinygrad_step.py` defaults to `DEV=CPU` for that reason; `DEV=METAL` is safe
for this small net. Point it at a non-default tinygrad checkout with
`TINYGRAD=/path/to/tinygrad`.
