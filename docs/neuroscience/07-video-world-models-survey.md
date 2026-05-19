# 07 - Video and game world models: a survey of the SANA-WM neighbourhood

This page is a standalone technique survey, written May 2026, of the
generative video and game world-model field clustered around **SANA-WM**
(arXiv:2605.15178). [Page 3](03-jepa-and-world-models.md) covered the
Joint Embedding Predictive Architecture (JEPA) line and made the
argument that *generating the input is the wrong way to build a brain*.
Almost every system below is in the camp that argument rejects: they
generate pixels. They are surveyed here anyway, because their
*engineering* -- compression, long-context backbones, drift-free
rollout, action and camera control, long-horizon memory -- is
paradigm-independent, and the `brain/` experiment arc needs exactly
those mechanisms as it builds its own (latent, abstract) learned world
model. Read this as the engineering companion to page 3's paradigm
argument.

A caveat that applies to the whole page: this is the fastest-moving
area in machine learning right now. Many entries are 2025-2026
preprints; arXiv identifiers in the 2601-2605 range are weeks to months
old and some have not been independently verified. Treat the
*techniques* as the durable content and the specific numbers as a
snapshot.

## The shape of a modern video world model

A contemporary video world model is a stack of four layers, and every
system below is a particular set of choices at each layer:

1. **Compression** -- a tokenizer or autoencoder that maps raw video to
   a short latent sequence. Fewer tokens is the master lever for
   everything downstream.
2. **Backbone** -- a sequence model over the latent tokens. The cost of
   modelling a minute of video is dominated here.
3. **Rollout** -- the scheme for generating the sequence
   autoregressively over horizons far longer than any training clip,
   without error accumulation.
4. **Interface** -- how the model is conditioned on actions and camera,
   and how it keeps a scene consistent when the viewer looks away and
   back.

Sections 1-3 walk the layers; sections 4-6 cover the model classes that
result (interactive game engines, embodied world models); section 7
places SANA-WM against its direct competitors; the last section
extracts what the `brain/` arc should take.

## 1. Compression and the SANA efficiency lineage

SANA is NVIDIA's line of efficient diffusion models. Its through-line
is a single idea: compress the latent aggressively, then a small model
suffices.

**Deep Compression Autoencoder (DC-AE)** (arXiv:2410.10733, 2024)
pushes the spatial compression ratio of the latent from the Stable
Diffusion variational autoencoder's 8x up to 32x, 64x, even 128x. Two
tricks make the high ratios trainable: *residual autoencoding*, where
non-parametric space-to-channel shortcuts let the network learn
residuals over a structure-preserving baseline rather than reconstruct
from scratch; and a *decoupled high-resolution adaptation* training
schedule. At 64x compression on 512x512 images it gives a ~19x
inference speed-up at competitive reconstruction quality. Aggressive
spatial compression is the master lever -- fewer latent tokens makes
every later layer cheaper.

**SANA** (arXiv:2410.10629, 2024; International Conference on Learning
Representations 2025) is the efficient text-to-image model built on
DC-AE. Four choices stack: DC-AE at 32x (a 1024x1024 image becomes a
32x32 latent); rectified-linear-unit *linear attention* replacing the
quadratic softmax in the diffusion transformer (DiT); a *Mix
feed-forward network* that injects a 3x3 depthwise convolution to
restore the local inductive bias linear attention loses, which lets
SANA drop positional embeddings entirely ("NoPE"); and a small
decoder-only language model (Gemma) as the text encoder. SANA-0.6B
generates a 1024x1024 image in 0.9s, reported as ~39x faster than
FLUX-dev at comparable benchmark quality.

**SANA 1.5** (arXiv:2501.18427, 2025) adds a depth-growth recipe -- a
20-layer model is grown into a 60-layer one by freezing the pretrained
layers and inserting identity-initialised new layers, reaching the same
quality with ~60% fewer steps -- and inference-time scaling by
ranking many samples with a vision-language model. **SANA-Sprint**
(arXiv:2503.09641, 2025) is the few-step distillation: a training-free
TrigFlow reparameterisation plus continuous-time consistency
distillation collapses 20 denoising steps to 1-4, generating an image
in ~0.2s.

**SANA-Video** (arXiv:2509.24695, 2025) extends the linear DiT to
video as a *block linear diffusion transformer*. Video is chunked into
blocks; within a block, linear attention accumulates a constant-size
D x D state matrix, and at block boundaries the state is passed
forward, giving the next block global context at zero extra memory.
This kills the O(N) key-value cache growth that otherwise makes
minute-length video infeasible. Paired with a 128x spatio-temporal
compression autoencoder, a 2B model trains in ~12 days on 64 H100 GPUs.

**SVDQuant** (arXiv:2411.05007, 2024; ICLR 2025) and the **NVFP4**
4-bit format are the deployment layer: SVDQuant absorbs activation
outliers into a low-rank high-precision branch so a diffusion
transformer can run at 4-bit weights and activations, with the
**Nunchaku** engine fusing the low-rank branch into the 4-bit kernel.
This is what lets a 2-3B video model run on a single consumer graphics
card.

The lesson of the lineage: a sub-3B model is competitive with much
larger ones if the token count is compressed hard enough and the
backbone is sub-quadratic. SANA-WM (section 7) composes the whole
stack.

## 2. The backbone: efficient long-context sequence models

Full softmax attention over a minute of video is quadratic in sequence
length and memory-bound. The backbone layer is where that cost is
attacked, and it is the same body of work that produced the
linear-attention language models.

**Linear and gated linear attention.** Linear attention
(Katharopoulos et al., 2020) replaces the softmax with a kernel inner
product, so attention becomes a recurrent matrix-valued state `S` of
fixed size `d x d` updated by `S <- S + k v^T`. This collapses
quadratic memory to a constant state but loses selectivity. **Gated
Linear Attention** (arXiv:2312.06635, International Conference on
Machine Learning 2024) adds data-dependent gating, `S <- G . S + k
v^T`, giving the model adaptive forgetting, and ships a
hardware-efficient chunkwise kernel.

**State-space models.** **Mamba** (arXiv:2312.00752, 2023) is a
selective state-space model -- a linear recurrence `h_t = A_t h_{t-1}
+ B_t x_t` whose transition matrices depend on the input, made fast by
a hardware-parallel scan. **Mamba-2** (arXiv:2405.21060, 2024)
establishes *state-space duality*: with a scalar transition the
state-space model is equivalent to masked linear attention, unifying
the two families.

**Gated DeltaNet (GDN)** (arXiv:2412.06464, ICLR 2025) is the backbone
SANA-WM actually uses. It combines the Hopfield *delta rule* with
gating: the state update first erases the old value bound to a key
(a rank-1 subtraction) then writes the new value, while a separate
gate controls bulk decay. The two mechanisms are complementary --
gating for switching context, the delta rule for precise key-value
replacement -- and GDN beats both Mamba-2 and gated linear attention
on recall-intensive benchmarks. Its in-context retrieval is good
enough that the recurrent state carries genuine history rather than a
lossy summary.

**RWKV-7** (arXiv:2503.14456, 2025) is a pure linear-recurrent
language model generalising the delta rule with vector-valued gating;
it demonstrates a ~3B linear-recurrent model matching transformers on
language tasks at constant memory per token.

**Hybrids.** Pure-linear models fail needle-in-a-haystack retrieval,
so the production pattern is *mostly-linear with occasional softmax*.
**Jamba** (arXiv:2403.19887, 2024) interleaves Mamba and attention
blocks; IBM **Granite 4.0** uses a 9:1 Mamba-2-to-attention ratio;
**Minimax-01** uses 7 linear-attention layers per softmax block for a
1M-token training context. SANA-WM follows the same template exactly:
15 GDN blocks and 5 softmax blocks across 20 layers. The hybrid ratio
is the dial between constant-memory history (the linear blocks) and
exact recall (the softmax blocks).

**Test-time-training (TTT) layers** (arXiv:2407.04620, 2024) take the
idea furthest: the sequence-mixer's hidden state is itself a small
neural network updated by online self-supervised gradient steps as the
sequence streams in -- an adaptively compressed memory of unbounded
context.

For a world model, the linear or state-space block is what carries
multi-minute temporal context in bounded memory; the occasional
softmax block handles fine-grained local consistency.

## 3. The rollout: autoregressive long-video generation and drift

A model trained on short clips must generate horizons far longer.
Autoregressive rollout does this but suffers **drift**: the model is
trained conditioned on clean ground-truth frames, then at test time
conditions on its own imperfect outputs, and small errors compound.
Drift is the central failure mode, and most of this section is its
mitigations.

**Diffusion Forcing** (arXiv:2407.01392, 2024) gives each token in the
sequence its *own* independently sampled noise level. During rollout,
past tokens are conditioned on at low noise and future tokens denoised
from high noise. Because the model is trained to denoise from a noisy
prefix rather than a perfect one, it is robust to its own errors, and
the causal structure lets it generate past the training horizon. This
is the foundational drift-resistance trick.

**Self-Forcing** (arXiv:2506.08009, 2025) closes the train-test gap
directly: during *training* the model runs a full autoregressive
rollout with a key-value cache, conditions each frame on its own
generated outputs, and a video-level loss supervises the whole
rollout (stochastic gradient truncation keeps it tractable). It is the
strongest known fix for exposure bias in video diffusion.
**CausVid** (arXiv:2412.07772, 2024) instead distils a clean
bidirectional teacher into a fast causal student.

**Magi-1** (arXiv:2505.13211, 2025) is chunk-wise autoregression at
24B parameters with a monotonically increasing per-chunk noise
schedule and constant inference cost (processed chunks are dropped
from the cache). **SkyReels-V2** (arXiv:2504.13074, 2025) is the
open-source infinite-length film generator built on Diffusion
Forcing. **LongLive** (arXiv:2509.22622, 2025) does frame-level
autoregression at ~20 frames per second for up to 240s, with a
*key-value recache* that refreshes the cache when the text prompt
changes -- the closest existing mechanism to redirecting a long
rollout toward a new goal mid-stream.

**FramePack** (arXiv:2504.12626, 2025) packs history into a fixed
token budget weighted by frame importance, and adds three explicit
anti-drift tricks: anchoring generation with a visible endpoint frame,
generating later frames before nearby ones so errors do not compound
sequentially, and a *discrete* history representation to stop gradual
floating-point drift. **Rolling Forcing** (arXiv:2509.25161, 2025)
applies bidirectional attention within a sliding window so local
errors are corrected before the window slides on.

The mitigation hierarchy, weakest to strongest: noise-augment the
history at training time (Diffusion Forcing); distil from a clean
teacher (CausVid); train on self-generated context (Self-Forcing);
bound the context window so drifted frames age out (Magi-1, LongLive);
anchor with endpoints and quantise history (FramePack).

## 4. Interactive and playable world models

These systems are the headline application: a trained network used as
a game engine. The player's action goes in, the next frame comes out,
and there is no explicit simulator.

**GameNGen** (arXiv:2408.14837, 2024) was the first -- a fine-tuned
Stable Diffusion playing DOOM at 20 frames per second, conditioned on
the last 64 frames and actions. It is sharp but forgets anything older
than its ~3s context window.

**Genie** (arXiv:2402.15391, ICML 2024) is the most conceptually
important: it learns a *latent action space* unsupervised from 30,000
hours of internet platformer video, with no action labels -- a latent
action model discovers a discrete action codebook purely from
frame-to-frame change. **Genie 2** (DeepMind, December 2024) scales to
3D worlds from a single image with ~10-60s consistency; **Genie 3**
(DeepMind, August 2025) reaches real-time 720p at 24 frames per second
with multi-minute coherence and mid-session "promptable world events".

**Oasis** (Decart and Etched, 2024) is the open-source real-time
playable Minecraft diffusion model. **DIAMOND** (arXiv:2405.12399,
NeurIPS 2024) is the rigorous reinforcement-learning result: an agent
trained entirely inside a diffusion world model reaches a new best on
the Atari 100k benchmark, and the paper shows *why diffusion* -- the
discrete latents of earlier world models (DreamerV3, IRIS) discard
visual detail that reinforcement learning needs.

The **Matrix-Game** lineage (1.0 arXiv:2506.18701; 2.0
arXiv:2508.13009; 3.0 arXiv:2604.08995) iterates toward real-time
720p streaming with camera-aware memory. **GameGen-X**
(arXiv:2411.00769, ICLR 2025) generates open-domain game video across
150+ titles and adds control through an instruction-tuned adapter on a
frozen backbone. **Hunyuan-GameCraft** (arXiv:2506.17201, 2025) adds
hybrid history conditioning and a language interface. Microsoft's
**Muse / WHAM** (*Nature*, 2025) jointly generates *both* frames and
controller actions, so the model encodes a behaviour policy implicit
in its training distribution -- it can predict frames given actions,
actions given frames, or dream both.

## 5. Control, memory, and geometric grounding

A world model is only useful if it can be steered and if it stays
consistent. Two problem families.

**Camera and action control.** **CameraCtrl** (arXiv:2404.02101,
2024) established the standard vocabulary: encode each pixel's camera
ray as a six-dimensional Plücker embedding and feed it through a
plug-in adapter to a frozen video model. **MotionCtrl**
(arXiv:2312.03641, 2023) separates camera motion from object motion
into independent adapters. **CamCo** (arXiv:2406.02509, 2024) adds an
epipolar-attention constraint for 3D consistency; **ViewCrafter**
(arXiv:2409.02048, 2024) and **SEVA / Stable Virtual Camera**
(arXiv:2503.14489, 2025) couple a geometry prior with the generator
for novel-view synthesis. **AC3D** (arXiv:2411.18673, 2024) shows
empirically that camera motion lives in the low spatial frequencies
and is decided in the first ~10% of denoising steps, so camera
conditioning only needs the early layers and early timesteps.
**GS-DiT** (arXiv:2501.02690, 2025) renders a pseudo-4D Gaussian field
along the target camera path and conditions the generator on the
render.

**Warp-as-History** (arXiv:2605.15182, 2025) is the one to study
closely. It adds *no* camera module: it warps the frames it already
has to the target camera pose, drops the disoccluded tokens, and feeds
the warped frames as "pseudo-history" through the model's existing
history-conditioning pathway. The model inpaints the dropped tokens.
After fine-tuning on a single video it competes with camera-control
methods trained on orders of magnitude more data. Two principles
generalise far beyond cameras: route a new capability through an
interface the model already has rather than bolting on a module, and
hand the model the cheap deterministic part (the warp) for free so it
only has to learn the hard residual.

**Memory and long-horizon consistency.** The core problem is object
permanence: look away from a region, look back, and the model must
reproduce it rather than hallucinate something new. **WorldMem**
(arXiv:2504.12369, 2025) keeps a memory bank of frames indexed by
camera pose and timestamp, and a memory-attention mechanism retrieves
the relevant units across large viewpoint and time gaps -- the
timestamp lets it separate static geometry from dynamic change.
**Video World Models with Long-term Spatial Memory**
(arXiv:2506.05284, 2025) makes the memory an explicit volumetric
geometry, fusing depth observations into a voxel grid (truncated
signed distance function fusion) that is rendered along the new
trajectory and fed back to the generator. **WorldWarp**
(arXiv:2512.19678, 2025) keeps an online 3D Gaussian-splat cache and
uses a spatially varying noise schedule: full noise on disoccluded
pixels (generate), partial noise on warped pixels (refine).

**Aether** (arXiv:2503.18945, ICCV 2025) makes the deepest point:
geometry is not just a memory aid but a *unifying prior* -- one model
jointly trained for 4D reconstruction, action-conditioned prediction,
and goal-conditioned planning, with camera trajectories as the action
space, generalises zero-shot from synthetic to real because geometric
reasoning is domain-invariant.

## 6. Embodied and predictive world models

A separate cluster builds world models for acting agents rather than
for video synthesis -- and this cluster includes the predict-a-latent
camp page 3 endorses.

**NVIDIA Cosmos** (arXiv:2501.03575, 2025) is an open-weight platform
of world foundation models for physical artificial intelligence --
tokenizers, autoregressive and diffusion backbones, post-training
recipes -- framed as a digital twin for training robot and vehicle
policies in simulation.

**V-JEPA 2** (arXiv:2506.09985, 2025) is the most relevant to the
`brain/` arc. It is a JEPA video model -- it predicts in
*representation space*, not pixels -- and its action-conditioned
variant adds a 300M-parameter transformer that predicts future latent
frames from actions. Planning is the Cross-Entropy Method optimising
an action sequence to minimise the latent distance to a goal-image
embedding, with receding-horizon replanning. Trained on under 62 hours
of unlabelled robot video, it reaches 65% zero-shot pick-and-place.
This is a latent world model used for planning by latent goal-distance
minimisation -- structurally the same shape as the `brain/` arc's
Monte Carlo Tree Search over a latent quasimetric.

**Navigation World Models** (arXiv:2412.03572, CVPR 2025) predict
future egocentric video from navigation actions and plan with the
Cross-Entropy Method against a goal image. **DreamDojo**
(arXiv:2602.06949, 2026) is a generalist robot world model pretrained
on 44,000 hours of human video with *continuous latent actions* as
proxy supervision -- the Genie latent-action idea scaled to robotics.
**GAIA-1** (arXiv:2309.17080, 2023) and **GAIA-2** (arXiv:2503.20523,
2025) are Wayve's driving world models; **Vista** (arXiv:2405.17398,
NeurIPS 2024) is a driving world model with multi-modal action
conditioning.

## 7. SANA-WM and its direct competitors

**SANA-WM** (arXiv:2605.15178, May 2026) composes the whole stack: the
LTX-2 tokenizer for compression, a *hybrid* backbone (15 frame-wise
Gated DeltaNet blocks plus 5 softmax blocks across 20 layers,
d_model 2240), dual-branch camera control (a coarse unified camera
positional encoding branch for global six-degree-of-freedom pose plus
a fine Plücker-raymap branch for within-stride motion), a two-stage
pipeline with a flow-matching long-video refiner, and NVFP4
quantisation for single-graphics-card deployment. It is a 2.6B model
trained on ~213K pose-annotated clips in 15 days on 64 H100 GPUs,
generating a 60s 720p clip; the distilled NVFP4 variant does it in
~34s on one RTX 5090. Its claim is efficiency: ~36x the generation
throughput of much larger baselines at comparable quality and better
camera-following accuracy.

Its named competitors map the current frontier of interactive world
models:

- **LingBot-World** (arXiv:2601.20540, 2026) -- a 28B mixture-of-experts
  model (two 14B experts, one active per denoising step) on a Wan2.2
  image-to-video backbone.
- **HY-WorldPlay** (arXiv:2512.14614, 2025) -- a streaming diffusion
  transformer with a dual action representation and a "reconstituted
  context memory" that mixes a rolling temporal window with a
  geometry-sampled spatial window, plus temporal-reframing positional
  tricks; targets long-term geometric consistency.
- **Infinite-World** (arXiv:2602.02393, 2026) -- a 1.3B model reaching
  1000-frame horizons via a hierarchical pose-free memory compressor
  that recursively squeezes the episode into a 20-token budget.
- **Matrix-Game 3.0** (arXiv:2604.08995, 2026) -- a 5B real-time
  streaming model (up to 40 frames per second at 720p) with
  camera-aware memory retrieval and distillation.

The common thread across all five is that long-horizon *memory* is now
the contested axis: every recent system ships a different mechanism
for it (mixture-of-experts capacity, reconstituted context, a
hierarchical compressor, camera-aware retrieval).

## What this means for the `brain/` arc

The arc's experiment 160 just built its first learned world model -- a
warp-residual dynamics model planned over by Monte Carlo Tree Search.
This survey says where it goes next.

**The drift problem is solved territory; reuse it.** Section 3 is a
ready-made menu for experiment 161's latent rollout. The arc's
104-139 experiments drowned in world-model drift; the field's answer
is consistent and proven -- never train the model on a clean prefix it
will not see at rollout time. Diffusion Forcing's per-token noise,
Self-Forcing's train-on-your-own-output, and FramePack's discrete
history and endpoint anchoring are the specific tools.

**The backbone choice for 161 is the hybrid.** When the arc lifts its
world model into latent space and rolls it out deep, section 2 says
use a mostly-linear-recurrent backbone with occasional softmax -- the
Gated DeltaNet hybrid that SANA-WM, Jamba, and Granite all converged
on. The linear state carries the long horizon in bounded memory; the
softmax blocks keep local steps sharp. This is the architectural form
of the arc's own recurring finding that a controller is a reliable
*local* map whose reliable depth must be extended.

**Memory is the field's open frontier, and it agrees with the arc.**
Every section-7 competitor ships a different long-horizon memory
mechanism, and the strongest (WorldMem, the spatial-memory and
WorldWarp lines, Aether) all reach for *explicit, geometry-indexed*
memory rather than a longer attention window. For the arc this is
permission: a learned world model may legitimately carry an explicit
structured state, not only an opaque latent.

**The arc is in the right camp, and V-JEPA 2 is the proof.** The
generative-video systems that dominate this survey are the
"generate-the-input" camp page 3 rejects. The arc does not generate
pixels; it predicts an abstract state and plans by goal-distance. The
methodological siblings are V-JEPA 2 and Navigation World Models --
latent world models planned over by goal-distance minimisation. Monte
Carlo Tree Search over a learned quasimetric is the discrete-search
cousin of the Cross-Entropy Method over a learned latent. The arc
should keep taking *engineering* from the generative camp (the warp
residual, the hybrid backbone, the drift fixes) while keeping its
*paradigm* aligned with V-JEPA 2.

**Warp-as-History is the design principle to internalise.** Route a
new capability through an interface the model already has; hand it the
cheap deterministic part for free and learn only the residual. The arc
has now demonstrated this three times (experiment 153's privileged
channel, 158's loss-not-module win, 160's warp residual). It should be
a standing prior, not a one-off.

## Pointers

- Paradigm and the JEPA line: [03-jepa-and-world-models.md](03-jepa-and-world-models.md).
- The arc's experiments that this survey informs:
  `brain/experiments/156`-`160`.
- Full citations for every arXiv identifier above: see the
  "Generative video and game world models (page 7)" block in
  [references.md](references.md).
