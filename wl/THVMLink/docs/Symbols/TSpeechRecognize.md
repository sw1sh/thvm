---
Template: Symbol
Name: TSpeechRecognize
Context: WolframInstitute`THVMLink`Examples`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TSpeechRecognize
Keywords: [speech-to-text, speech recognition, transcription, audio]
SeeAlso: [WhisperRecognize, TSpeechSynthesize, TSpeak, SpeechRecognize, Audio]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TSpeechRecognize]()[*audio*]</code> recognizes the speech in *audio* with a speech-to-text model run by thvm and gives it as a string.

<!-- #| annotation: 26.11.24: TSpeechRecognize dispatches on Method to a per-model recognizer and forwards the options that recognizer declares (Kernel/NN/Examples/SpeechRecognize.wl, laid out like TSpeechSynthesize in SpeechSynthesize.wl). The one method is "Whisper", WhisperRecognize. An unknown method issues TSpeechRecognize::badmethod and gives a Failure. The same file holds the audio reading the recognizers share: the WAV reader, the resampler, and the reading of Audio objects and other audio files. -->

## Details & Options

- [TSpeechRecognize]() is shaped after the built-in [SpeechRecognize](): audio in, a string out.
- *audio* is an [Audio]() object, the path of an audio file or [File]()[*path*], or a list of samples.
- The [Method]() option chooses the model. `"Whisper"`, the default, is OpenAI's Whisper of [WhisperRecognize]().
- The other options are those of the chosen recognizer, such as `"Model"` and [Language]() for `"Whisper"`.

## Basic Examples

Recognize the speech in an audio file:

```wl
#| eval: false
TSpeechRecognize["speech.wav"]
```

<!-- => the text spoken in the file, as a string -->

## Scope

Recognize speech that [TSpeechSynthesize]() made:

```wl
#| eval: false
TSpeechRecognize[TSpeechSynthesize["Hello, this is a test of speech synthesis.", "Temperature" -> 0]]
```

<!-- => "Hello, this is a test of speech synthesis." -->

## Options

### Method

Name the model:

```wl
#| eval: false
TSpeechRecognize["speech.wav", Method -> "Whisper"]
```

<!-- => the text, as a string -->

---

Options of the chosen recognizer pass through:

```wl
#| eval: false
TSpeechRecognize["speech.wav", Method -> "Whisper", "Model" -> "tiny.en"]
```

<!-- => the text, as a string -->

## Possible Issues

An unknown method issues a message:

```wl
#| eval: false
TSpeechRecognize["speech.wav", Method -> "Unknown"]
```

<!-- => the message TSpeechRecognize::badmethod is issued and the result is a Failure -->
