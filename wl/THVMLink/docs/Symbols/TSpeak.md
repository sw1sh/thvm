---
Template: Symbol
Name: TSpeak
Context: WolframInstitute`THVMLink`Examples`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSpeak
Keywords: [text-to-speech, speech synthesis, speak, audio, playback]
SeeAlso: [TSpeechSynthesize, PhononSynthesize, Speak, SpeechSynthesize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSpeak]()["*text*"]</code> speaks *text* through the system's audio player.

<!-- #| annotation: 26.11.24: TSpeak synthesizes the samples with the synthesizer TSpeechSynthesize would use (Kernel/NN/Examples/SpeechSynthesize.wl), writes them to a mono 16-bit WAV file in $TemporaryDirectory with BinaryWrite, runs the system's player on the file with RunProcess (afplay on macOS; the first of paplay, aplay, ffplay found on Linux) and deletes the file. It uses no Audio function of the kernel, so it also works on a kernel without audio support; checked on Wolfram 15.1 and on the v2 kernel 0.7.96. With no player installed it issues TSpeak::player and gives $Failed. -->

## Details & Options

- [TSpeak]() is shaped after the built-in [Speak]() for a string: it returns [Null]() when the speech has ended.
- The speech is synthesized as by [TSpeechSynthesize](), with its [Method]() option and the options of the chosen synthesizer, such as `"Voice"` and `"Temperature"` for the default `"Phonon"`.
- The speech is played by the audio player of the operating system, from a temporary file that is deleted afterwards.
- `"Volume"` scales the loudness: 1, the default, plays the speech as synthesized, and 0 plays silence.

## Basic Examples

Speak a sentence:

```wl
#| eval: false
TSpeak["Hello! This is a test of speech synthesis."]
```

<!-- => the sentence is heard; the result is Null -->

## Options

### Volume

Speak at half the loudness:

```wl
#| eval: false
TSpeak["Good morning.", "Volume" -> 0.5]
```

<!-- => the words are heard at half the amplitude; the result is Null -->

### Method

Options of the synthesizer pass through:

```wl
#| eval: false
TSpeak["Good morning.", Method -> "Phonon", "Temperature" -> 0]
```

<!-- => the words are heard, the same way every time; the result is Null -->

## Possible Issues

An unknown method issues a message:

```wl
#| eval: false
TSpeak["Good morning.", Method -> "Unknown"]
```

<!-- => the message TSpeechSynthesize::badmethod is issued and the result is $Failed -->
