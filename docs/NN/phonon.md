# Phonon text to speech

`PhononSynthesize` speaks English text with Gradium's Phonon 31M
(`gradium/phonon-31m-en`, Apache 2.0), a 31 million parameter text-to-speech
model, on the thvm tensor runtime. The port is Wolfram Language over THVMLink:
[wl/THVMLink/Kernel/NN/Examples/Phonon/](../../wl/THVMLink/Kernel/NN/Examples/Phonon/).
`TSpeechSynthesize` is the generic entry that picks a synthesizer by `Method`,
and `TSpeak` plays the speech
([wl/THVMLink/Kernel/NN/Examples/SpeechSynthesize.wl](../../wl/THVMLink/Kernel/NN/Examples/SpeechSynthesize.wl)).

```wl
Get["WolframInstitute`THVMLink`Examples`"]
PhononSynthesize["Hello! This is a test of speech synthesis."]   (* an Audio, 24 kHz mono *)
```

## The model

| Part | What it does |
| --- | --- |
| Flow transformer | 6 pre-norm layers, 512 wide, 8 heads, rotary positions, causal. Reads the prompt, then one position per audio frame. |
| Flow head | A 6-block conditioned multilayer perceptron. One step maps noise to a 32-dim audio latent, conditioned on the transformer's last hidden state. |
| End-of-speech head | A logit on the hidden state; above 4 the speech has ended. |
| Mimi decoder | Latents to 24 kHz samples: a 16x transposed convolution, 2 transformer layers over a 250-step window, and a convolutional decoder with 6x, 5x and 4x upsampling. One latent is 80 ms, 1920 samples. |

The prompt is the voice (125 frames of a reference speaker, projected to the
model width), the text tokens, and a start latent. Each generated latent goes
back in through a linear layer, with a speaker-count and duration condition
added.

The spec is pocket_tts (the reference implementation of this model family) with
Phonon's differences from it: exact GELU, a root-mean-square norm closing the
time embeddings, and the condition added at every audio position. The weights
are read from the runtime's `model.q8.gguf` (bf16 tensors, and 8-bit blocks for
the flow transformer's projections) and decoded to f32 on the host at load.

## Files

| File | Holds |
| --- | --- |
| `PhononModel.wl` | weight loading, the transformer layer, the flow head, the Mimi decoder, the session |
| `PhononText.wl` | the tokenizer, prompt preparation, chunking, the normaliser |
| `PhononGenerate.wl` | `PhononSynthesize`, `PhononUnload`, model fetch, voices |

## Weights

The weights are not shipped with the paclet. The first call fetches three
files from the model's HuggingFace repository, `gradium/phonon-31m-en`, at the
revision the port was checked against, into `~/.cache/thvm/phonon-31m-en` (or
the `"ModelDirectory"` given):

| File | Bytes | What |
| --- | --- | --- |
| `model.q8.gguf` | 64512352 | the flow transformer, flow head and Mimi decoder |
| `tokenizer.json` | 277967 | the SentencePiece Unigram vocabulary |
| `default-voice.safetensors` | 256136 | the default voice's frames |

- The fetch runs under a progress display that names the file and the source
  and counts megabytes (`ProgressReporting -> False` hides it).
- A download goes to a `.part` file and is kept only when its size and SHA-256
  are the pinned ones (`$phFiles` in `PhononGenerate.wl`); otherwise it is
  deleted and the call returns a `Failure` that names the file, its address
  and the reason.
- The fetch itself (`hubFetch`) is in `Models.wl` beside the model
  directories, for every example model that fetches from HuggingFace.
- A file already in the directory counts when it has its pinned size, so a
  directory filled by hand works offline.
- The download is `curl` in the background, its growing file polled for the
  progress, because not every kernel can download asynchronously; without
  `curl` it is one `URLDownload`.

## One shape per step

A decode loop whose tensors grow with the text would compile new kernels for
every sentence and every frame. Every step here has one shape whatever the
text:

- The prompt is zero-padded to 256 positions and prefilled under a causal mask,
  so the padding never reaches the real positions.
- Keys and values live in caches of 512 rows whose unused rows are zero. A
  decode step adds its row through a one-hot column and masks the rows after
  it, so the position is data, not shape.
- Mimi decodes 16 frames at a time over its last 250 keys; zero frames pad the
  last chunk, and causality keeps them out of the real samples.

The prefill, one decode step and one Mimi chunk are each captured once with
`TJit` and replayed. A replay runs the recorded kernels into the same buffers,
and the steps update their state buffers in place (`TSet`), so synthesis
allocates nothing per frame. A step writes a state buffer only after realizing
everything that reads its old contents. The host-built inputs of a step (rotary
tables, one-hot columns, masks) are kept by position and reused.

Each projection is realized on both sides (`phMat`), so it is a matrix product
alone in its kernel and runs through BLAS; fused with the normalization or
activation around it, it would not.

## Text

1. `phNormalize` writes symbols as words, after the English rules of the
   model's own runtime: money, thousands separators, decimal points, degrees,
   web and mail addresses, month abbreviations and operators. Whole numbers
   stay digits, which the model reads itself.
2. `phChunks` splits the text into sentences and packs them into chunks of at
   most 50 tokens (pocket_tts `split_into_best_sentences`); a longer sentence
   splits at commas, semicolons and colons.
3. `phPrepare` capitalizes a chunk and closes it with sentence punctuation.
4. `phEncode` is the SentencePiece Unigram tokenizer of the model's
   `tokenizer.json`: the best-scoring split of each word into pieces, with
   byte tokens for characters no piece covers.

A chunk is spoken on its own: prefill, then frames until the end-of-speech
logit fires (plus 1 frame, or 3 for a chunk of up to 4 words), within
`ceil((tokens / 3 + 2) * 12.5)` frames.

## Checks

[tools/phonon/ref_phonon.py](../../tools/phonon/ref_phonon.py) is a NumPy
implementation of the model from the same weights. It writes the tensors of one
sentence, and [tools/phonon/phonon_check.wls](../../tools/phonon/phonon_check.wls)
compares the port against them:

| Quantity | Max absolute difference |
| --- | --- |
| prompt rows | 1e-6 |
| latents, 42 frames, same end frame | 7e-4 |
| audio from the reference latents | 4e-6 (signal rms 0.059) |

The tokenizer gives the HuggingFace tokenizer's ids, byte fallback included,
and the normaliser follows the reference runtime's on its test sentences except
where that one is wrong ("1 dollars") or drops a range dash
([wl/THVMLink/Tests/phonon.wlt](../../wl/THVMLink/Tests/phonon.wlt)).

## Without the kernel's audio support

The engine asks little of the kernel it runs on, so that it also runs on a
Wolfram Language kernel that has no audio functions yet:

- `TSpeechSynthesize[text, "Samples"]` gives the samples as a list; the
  `Audio` form only wraps them.
- `TSpeak` writes the samples to a 16-bit WAV file itself and hands the file to
  the system's player (`afplay` on macOS; `paplay`, `aplay` or `ffplay` on
  Linux), so it needs neither `AudioPlay` nor `Export`.
- Host arrays reach thvm through `TTensor[shape, data]`, which converts to f32
  in C. Building a `"Real32"` `NumericArray` first, as `TTensorCreate` does, is
  what a kernel may be slow at: on the v2 kernel it took 11 microseconds an
  element, 499 seconds for the weights against 14 this way.

On the v2 kernel (build 0.7.96, no fallback kernel) the samples of the test
sentence are byte for byte those of Wolfram 15.

## Speed and memory

On the CPU backend of an Apple M3 Max, a warm call takes about 0.13 seconds
per second of audio: 8 ms per frame for the flow transformer and head, and 40 ms
per 16-frame Mimi chunk. The first call in a process loads the weights (3 s)
and captures the three steps (1.3 s with the kernels in the on-disk cache; the
first run on a machine compiles them, about a minute).

A session holds about 600 MB of buffers. Each chunk leaves its prompt upload
(0.5 MB) behind, since the CPU backend does not free the buffer of a dropped
tensor on its own; `PhononSynthesize` calls `TReclaim` once the buffers have
grown by 64 MB. `PhononUnload` resets the engine, as `FluxUnload` does: the
kernel table keeps every tensor a session's kernels read, so nothing short of
a reset frees them. The reset takes the other loaded example models with it;
each registers what to drop (`modelOnReset` in `Models.wl`) and loads again on
its next call.
