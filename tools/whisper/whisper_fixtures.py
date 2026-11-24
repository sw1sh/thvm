"""whisper_fixtures.py: reference tensors of OpenAI Whisper from the transformers implementation, the spec
the thvm port (wl/THVMLink/Kernel/NN/Examples/Whisper/) is checked against.

    python tools/whisper/whisper_fixtures.py <speech.wav> <fixtures dir> [model]

writes, for one utterance of at most 30 seconds and the model given (tiny.en by default): the source
samples, the samples at 16 kHz (scipy's polyphase resampler), the log-mel features, the encoder's states,
the logits after the start tokens and the greedy tokens with their text, as raw little-endian float32
files with their shapes in meta.json, for tools/whisper/whisper_check.wls.  The decoding is the plain
greedy loop of tinygrad's examples/whisper.py: no timestamps, no suppressed tokens, at most 224 new
tokens.  Needs numpy, scipy, soundfile, torch and transformers, which fetches the model itself.
"""
import json, os, sys
from math import gcd
import numpy as np
import soundfile as sf
import torch
from scipy.signal import resample_poly
from transformers import WhisperFeatureExtractor, WhisperForConditionalGeneration, WhisperTokenizer

wav, out = sys.argv[1], sys.argv[2]
name = sys.argv[3] if len(sys.argv) > 3 else "tiny.en"
repo = "openai/whisper-" + name
os.makedirs(out, exist_ok=True)

source, rate = sf.read(wav, dtype="float32")
if source.ndim > 1:
    source = source.mean(axis=1)
g = gcd(16000, rate)
wave = resample_poly(source, 16000 // g, rate // g).astype(np.float32)
extractor = WhisperFeatureExtractor.from_pretrained(repo)
tokenizer = WhisperTokenizer.from_pretrained(repo)
model = WhisperForConditionalGeneration.from_pretrained(repo, torch_dtype=torch.float32).eval()
features = extractor(wave, sampling_rate=16000, return_tensors="pt").input_features  # (1, 80, 3000)
shapes = {}


def put(key, x):
    x = np.ascontiguousarray(x, np.float32)
    x.tofile(os.path.join(out, key + ".f32"))
    shapes[key] = list(x.shape)


put("wave_source", source)
put("wave16k", wave)
put("mel", features[0].numpy().T)  # time-major (3000, 80)
with torch.no_grad():
    encoded = model.model.encoder(features).last_hidden_state  # (1, 1500, d)
    put("encoded", encoded[0].numpy())
    token = tokenizer.convert_tokens_to_ids
    eot = token("<|endoftext|>")
    start = [token("<|startoftranscript|>")]
    if not name.endswith(".en"):
        start += [token("<|en|>"), token("<|transcribe|>")]
    start.append(token("<|notimestamps|>"))
    context = list(start)
    first = None
    for _ in range(224):
        hidden = model.model.decoder(input_ids=torch.tensor([context]), encoder_hidden_states=encoded).last_hidden_state
        logits = (hidden[0, -1] @ model.model.decoder.embed_tokens.weight.T).numpy()
        if first is None:
            first = logits
        context.append(int(logits.argmax()))
        if context[-1] == eot:
            break
put("logits0", first)
tokens = context[len(start):-1] if context[-1] == eot else context[len(start):]
text = tokenizer.decode(tokens).strip()
with open(os.path.join(out, "meta.json"), "w") as f:
    json.dump({"model": name, "source_rate": rate, "start": start, "eot": eot, "tokens": tokens, "text": text, "shapes": shapes}, f, indent=1)
print(name, shapes)
print(len(tokens), "tokens:", text)
