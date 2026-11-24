# Whisper speech to text

`WhisperRecognize` turns speech into text with OpenAI's Whisper
(`openai/whisper-*`, MIT) on the thvm tensor runtime. The port is Wolfram
Language over THVMLink:
[wl/THVMLink/Kernel/NN/Examples/Whisper/](../../wl/THVMLink/Kernel/NN/Examples/Whisper/).
`TSpeechRecognize` is the generic entry that picks a recognizer by `Method`,
and holds the reading of audio
([wl/THVMLink/Kernel/NN/Examples/SpeechRecognize.wl](../../wl/THVMLink/Kernel/NN/Examples/SpeechRecognize.wl)).

```wl
Get["WolframInstitute`THVMLink`Examples`"]
WhisperRecognize["speech.wav"]                        (* a string *)
WhisperRecognize[samples, "SampleRate" -> 24000, "Model" -> "tiny.en"]
```

## The model

| Part | What it does |
| --- | --- |
| Front end | 16 kHz samples in 30-second segments; a 400-point Hann-windowed Fourier transform every 160 samples, 80 mel bands, log10 floored 8 below the maximum: 3000 frames of 80. |
| Encoder | Two convolutions of kernel 3 (the second of stride 2) with the exact GELU, sinusoidal positions, n pre-norm transformer layers, a final layer norm: 1500 states. |
| Decoder | Token and learned position embeddings, n layers that attend the decoder's own past and then the encoder's states, a final layer norm, and the token table again as the output projection. |

The spec is tinygrad's `examples/whisper.py` (`AudioEncoder`, `TextDecoder`,
`MultiHeadAttention` with its self and cross caches, `prep_audio`,
`transcribe_waveform`) and `examples/audio_helpers.py` (`mel`), with two things
the original model has and that example leaves out: the exact GELU, and frames
centered by reflecting the segment's edges. The weights are read from the
HuggingFace `model.safetensors` of each model, mapped, not copied: a linear
layer multiplies by a transposed view of its mapped weight.

| Model | Width | Heads | Layers | Languages |
| --- | --- | --- | --- | --- |
| `tiny`, `tiny.en` | 384 | 6 | 4 + 4 | 99, English |
| `base`, `base.en` | 512 | 8 | 6 + 6 | 99, English |
| `small`, `small.en` | 768 | 12 | 12 + 12 | 99, English |

The sizes are read off the tensors of the file, so the code has no table of
them.

## Files

| File | Holds |
| --- | --- |
| `WhisperModel.wl` | the front end, weight loading, the layers, the session's captured steps, greedy decoding, language detection, the text of tokens |
| `WhisperRecognize.wl` | `WhisperRecognize`, `WhisperUnload`, the model registry, languages |
| `../SpeechRecognize.wl` | `TSpeechRecognize`, the WAV reader, the resampler, audio files and objects |
| `../Models.wl` | the fetch from HuggingFace and the engine reset, shared with Phonon and FLUX |

## Weights

The weights are not shipped with the paclet. The first call with a model
fetches two files from its HuggingFace repository, `openai/whisper-` and the
model's name, at the revision the port was checked against, into
`~/.cache/thvm/whisper-` and the name (or the `"ModelDirectory"` given):

| Model | `model.safetensors` bytes | `vocab.json` bytes |
| --- | --- | --- |
| `tiny` | 151061672 | 835550 |
| `tiny.en` | 151060136 | 798156 |
| `base` | 290403936 | 835550 |
| `base.en` | 290401888 | 798156 |
| `small` | 966995080 | 835550 |
| `small.en` | 966992008 | 798156 |

The fetch is the one Phonon uses ([phonon.md](phonon.md), Weights), now in
`Models.wl`: a progress display that names the file, the source and the
megabytes; a download kept only when its size and SHA-256 are the pinned ones
(`$whModels` in `WhisperRecognize.wl`); a `Failure` that names the file, its
address and the reason otherwise.

## One shape per step

As in Phonon, every step has one shape and is captured once with `TJit`:

- **Stem.** Takes the samples of a segment (480320: the segment with its
  edges reflected by 200 samples), frames them on the device (a frame is two
  and a half consecutive hops of 160), computes the spectrogram and runs the
  two convolutions.
- **Layer.** One encoder layer is captured with its 15 weights as inputs and
  replayed for each layer. A capture keeps the buffers of every kernel it
  recorded, and the attention matrices of 1500 positions are most of an
  encoder's intermediates, so capturing the whole encoder would keep them for
  every layer: 1.5 GB for `tiny.en` against 0.6 GB this way.
- **Cross step.** The final layer norm, then each decoder layer's cross keys
  and values written into state buffers (`TSet`).
- **Decode step.** One token: its keys and values go into the cache row of its
  position through a one-hot column, the rows after it are masked, and the
  step gives the logits of the next token.

The decoder's input, the token's row of the embedding table plus the row of
its position, is built on the host: the token's row is read from the model
file at the tensor's offset. A one-hot row times the table on the device costs
a product over the whole vocabulary on every step, 27 ms of a 37 ms step for
`tiny.en`.

Every product is a kernel of its own, so that BLAS runs it (`whMat`); the
output projection is the last hidden row times a transposed view of the token
table, 2.6 ms for `base.en` against 7.6 ms as a matrix-vector reduce. The two
convolutions are products as well (`whConv`): an output row reads three
consecutive rows of the padded input, laid side by side, times the kernel as a
`{3 in, out}` matrix; with stride 2 the three rows are the even and odd rows of
the input taken in pairs. As generic convolution kernels they took 241 ms of a
415 ms encode for `tiny.en`.

## Decoding

`whTranscribe` follows tinygrad's `transcribe_waveform`:

1. Each segment is encoded, then decoded greedily from the start tokens until
   the end-of-text token, 224 new tokens, or the end of the 448-token context.
2. The start tokens are the start-of-transcript token, for a multilingual
   model the language and the transcribe task, and the no-timestamps token.
3. From the second segment on, the prompt is the start-of-previous token, the
   text tokens so far (at most 224 less the start tokens) and the start
   tokens.
4. `whText` joins the strings of the text tokens. The vocabulary is byte-level
   as GPT-2's: each character of a token stands for a byte, and the bytes of
   all tokens together are UTF-8.

`Language -> Automatic` has a multilingual model detect the language from the
first segment: the likeliest of the 99 language tokens after the
start-of-transcript token (OpenAI's `detect_language`; tinygrad's example
assumes English). A language is given by its code (`"de"`) or its English name
(`"German"`).

## Audio

Whisper wants mono samples at 16 kHz. `speechSamples` gets them from:

- a list of samples at the `"SampleRate"` given;
- an `Audio` object, through `AudioData` where the kernel has audio support
  and from the samples it was built of where `Audio` is an inert head;
- a WAV file, parsed here (`speechWavRead`): PCM of 8, 16, 24 or 32 bits and
  floats of 32 or 64, the channels averaged;
- another audio file, through the kernel's `Audio` where that works, and else
  through the system's converter (`afconvert` on macOS, `ffmpeg`).

`speechResample` changes the rate by band-limited interpolation: a sinc cut at
the lower of the two Nyquist frequencies under a Blackman window of 16 zero
crossings a side. With the ratio p / q in lowest terms, every p-th output
shares its taps, so each tap is one strided product over the input. A 440 Hz
tone comes through a change of rate to within 2e-5.

## Checks

[tools/whisper/whisper_fixtures.py](../../tools/whisper/whisper_fixtures.py)
writes the tensors of one utterance from the transformers implementation, and
[tools/whisper/whisper_check.wls](../../tools/whisper/whisper_check.wls)
compares the port against them, for `tiny.en`:

| Quantity | Max absolute difference |
| --- | --- |
| samples resampled from 24 kHz | 2.4e-3 of a peak of 0.56 (scipy's filter is another) |
| log-mel spectrogram | 6e-6 |
| encoder states | 1.4e-4 (largest state 12.7) |
| logits after the start tokens | 4e-5 |
| greedy tokens | the same |

`base` detects and transcribes German speech; `tiny`, `base.en` and `small.en`
transcribe the test recordings; `small` was not run.
[wl/THVMLink/Tests/whisper.wlt](../../wl/THVMLink/Tests/whisper.wlt) covers
the audio reading and the front end without a model, and with the files in
place has Whisper recognize the sentence Phonon speaks.

The port found a scheduler bug: a rank-growing `EXPAND` over a reshaped source
read element 0 everywhere, which is what the decoder's output projection first
was (`TMatVec` over the last hidden row reshaped to a vector). The fix is in
`src/schedule/rangeify_unified.c`, with `tests/test_expand_axis.c`.

## Without the kernel's audio support

The engine asks little of the kernel it runs on, as Phonon does
([phonon.md](phonon.md)): no `Import`, `AudioData` or `AudioResample` is
needed for a WAV file or a list of samples, and an inert `Audio[samples,
SampleRate -> rate]` is read from its arguments. On the v2 kernel (build
0.7.102, no fallback kernel) the transcripts are those of Wolfram 15.

## Speed and memory

On the CPU backend of an Apple M3 Max, warm:

| Model | Encode a segment | Decode a token | 18.9 s of speech | Buffers held |
| --- | --- | --- | --- | --- |
| `tiny.en` | 0.18 s | 10 ms | 0.8 s | 0.56 GB |
| `base.en` | 0.4 s | 18 ms | 1.6 s | 0.86 GB |
| `small.en` | 1.1 s | 49 ms | 4.5 s | 1.95 GB |

- A segment is always the full 30 seconds, so a short clip pays a whole
  encode.
- The buffers include the mapped weights, which are file pages the system can
  drop: the process footprint is lower (0.6, 0.8 and 1.2 GB).
- The first call with a model loads it and captures the four steps: about a
  second for `tiny.en` with the kernels in the on-disk cache; the first run on
  a machine compiles them.
- Each segment leaves its uploads behind, so `whTranscribe` calls `TReclaim`
  after each one.
- `WhisperUnload` resets the engine, as `PhononUnload` and `FluxUnload` do.
  The reset takes every loaded example model with it, so each registers what
  to drop (`modelOnReset` in `Models.wl`) and loads again on its next call.
