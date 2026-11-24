---
Template: Symbol
Name: WhisperRecognize
Context: WolframInstitute`THVMLink`Examples`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/WhisperRecognize
Keywords: [Whisper, speech-to-text, speech recognition, transcription, audio]
SeeAlso: [TSpeechRecognize, PhononSynthesize, TSpeechSynthesize, TJit, TSet, TReclaim, SpeechRecognize, Audio, ProgressReporting]
RelatedGuides: [THVMLink]
---

## Usage

<code>[WhisperRecognize]()[*audio*]</code> recognizes the speech in *audio* with OpenAI's Whisper and gives it as a string.

<!-- #| annotation: 26.11.24: The model is OpenAI's Whisper (openai/whisper-*, MIT), ported from tinygrad's examples/whisper.py: 30-second segments of 16 kHz audio become a log-mel spectrogram, an encoder of two convolutions and n transformer layers reads it into 1500 states, and a decoder gives one token at a time, greedily, each segment prompted with the text of the one before. Four steps are captured once with TJit and replayed: the stem (framing, spectrogram, convolutions), one encoder layer with its weights as inputs (replayed per layer, so a capture holds one layer's intermediates), the cross step (TSet of each decoder layer's cross keys and values) and the decode step. Sources: Kernel/NN/Examples/Whisper/ (WhisperModel.wl, WhisperRecognize.wl), Kernel/NN/Examples/SpeechRecognize.wl (audio reading), Kernel/NN/Examples/Models.wl (fetch); architecture page docs/NN/whisper.md; the port is checked against the transformers implementation by tools/whisper/whisper_check.wls. WhisperUnload[] (no reference page) unloads the models by resetting the engine. -->

## Details & Options

- The model is Whisper, a transformer that reads 30 seconds of audio at a time and writes its text one token after another.
- *audio* is an [Audio]() object, the path of an audio file or [File]()[*path*], or a list of samples.
- A WAV file is read directly. Another format goes through [Audio]() where the kernel supports it and through the system's converter otherwise.
- Audio at another rate than 16 kHz is resampled, and several channels are averaged.
- The weights are not part of the paclet. The first call with a model fetches its two files from its HuggingFace repository, `openai/whisper-` followed by the model's name, at a pinned revision, showing the file, the source and the megabytes received as it goes. A download is kept only when its size and SHA-256 are the expected ones.
- The first call with a model also loads the weights and records the kernels of each stage; later calls replay the recorded kernels, so only the first is slow.
- Silence gives an empty string: a silent stretch of 30 seconds is skipped, and so is one the model itself takes for no speech.
- A call that cannot do its job gives a [Failure]() that says why, such as audio that cannot be read or a file that could not be fetched.
- The following options can be given:

  | | | |
  | --- | --- | --- |
  | `"Model"` | `"base"` | the model: `"tiny"`, `"base"` or `"small"`, which know 99 languages, or `"tiny.en"`, `"base.en"` or `"small.en"`, which know English only |
  | [Language]() | [Automatic]() | the language of the speech: a name such as `"German"`, a code such as `"de"`, or [Automatic]() to detect it from the first 30 seconds |
  | `"SampleRate"` | 16000 | the sample rate of a list of samples |
  | `"ModelDirectory"` | [Automatic]() | the directory of the model files; [Automatic]() is `~/.cache/thvm/whisper-` followed by the model's name, and a directory that already holds the files is used without a fetch |
  | [ProgressReporting]() | [Automatic]() | whether to show the progress of the fetch, the load and the segments |

- The models differ in size and speed. On a central processing unit (CPU), with the English-only models:

  | | | |
  | --- | --- | --- |
  | `"tiny.en"` | 151 megabytes | 19 seconds of speech in 0.8 seconds |
  | `"base.en"` | 290 megabytes | 19 seconds of speech in 1.6 seconds |
  | `"small.en"` | 967 megabytes | 19 seconds of speech in 4.5 seconds |

## Basic Examples

Recognize the speech in an audio file:

```wl
#| eval: false
WhisperRecognize["speech.wav"]
```

<!-- => the text spoken in the file, as a string -->

---

Recognize speech that [PhononSynthesize]() made:

```wl
#| eval: false
WhisperRecognize[PhononSynthesize["Hello, this is a test of speech synthesis.", "Temperature" -> 0]]
```

<!-- => "Hello, this is a test of speech synthesis." -->

## Scope

A list of samples at a given rate:

```wl
#| eval: false
WhisperRecognize[PhononSynthesize["Good morning.", "Samples"], "SampleRate" -> 24000]
```

<!-- => "Good morning." -->

---

Speech in another language is recognized in that language:

```wl
#| eval: false
WhisperRecognize["german.wav"]
```

<!-- => the German text spoken in the file -->

---

Silence gives no text:

```wl
#| eval: false
WhisperRecognize[ConstantArray[0., 16000]]
```

<!-- => "" -->

---

Audio longer than 30 seconds is read segment by segment:

```wl
#| eval: false
StringLength[WhisperRecognize["lecture.wav"]]
```

<!-- => the number of characters of the whole transcript -->

## Options

### Model

A smaller model is faster and makes more mistakes:

```wl
#| eval: false
WhisperRecognize["speech.wav", "Model" -> "tiny.en"]
```

<!-- => the text, as a string -->

### Language

Name the language when it is known:

```wl
#| eval: false
WhisperRecognize["german.wav", Language -> "German"]
```

<!-- => the German text spoken in the file -->

---

Speech read as another language comes out translated in part:

```wl
#| eval: false
WhisperRecognize["german.wav", Language -> "English"]
```

<!-- => a string that is partly English, partly German -->

### SampleRate

The rate of a list of samples:

```wl
#| eval: false
WhisperRecognize[samples, "SampleRate" -> 44100]
```

<!-- => the text, as a string -->

### ModelDirectory

Read the model files from another directory, fetching them there if they are missing:

```wl
#| eval: false
WhisperRecognize["speech.wav", "ModelDirectory" -> FileNameJoin[{$TemporaryDirectory, "whisper"}]]
```

<!-- => the text, as a string -->

### ProgressReporting

Fetch, load and recognize without a progress display:

```wl
#| eval: false
WhisperRecognize["speech.wav", ProgressReporting -> False]
```

<!-- => the text, as a string -->

## Properties and Relations

[TSpeechRecognize]() with its default method gives the same text:

```wl
#| eval: false
TSpeechRecognize["speech.wav"] === WhisperRecognize["speech.wav"]
```

<!-- => True -->

## Possible Issues

An English-only model does not take another language:

```wl
#| eval: false
WhisperRecognize["german.wav", "Model" -> "base.en", Language -> "German"]
```

<!-- => the message WhisperRecognize::language is issued and the result is a Failure -->

---

A file that is not there gives a failure that says so:

```wl
#| eval: false
WhisperRecognize["missing.wav"]
```

<!-- => the message WhisperRecognize::audio is issued and the result is a Failure whose parameter reads "there is no file" with the path -->

---

A sound that is not speech may come out as a description of it:

```wl
#| eval: false
WhisperRecognize[0.3 Sin[2. Pi 440. Range[0, 79999] / 16000], "Model" -> "tiny.en"]
```

<!-- => "(phone ringing)" -->
