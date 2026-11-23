---
Template: Symbol
Name: TSpeechSynthesize
Context: WolframInstitute`THVMLink`Examples`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSpeechSynthesize
Keywords: [text-to-speech, speech synthesis, voice, audio]
SeeAlso: [PhononSynthesize, SpeechSynthesize, Audio, FluxGenerate]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSpeechSynthesize]()["*text*"]</code> synthesizes speech for *text* with a text-to-speech model run by thvm and gives an [Audio]() object.

<!-- #| annotation: 26.11.23: TSpeechSynthesize dispatches on Method to a per-model synthesizer and forwards the options that synthesizer declares (Kernel/NN/Examples/SpeechSynthesize.wl, laid out like TImageSynthesize in ImageSynthesize.wl). The one method is "Phonon", PhononSynthesize. An unknown method issues TSpeechSynthesize::badmethod and gives $Failed. -->

## Details & Options

- [TSpeechSynthesize]() is shaped after the built-in [SpeechSynthesize](): text in, an [Audio]() object out.
- The [Method]() option chooses the model. `"Phonon"`, the default, is the English Phonon 31M model of [PhononSynthesize]().
- The other options are those of the chosen synthesizer, such as `"Voice"` and `"Temperature"` for `"Phonon"`.

## Basic Examples

Synthesize speech from text:

```wl
#| eval: false
TSpeechSynthesize["Hello! This is a test of speech synthesis."]
```

<!-- => an Audio object, about 3.4 seconds, 24 kHz mono -->

## Options

### Method

Name the model:

```wl
#| eval: false
TSpeechSynthesize["Good morning.", Method -> "Phonon"]
```

<!-- => an Audio object -->

---

Options of the chosen synthesizer pass through:

```wl
#| eval: false
TSpeechSynthesize["Good morning.", Method -> "Phonon", "Temperature" -> 0]
```

<!-- => an Audio object, the same every time -->

## Possible Issues

An unknown method issues a message:

```wl
#| eval: false
TSpeechSynthesize["Good morning.", Method -> "Unknown"]
```

<!-- => the message TSpeechSynthesize::badmethod is issued and the result is $Failed -->
