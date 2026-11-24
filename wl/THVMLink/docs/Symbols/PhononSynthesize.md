---
Template: Symbol
Name: PhononSynthesize
Context: WolframInstitute`THVMLink`Examples`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/PhononSynthesize
Keywords: [Phonon, text-to-speech, speech synthesis, voice, audio, flow matching]
SeeAlso: [TSpeechSynthesize, TSpeak, TVoiceStyleData, TJit, TSet, TReclaim, Audio, SpeechSynthesize, ProgressReporting, FluxGenerate]
RelatedGuides: [THVMLink]
---

## Usage

<code>[PhononSynthesize]()["*text*"]</code> synthesizes speech for the English *text* with the Phonon 31M text-to-speech model and gives an [Audio]() object.

<code>[PhononSynthesize]()["*text*", "Samples"]</code> gives the list of samples.

<!-- #| annotation: 26.11.23: The model is Gradium's Phonon 31M (gradium/phonon-31m-en, Apache 2.0), ported from pocket_tts's modules: a 6-layer flow transformer reads the voice frames, the text tokens and a start latent, a one-step flow head draws a 32-dim latent per 80 ms frame until the end-of-speech logit passes 4, and the Mimi decoder turns the latents into 24 kHz samples. The prefill, one decode step and one Mimi chunk are each captured once with TJit and replayed, with every tensor padded to one shape (prompt 256 positions, context 512, Mimi chunks of 16 frames) and the state updated in place with TSet. Sources: Kernel/NN/Examples/Phonon/ (PhononModel.wl, PhononText.wl, PhononGenerate.wl); architecture page docs/NN/phonon.md; the port is checked against a NumPy reference by tools/phonon/phonon_check.wls. PhononSynthesize calls TReclaim once the session's buffers have grown by 64 MB (each chunk leaves a 0.5 MB prompt upload). PhononUnload[] (no reference page) unloads the model by resetting the engine (TReset), as FluxUnload does. -->

## Details & Options

- The model is Phonon 31M, a 31 million parameter English text-to-speech model: a transformer generates one audio latent for every 80 milliseconds of speech, and a neural audio decoder turns the latents into sound.
- The result is a mono [Audio]() object at 24 kHz.
- The text is first normalized, so that money, web addresses and other symbols are read as words, then split into chunks of whole sentences of at most 50 tokens, and each chunk is spoken on its own.
- The weights are not part of the paclet. The first call fetches the model's three files, about 65 megabytes, from its HuggingFace repository, `gradium/phonon-31m-en`, at a pinned revision, showing the file, the source and the megabytes received as it goes. A download is kept only when its size and SHA-256 are the expected ones.
- The first call also loads the weights and records the kernels of each stage; later calls replay the recorded kernels, so only the first is slow.
- A call that cannot do its job gives a [Failure]() that says why, such as a file that could not be fetched.
- On a central processing unit (CPU), a later call takes about an eighth of the duration of the speech it produces.
- The following options can be given:

  | | | |
  | --- | --- | --- |
  | `"Voice"` | [Automatic]() | the voice: [Automatic]() for the model's default voice, the name of a voice of [TVoiceStyleData](), the path of a voice file, or a `{`*n*`, 512}` array of voice frames |
  | `"Temperature"` | 0.3 | the variance of the noise the latents are drawn from; 0 gives the same audio every time |
  | `"Normalize"` | [True]() | whether to write symbols as words before speaking |
  | `"ModelDirectory"` | [Automatic]() | the directory of the model files; [Automatic]() is `~/.cache/thvm/phonon-31m-en`, and a directory that already holds the files is used without a fetch |
  | [ProgressReporting]() | [Automatic]() | whether to show the progress of the fetch and the load of the first call |

## Basic Examples

Synthesize speech from text:

```wl
#| eval: false
PhononSynthesize["Hello! This is a test of speech synthesis."]
```

<!-- => an Audio object, about 3.4 seconds, 24 kHz mono -->

---

Speech that is the same every time, from a temperature of 0:

```wl
#| eval: false
PhononSynthesize["Hello! This is a test of speech synthesis.", "Temperature" -> 0]
```

<!-- => an Audio object of 3.36 seconds -->

## Scope

Symbols are read as words:

```wl
#| eval: false
PhononSynthesize["It costs $3.50, or about 50 cents more than last year."]
```

<!-- => an Audio object saying "it costs three dollars and fifty cents, or about fifty cents more than last year" -->

---

A longer text is spoken sentence by sentence:

```wl
#| eval: false
Duration[PhononSynthesize["Speech synthesis has a long history. The first machines that could speak were built in the eighteenth century, and they used bellows and reeds. Today, a small neural network running on a laptop can read any text aloud, in real time, with a natural voice."]]
```

<!-- => a duration of about 15 seconds -->

## Options

### Voice

A voice of [TVoiceStyleData]() by its name; its file, a quarter of a megabyte, is fetched on first use:

```wl
#| eval: false
PhononSynthesize["Good morning.", "Voice" -> "Hugh"]
```

<!-- => an Audio object in a British male voice -->

---

The frames of a voice file select the speaker:

```wl
#| eval: false
PhononSynthesize["Good morning.", "Voice" -> FileNameJoin[{$HomeDirectory, ".cache", "thvm", "phonon-31m-en", "default-voice.safetensors"}]]
```

<!-- => an Audio object in the default voice -->

### Temperature

A higher temperature varies the delivery more from call to call:

```wl
#| eval: false
PhononSynthesize["Good morning.", "Temperature" -> 0.8]
```

<!-- => an Audio object -->

### Normalize

Without normalization the text reaches the model as written:

```wl
#| eval: false
PhononSynthesize["The answer is 42.", "Normalize" -> False]
```

<!-- => an Audio object saying "the answer is forty-two" -->

### ModelDirectory

Read the model files from another directory, fetching them there if they are missing:

```wl
#| eval: false
PhononSynthesize["Good morning.", "ModelDirectory" -> FileNameJoin[{$TemporaryDirectory, "phonon"}]]
```

<!-- => an Audio object -->

### ProgressReporting

Fetch and load without a progress display:

```wl
#| eval: false
PhononSynthesize["Good morning.", ProgressReporting -> False]
```

<!-- => an Audio object -->

## Properties and Relations

[TSpeechSynthesize]() with its default method gives the same speech:

```wl
#| eval: false
AudioData[TSpeechSynthesize["Good morning.", "Temperature" -> 0]] == AudioData[PhononSynthesize["Good morning.", "Temperature" -> 0]]
```

<!-- => True -->

## Possible Issues

Without a network connection and without the files in the model directory, the result is a failure that names the file:

```wl
#| eval: false
PhononSynthesize["Good morning.", "ModelDirectory" -> CreateDirectory[]]
```

<!-- => the message PhononSynthesize::fetch is issued and the result is a Failure whose parameters name model.q8.gguf, its address and "the download failed" -->

---

The model speaks English only; text in another language is read with English sounds:

```wl
#| eval: false
PhononSynthesize["Guten Morgen."]
```

<!-- => an Audio object -->
