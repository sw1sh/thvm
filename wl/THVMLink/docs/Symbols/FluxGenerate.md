---
Template: Symbol
Name: FluxGenerate
Context: WolframInstitute`THVMLink`Examples`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/FluxGenerate
Keywords: [FLUX, text-to-image, diffusion, flow matching, Qwen, VAE, MMDiT, latent, guidance-distilled]
SeeAlso: [TJit, TToDevice, TSafeTensorLoad, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[FluxGenerate]()[*prompt*]</code> generates an [Image]() from a text *prompt* with the FLUX.2-klein-4B text-to-image model: a Qwen3-4B text encoder, an MMDiT velocity net sampled by a 4-step Euler flow-match, and an `AutoencoderKLFlux2` decoder.

<code>[FluxGenerate]()[*prompt*, *spec*]</code> returns the part(s) named by *spec*: `"Image"` (the default [Image]()), `"Latent"` (the initial packed-latent noise *z*<sub>0</sub>, a `{S_img, 128}` [NumericArray]() that feeds back through the `"InitialLatent"` option to reproduce the image), `"Embedding"` (the Qwen text embedding, a `{512, 7680}` [NumericArray]()), [All]() (an [Association]() of every part), or a list of those keys (an [Association]() of just those parts).

<code>[FluxGenerate]()[*prompt*, *n*]</code> generates *n* images of the same *prompt*, each from a distinct fresh seed, reusing one model session - only the first is a cold start. <code>[FluxGenerate]()[*prompt*, *n*, *spec*]</code> returns a list of *n* *spec* results.

<code>[FluxGenerate]()[{*prompt*<sub>1</sub>, *prompt*<sub>2</sub>, ...}]</code>, optionally with a trailing *spec*, generates one result per prompt as a batch, building the model session ONCE and replaying the captured kernels per prompt (every result after the first is warm).

## Details & Options

- The model is the same instance loaded once into a persistent session keyed by `{`*ModelDir*`, `*Device*`, `*ImageSize*`, `*Steps*`}`: the first call pays the cold weight load + JIT capture; later calls with the same settings replay the captured velocity / Qwen / VAE kernels, so a second image is warm, not another cold start.
- *spec* is the last positional argument and is type-distinguishable from the integer count *n*, so the call forms compose like <code>[Import]()[*file*, *fmt*]</code>.
- The following options can be given:

  | option | default | |
  | --- | --- | --- |
  | `"ImageSize"` | `{256, 256}` | output size; a scalar *n* means a square *n* x *n* image, a pair `{`*w*`, `*h*`}` a rectangle (rounded to the `/16` patch grid). |
  | `"Steps"` | [Automatic]() | number of Euler sampling steps; [Automatic]() uses `"NumSteps"`. |
  | [RandomSeeding]() | [Automatic]() | [Automatic]() gives a fresh random image each call; an integer seed makes the result reproducible. |
  | `"InitialLatent"` | [Automatic]() | [Automatic]() samples fresh *z*<sub>0</sub> ~ N(0,1); a `{S_img, 128}` array (e.g. a `"Latent"` result) is used as *z*<sub>0</sub>, so a saved latent reproduces its image (a latent round-trip). |
  | `"NegativePrompt"` | [None]() | unsupported (see Possible Issues); a value issues `FluxGenerate::nocfg` and is ignored. |
  | `"ShowSteps"` | `False` | `True` decodes each Euler-step latent and shows the denoising progression live (in a notebook); the per-step Images + latents are then carried under the `"Steps"` return key. |
  | [ProgressReporting]() | [Automatic]() | [Automatic]() or `True` shows a live progress panel (a no-op with no front end); `False` runs silent. |
  | `"Device"` | `"metal"` | backend to run on (`"metal"` &#124; `"cpu"` &#124; `"cuda"`). |
  | `"NumSteps"` | `4` | legacy alias for `"Steps"`. |
  | `"ModelDir"` | [Automatic]() | weight directory ([Automatic]() -> `~/.cache/thvm/flux2-klein-4b`). |

- FLUX.2-klein is *guidance-distilled*: it samples in 4 steps with no classifier-free guidance, so there is no `"NegativePrompt"` and no guidance scale.
- A *spec* that needs no decode runs fewer stages: `"Embedding"` alone runs only the text encoder; `"Latent"` alone runs the sampler but no VAE decode.

## Basic Examples

Generate an image from a prompt (the default `"Image"` spec):

```wl
#| eval: false
FluxGenerate["a cat"]
```
<!-- => -an Image of a cat- -->

A reproducible image from an integer seed:

```wl
#| eval: false
FluxGenerate["a cat", RandomSeeding -> 0]
```
<!-- => -an Image of a cat- -->

## Scope

Return the initial latent instead of the image - a `{64, 128}` [NumericArray]() at the default `128`x`128` grid:

```wl
#| eval: false
Dimensions @ FluxGenerate["a cat", "Latent", "ImageSize" -> {128, 128}]
```
<!-- => {64, 128} -->

Return every part as an [Association]():

```wl
#| eval: false
Keys @ FluxGenerate["a cat", All]
```
<!-- => {"Image", "Latent", "Embedding"} -->

Generate two images of one prompt, each with its own fresh seed, and keep just their latents:

```wl
#| eval: false
Dimensions /@ FluxGenerate["a cat", 2, "Latent"]
```
<!-- => {{64, 128}, {64, 128}} -->

## Properties & Relations

The `"Latent"` result is the round-trip handle: feed it back through `"InitialLatent"` and the same image comes out (to the backend's own bf16-GPU reproducibility floor), so a saved latent controls the generation deterministically:

```wl
#| eval: false
lat = FluxGenerate["a cat", "Latent", RandomSeeding -> 0, "ImageSize" -> {128, 128}];
img = FluxGenerate["a cat", "InitialLatent" -> lat, "ImageSize" -> {128, 128}];
ImageQ[img]
```
<!-- => True -->

With `"ShowSteps" -> True`, an [All]() / `{"Steps"}` return carries the denoising progression - one decoded [Image]() per Euler step plus the running latents:

```wl
#| eval: false
r = FluxGenerate["a cat", All, "ShowSteps" -> True];
{Length @ r["Steps"]["Images"], Keys @ r["Steps"]}
```
<!-- => {4, {"Images", "Latents"}} -->

The text encoder, velocity net, and VAE are each captured once with [TJit]() and replayed; weights load as zero-copy disk-mmap wraps via [TSafeTensorLoad]() and upload with [TToDevice]().

## Possible Issues

- **No negative prompt.** FLUX.2-klein-4B is a guidance-distilled 4-step flow-matching model with no classifier-free guidance - its transformer carries only a `timestep_embedder`, no guidance embedder. A negative prompt needs the conditional-vs-unconditional CFG pass klein was distilled to skip, so `"NegativePrompt"` is unsupported: a value issues `FluxGenerate::nocfg` and is ignored.

```wl
#| eval: false
FluxGenerate["a cat", "NegativePrompt" -> "blurry"]
```
<!-- => FluxGenerate::nocfg fires; an Image of a cat is returned -->

- **Warm-prompt collapse (known bug).** Only the *first* prompt in a session currently generates the correct image. A cold first prompt is right, but a later prompt in a batch, a second call on the cached session, or any `n > 1` count decodes the *first* prompt's content (a warm `"a blue bird"` after `"a red apple on a table"` comes out an apple). This is a C-level JIT capture/replay issue in the velocity sampler (the per-step Euler loop's text-encoding rebind), tracked by the failing `flux/warm-prompt-rebinds-text-encoding` regression test; for now, generate each distinct prompt in a fresh session.

- **Latent shape.** An `"InitialLatent"` array must be `{S_img, 128}` for the current `"ImageSize"` (`S_img = (`*w*`/16)(`*h*`/16)`); a mismatch issues `FluxGenerate::badlatent` and falls back to fresh noise.
- **Weights.** The first call needs the FLUX.2-klein-4B weights (~16 GB across the Qwen, transformer, and VAE safetensors) under `"ModelDir"`; without them the session build fails.
- **Reproducibility floor.** On a GPU the bf16 tensor-core reductions are not bit-reproducible across dispatches, so two identical seeded runs (and a latent round-trip) agree only to ~`1/64` per channel, not byte-for-byte.
