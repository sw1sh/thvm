# linear-train

Smallest end-to-end training example: a single linear layer
(`{3, 4} W + {3} b`) classifying 4-feature inputs into 3
classes via softmax + cross-entropy, trained with Adam.

The point isn't accuracy -- it's a tiny, readable training
step whose **memory plan** fits on screen.  The LeNet probe
produces 455 kernels and a Gantt chart you have to squint at;
this one produces ~93 kernels (forward + two TGrad realizes)
with bars you can actually inspect, and the slot-reuse
headroom is 58-61% (CPU / Metal) -- the largest signal we
have that lifetime-aware scheduling has room to win.

## Run

```bash
wolframscript -f wl/Examples/linear-train/train.wls
wolframscript -f wl/Examples/linear-train/memory-probe.wls
THVM_BACKEND=metal wolframscript -f wl/Examples/linear-train/memory-probe.wls
```

`train.wls` runs 20 Adam steps on synthetic data (3 fixed input
samples) and prints the loss curve.

`memory-probe.wls` runs one forward + backward + Adam update,
prints kernel / tensor / buffer deltas at each phase, then
exports `memory-plan-{cpu,metal}.png` Gantt charts for the
captured `TMemoryPlan`.

## What you should see

- Loss bouncing on the per-sample cycle (3 samples in
  rotation) but the cycle envelope falling: starts ~1.0-1.6,
  ends ~0.2-0.4 after 20 steps.
- A Gantt chart whose y-axis is buffer slots packed by
  topological depth.  The longest-lived bars are the
  weight upload (W: depth 0..18) and the autograd cotangent
  chain (depth 1..25); the matmul output, softmax exp, and
  cross-entropy intermediates appear as short 1-3 depth bars.

The compact size makes this the right example to iterate on
when working on memory-planning fixes -- visual inspection is
practical here in a way it isn't for LeNet.
