---
Template: Symbol
Name: FluxGenerate
Context: WolframInstitute`THVMLink`Examples`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/FluxGenerate
Keywords: [FLUX, text-to-image, image generation, diffusion, flow matching, latent, LoRA]
SeeAlso: [TJit, TRealize, Image, RandomSeeding, ProgressReporting]
RelatedGuides: [THVMLink]
---

## Usage

<code>[FluxGenerate]()["*prompt*"]</code> generates an [Image]() from the text *prompt* with the FLUX.2-klein-4B text-to-image model.

<code>[FluxGenerate]()["*prompt*", *spec*]</code> gives the part of the generation named by *spec*.

<code>[FluxGenerate]()["*prompt*", *n*]</code> generates *n* images of the same *prompt*.

<code>[FluxGenerate]()["*prompt*", *n*, *spec*]</code> gives the part named by *spec* for each of *n* generations.

<code>[FluxGenerate]()[{"$p_1$", "$p_2$", …}]</code> generates one image for each prompt $p_i$.

<code>[FluxGenerate]()[{"$p_1$", "$p_2$", …}, *spec*]</code> gives the part named by *spec* for each prompt.

<!-- #| annotation: 26.10.19: The model is FLUX.2-klein-4B: a Qwen3-4B text encoder, an MMDiT velocity network sampled by a 4-step Euler flow-matching schedule, and the AutoencoderKLFlux2 decoder. The text encoder, the velocity network and the VAE decode are each captured once with TJit and replayed; weights load as zero-copy memory-mapped safetensors (TSafeTensorLoad) and upload with TToDevice. Sessions are cached per {ModelDir, Device, ImageSize, Steps, Q8, LoRA}; FluxUnload[] (no reference page yet) frees one. The warm-prompt collapse the earlier page described (every warm prompt decoding the first prompt's content) is recorded as fixed in the source (df8653ec input rebinding in Jit.wl, plus the capture.c gather-replay dtype guard) and covered by flux/warm-prompt-rebinds-text-encoding in Tests/flux_jit_replay.wlt; it was not re-verified on hardware for this page. The examples are not evaluated in the documentation build: they need the ~16 GB of weights and minutes of GPU time. -->

## Details & Options

- The model is FLUX.2-klein-4B: a Qwen3-4B text encoder, a transformer that predicts the velocity of the denoising flow, sampled in 4 Euler steps, and a variational autoencoder that decodes the final latent to an image.
- *spec* is `"Image"` (the default), `"Latent"`, `"Embedding"`, [All](), or a list of those names. `"Latent"` gives the initial noise latent $z_0$, a [NumericArray]() of shape `{`*s*`, 128}` for *s* image tokens, and `"Embedding"` the text embedding, a [NumericArray]() of shape `{512, 7680}`. [All]() or a list gives an [Association]() of the named parts.
- A *spec* that needs no image runs fewer stages: `"Embedding"` alone runs only the text encoder, and `"Latent"` alone runs the sampler without the image decoder.
- The first generation loads the weights and records the kernels of each stage; later generations with the same settings replay the recorded kernels, so only the first is slow. The forms with *n* or a list of prompts share one such session, and each of *n* images gets its own random seed.
- The weights, about 16 gigabytes in total, are read from `"ModelDir"`; without them no session can be built.
- FLUX.2-klein is distilled to sample without classifier-free guidance, so it has no guidance scale and no negative prompt.
- The default device is the graphics processing unit (GPU) through Metal. The following options can be given:

  | | | |
  | --- | --- | --- |
  | `"ImageSize"` | `{256, 256}` | the size of the image, `{`*w*`, `*h*`}` or *n* for *n* x *n*, rounded to a multiple of 16 |
  | `"Steps"` | [Automatic]() | the number of Euler sampling steps; [Automatic]() uses `"NumSteps"` |
  | [RandomSeeding]() | [Automatic]() | an integer seed makes the image reproducible; [Automatic]() gives a new image each time |
  | `"InitialLatent"` | [Automatic]() | a `{`*s*`, 128}` array used as the initial noise $z_0$, such as a `"Latent"` result |
  | `"NegativePrompt"` | [None]() | not supported; a value issues a message and is ignored |
  | `"ShowSteps"` | [False]() | whether to decode and show the latent after each sampling step |
  | [ProgressReporting]() | [Automatic]() | whether to show a progress panel in a notebook |
  | `"Device"` | `"metal"` | the backend: `"metal"`, `"cpu"` for the central processing unit (CPU), or `"cuda"` |
  | `"NumSteps"` | 4 | the number of sampling steps when `"Steps"` is [Automatic]() |
  | `"ModelDir"` | [Automatic]() | the directory of the weights; [Automatic]() is `~/.cache/thvm/flux2-klein-4b` |
  | `"Q8"` | [False]() | whether to store the large projection weights as 8-bit integers, halving their memory |
  | `"LoRA"` | [None]() | a low-rank adaptation (LoRA) file, a list of files, or a list of *file* -> *scale* rules, merged into the transformer weights |

## Basic Examples

Generate an image from a prompt:

```wl
#| eval: false
FluxGenerate["a cat"]
```

<!-- => an Image of a cat, 256 x 256 -->

---

A reproducible image, from an integer seed:

```wl
#| eval: false
FluxGenerate["a cat", RandomSeeding -> 0]
```

<!-- => an Image of a cat; the same seed gives the same image -->

## Scope

The initial latent instead of the image; at 128 x 128 it has 64 image tokens:

```wl
#| eval: false
Dimensions[FluxGenerate["a cat", "Latent", "ImageSize" -> {128, 128}]]
```

<!-- => {64, 128} -->

---

The text embedding, which runs only the text encoder:

```wl
#| eval: false
Dimensions[FluxGenerate["a cat", "Embedding"]]
```

<!-- => {512, 7680} -->

---

Every part, as an association:

```wl
#| eval: false
Keys[FluxGenerate["a cat", All]]
```

<!-- => {"Image", "Latent", "Embedding"} -->

---

Two images of one prompt, each from its own seed, keeping only their latents:

```wl
#| eval: false
Dimensions /@ FluxGenerate["a cat", 2, "Latent", "ImageSize" -> {128, 128}]
```

<!-- => {{64, 128}, {64, 128}} -->

---

One image per prompt, from one session:

```wl
#| eval: false
FluxGenerate[{"a red apple on a table", "a blue bird"}]
```

<!-- => a list of two Images -->

## Options

### ImageSize

A rectangular image:

```wl
#| eval: false
ImageDimensions[FluxGenerate["a lighthouse at dusk", "ImageSize" -> {384, 256}]]
```

<!-- => {384, 256} -->

### Steps

More sampling steps than the default 4:

```wl
#| eval: false
FluxGenerate["a cat", "Steps" -> 8]
```

<!-- => an Image of a cat, sampled in 8 steps -->

### RandomSeeding

The same seed gives the same initial latent:

```wl
#| eval: false
FluxGenerate["a cat", "Latent", RandomSeeding -> 7] == FluxGenerate["a cat", "Latent", RandomSeeding -> 7]
```

<!-- => True -->

### InitialLatent

A latent saved from one generation:

```wl
#| eval: false
lat = FluxGenerate["a cat", "Latent", RandomSeeding -> 0, "ImageSize" -> {128, 128}]
```

<!-- => a NumericArray of dimensions {64, 128} -->

Used as the initial noise, it reproduces that generation's image:

```wl
#| eval: false
FluxGenerate["a cat", "InitialLatent" -> lat, "ImageSize" -> {128, 128}]
```

<!-- => the Image the seed 0 gives -->

### NegativePrompt

A negative prompt is not supported; it issues a message and is ignored:

```wl
#| eval: false
FluxGenerate["a cat", "NegativePrompt" -> "blurry"]
```

<!-- => the message FluxGenerate::nocfg is issued and an Image of a cat is generated -->

### ShowSteps

Keep the decoded image of each sampling step:

```wl
#| eval: false
Length[FluxGenerate["a cat", All, "ShowSteps" -> True]["Steps"]["Images"]]
```

<!-- => 4 -->

### ProgressReporting

Generate without the progress panel:

```wl
#| eval: false
FluxGenerate["a cat", ProgressReporting -> False]
```

<!-- => an Image of a cat -->

### Device

Run on the CPU instead of the GPU:

```wl
#| eval: false
FluxGenerate["a cat", "Device" -> "cpu", "ImageSize" -> {128, 128}]
```

<!-- => an Image of a cat, 128 x 128 -->

### NumSteps

The number of steps used when `"Steps"` is [Automatic]():

```wl
#| eval: false
FluxGenerate["a cat", "NumSteps" -> 6]
```

<!-- => an Image of a cat, sampled in 6 steps -->

### ModelDir

Read the weights from another directory:

```wl
#| eval: false
FluxGenerate["a cat", "ModelDir" -> FileNameJoin[{$HomeDirectory, "models", "flux2-klein-4b"}]]
```

<!-- => an Image of a cat -->

### Q8

Store the large projection weights as 8-bit integers to save memory:

```wl
#| eval: false
FluxGenerate["a cat", "Q8" -> True]
```

<!-- => an Image of a cat -->

### LoRA

Merge a low-rank adaptation into the transformer at half strength:

```wl
#| eval: false
FluxGenerate["a cat", "LoRA" -> {FileNameJoin[{$HomeDirectory, "loras", "style.safetensors"}] -> 0.5}]
```

<!-- => an Image of a cat in the adapted style -->

## Properties and Relations

The `"Latent"` result is what `"InitialLatent"` takes, so a saved latent reproduces its image:

```wl
#| eval: false
lat = FluxGenerate["a cat", "Latent", RandomSeeding -> 0, "ImageSize" -> {128, 128}]
```

<!-- => a NumericArray of dimensions {64, 128} -->

The image from the saved latent:

```wl
#| eval: false
ImageQ[FluxGenerate["a cat", "InitialLatent" -> lat, "ImageSize" -> {128, 128}]]
```

<!-- => True -->

## Possible Issues

On a GPU, the reductions in 16-bit floating point are not reproducible bit for bit, so two runs with the same seed, or a latent round trip, agree to about 1/64 per channel rather than exactly:

```wl
#| eval: false
Max[Abs[ImageData[FluxGenerate["a cat", RandomSeeding -> 0]] - ImageData[FluxGenerate["a cat", RandomSeeding -> 0]]]]
```

<!-- => a small number, about 1/64 or less -->

---

An `"InitialLatent"` whose shape does not match the `"ImageSize"` issues a message and is replaced by new noise:

```wl
#| eval: false
FluxGenerate["a cat", "InitialLatent" -> ConstantArray[0., {10, 128}], "ImageSize" -> {128, 128}]
```

<!-- => the message FluxGenerate::badlatent is issued and an Image is generated from fresh noise -->
