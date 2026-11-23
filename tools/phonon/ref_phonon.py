"""ref_phonon.py: a NumPy reference of Gradium Phonon 31M (gradium/phonon-31m-en), read straight from the
runtime's model.q8.gguf: the spec the thvm port (wl/THVMLink/Kernel/NN/Examples/Phonon/) is checked
against.  It follows pocket_tts's modules (flow_lm.py, mlp.py, transformer.py, attention.py, rope.py,
mimi.py, seanet.py, conv.py) with Phonon's differences from transformers' gradium_phonon_tts: exact (erf)
GELU, an RMS time norm, the text table, the voice projection and the speaker/duration condition added to
every audio-latent position.  The prompt is voice frames, then text tokens, then the start-of-audio latent
(the model's chat_template.jinja).

    python tools/phonon/ref_phonon.py <fixtures dir>

writes the tensors of one sentence (default voice, temperature 0) as raw little-endian float32 files and
their shapes in meta.json, for tools/phonon/phonon_check.wls.  The model files are read from PHONON_DIR
(default ~/.cache/thvm/phonon-31m-en, where PhononSynthesize fetches them).  Needs numpy and scipy.
"""
import json, math, os, struct, sys
import numpy as np
from scipy.special import erf

MODEL = os.environ.get("PHONON_DIR", os.path.expanduser("~/.cache/thvm/phonon-31m-en"))

# "Hello! This is a test of speech synthesis." under the model's tokenizer.json.
TEXT_IDS = [3248, 1367, 919, 677, 668, 1606, 672, 660, 3777, 3695, 2639, 662, 983, 663]


# --- weights ------------------------------------------------------------------------------------
def load_gguf(path):
    """Every tensor as float32, torch layout (GGUF dims reversed)."""
    f = open(path, "rb")
    buf = f.read()
    assert buf[:4] == b"GGUF"
    pos = 4
    version, n_t, n_kv = struct.unpack_from("<IQQ", buf, pos); pos += 20

    def rstr():
        nonlocal pos
        (n,) = struct.unpack_from("<Q", buf, pos); pos += 8
        s = buf[pos:pos + n].decode("utf-8", "replace"); pos += n
        return s

    def rval(t):
        nonlocal pos
        fmt = {0: "<B", 1: "<b", 2: "<H", 3: "<h", 4: "<I", 5: "<i", 6: "<f", 7: "<?", 10: "<Q", 11: "<q", 12: "<d"}
        if t in fmt:
            v = struct.unpack_from(fmt[t], buf, pos)[0]; pos += struct.calcsize(fmt[t]); return v
        if t == 8:
            return rstr()
        if t == 9:
            (et,) = struct.unpack_from("<I", buf, pos); pos += 4
            (n,) = struct.unpack_from("<Q", buf, pos); pos += 8
            return [rval(et) for _ in range(n)]
        raise ValueError(t)

    kv = {}
    for _ in range(n_kv):
        k = rstr(); (t,) = struct.unpack_from("<I", buf, pos); pos += 4
        kv[k] = rval(t)
    infos = []
    for _ in range(n_t):
        name = rstr(); (nd,) = struct.unpack_from("<I", buf, pos); pos += 4
        dims = struct.unpack_from(f"<{nd}Q", buf, pos); pos += 8 * nd
        ty, off = struct.unpack_from("<IQ", buf, pos); pos += 12
        infos.append((name, ty, dims, off))
    align = kv.get("general.alignment", 32)
    data = (pos + align - 1) // align * align
    W = {}
    for name, ty, dims, off in infos:
        n = int(np.prod(dims))
        o = data + off
        if ty == 0:
            a = np.frombuffer(buf, np.float32, n, o)
        elif ty == 30:
            a = (np.frombuffer(buf, np.uint16, n, o).astype(np.uint32) << 16).view(np.float32)
        elif ty == 8:
            nb = n // 32
            blk = np.frombuffer(buf, np.uint8, nb * 34, o).reshape(nb, 34)
            d = blk[:, :2].copy().view(np.float16).astype(np.float32)
            q = blk[:, 2:].copy().view(np.int8).astype(np.float32)
            a = (q * d).reshape(-1)
        else:
            raise ValueError(f"{name}: ggml type {ty}")
        W[name] = a.astype(np.float32).reshape(tuple(reversed(dims)))
    return W


# --- layers -------------------------------------------------------------------------------------
def qmm(x, w):
    """x @ w.T for a Q8_0 weight (dequantized at load)."""
    return x @ w.T


def ln(x, w=None, b=None, eps=1e-5):
    m = x.mean(-1, keepdims=True)
    v = ((x - m) ** 2).mean(-1, keepdims=True)
    y = (x - m) / np.sqrt(v + eps)
    return y if w is None else y * w + b


def gelu(x):
    return 0.5 * x * (1 + erf(x / math.sqrt(2)))


def silu(x):
    return x / (1 + np.exp(-x))


def elu(x):
    return np.where(x > 0, x, np.expm1(np.minimum(x, 0)))


def rope(x, pos, max_period=10000.0):
    """x: (T, H, D), interleaved pairs (pocket_tts rope.py); pos: (T,) absolute positions."""
    T, H, D = x.shape
    freqs = np.exp(np.arange(D // 2, dtype=np.float32) * (-math.log(max_period) * 2 / D))
    ang = pos[:, None, None].astype(np.float32) * freqs
    c, s = np.cos(ang), np.sin(ang)
    xr, xi = x[..., 0::2], x[..., 1::2]
    out = np.empty_like(x)
    out[..., 0::2] = xr * c - xi * s
    out[..., 1::2] = xr * s + xi * c
    return out


class Transformer:
    """pocket_tts StreamingTransformer: pre-norm layers, fused qkv, interleaved RoPE, causal attention
    within `context`, optional layer scale; a KV cache per layer for streaming."""

    def __init__(self, W, prefix, n_layers, heads=8, context=None, layer_scale=False):
        self.W, self.p, self.n, self.h, self.ctx, self.ls = W, prefix, n_layers, heads, context, layer_scale
        self.cache = [None] * n_layers
        self.offset = 0

    def __call__(self, x):
        T, C = x.shape
        D = C // self.h
        pos = np.arange(self.offset, self.offset + T)
        for i in range(self.n):
            L = f"{self.p}.layers.{i}."
            w = lambda k: self.W[L + k]
            h = ln(x, w("norm1.weight"), w("norm1.bias"))
            mm = qmm if self.p == "flow_lm.transformer" else (lambda a, b: a @ b.T)
            qkv = mm(h, w("self_attn.in_proj.weight"))
            q, k, v = (qkv[:, j * C:(j + 1) * C].reshape(T, self.h, D) for j in range(3))
            q, k = rope(q, pos), rope(k, pos)
            if self.cache[i] is not None:
                k = np.concatenate([self.cache[i][0], k]); v = np.concatenate([self.cache[i][1], v])
            self.cache[i] = (k, v)
            kpos = np.arange(k.shape[0])
            att = np.einsum("thd,shd->hts", q, k) / math.sqrt(D)
            delta = pos[:, None] - kpos[None, :]
            mask = delta >= 0
            if self.ctx is not None:
                mask &= delta < self.ctx
            att = np.where(mask[None], att, -np.inf)
            att = np.exp(att - att.max(-1, keepdims=True)); att /= att.sum(-1, keepdims=True)
            o = mm(np.einsum("hts,shd->thd", att, v).reshape(T, C), w("self_attn.out_proj.weight"))
            if self.ls:
                o = o * w("layer_scale_1.scale")
            x = x + o
            h = ln(x, w("norm2.weight"), w("norm2.bias"))
            f = mm(gelu(mm(h, w("linear1.weight"))), w("linear2.weight"))
            if self.ls:
                f = f * w("layer_scale_2.scale")
            x = x + f
        self.offset += T
        return x


def flow_head(W, c, x0):
    """LSD with one step (s = 0, t = 1): x0 + flow_net(c, s, t, x0).  pocket_tts mlp.SimpleMLPAdaLN with the
    RMS time norm of transformers' GradiumPhononTTSTimestepNorm."""
    p = "flow_lm.flow_net."
    lin = lambda k, v: v @ W[p + k + ".weight"].T + W[p + k + ".bias"]

    def time_embed(i, t):
        q = f"time_embed.{i}."
        args = t * W[p + q + "freqs"]
        e = np.concatenate([np.cos(args), np.sin(args)])
        h = lin(q + "mlp.2", silu(lin(q + "mlp.0", e)))
        return W[p + q + "mlp.3.alpha"] * h / np.sqrt((h ** 2).mean() + 1e-5)

    x = lin("input_proj", x0)
    y = lin("cond_embed", c) + (time_embed(0, 0.0) + time_embed(1, 1.0)) / 2
    for i in range(6):
        q = f"res_blocks.{i}."
        shift, scale, gate = np.split(lin(q + "adaLN_modulation.1", silu(y)), 3)
        h = ln(x, W[p + q + "in_ln.weight"], W[p + q + "in_ln.bias"], 1e-6) * (1 + scale) + shift
        x = x + gate * lin(q + "mlp.2", silu(lin(q + "mlp.0", h)))
    shift, scale = np.split(lin("final_layer.adaLN_modulation.1", silu(y)), 2)
    return x0 + lin("final_layer.linear", ln(x, eps=1e-6) * (1 + scale) + shift)


def condition(W):
    """The speaker-count and duration condition added to every audio-latent position: num_speakers = 1,
    duration_delta = 0 (config.json)."""
    c = "flow_lm.condition_provider.conditioners."
    spk = W[c + "num_speakers.embed.weight"][1] @ W[c + "num_speakers.output_proj.weight"].T
    half = 64
    phases = 1000.0 * 0.0 / 10000.0 ** (np.arange(half) / (half - 1))
    dur = np.concatenate([np.cos(phases), np.sin(phases)]).astype(np.float32)
    return spk + dur @ W[c + "duration_delta.output_proj.weight"].T


# --- Mimi decoder -------------------------------------------------------------------------------
def conv1d(x, w, b, dilation=1):
    """Causal conv, zero left padding (pocket_tts StreamingConv1d from a fresh state). x (Cin, L), w (Cout, Cin, K)."""
    Cout, Cin, K = w.shape
    pad = (K - 1) * dilation
    xp = np.concatenate([np.zeros((Cin, pad), np.float32), x], 1)
    L = x.shape[1]
    cols = np.stack([xp[:, k * dilation:k * dilation + L] for k in range(K)], 1)   # (Cin, K, L)
    y = np.einsum("ock,ckl->ol", w, cols)
    return y + (b[:, None] if b is not None else 0)


def convtr1d(x, w, b, stride, groups=1):
    """Transposed conv keeping the first L * stride samples (streaming overlap-add from a fresh state)."""
    Cin, L = x.shape
    _, Coutg, K = w.shape
    Cout = Coutg * groups
    y = np.zeros((Cout, (L - 1) * stride + K), np.float32)
    if groups == 1:
        contrib = np.einsum("cl,cok->olk", x, w)                     # (Cout, L, K)
    else:
        contrib = (x[:, :, None] * w[:, 0, None, :])                 # depthwise: (C, L, K)
    for k in range(K):
        y[:, k:k + (L - 1) * stride + 1:stride] += contrib[:, :, k]
    y = y[:, :L * stride]
    return y + (b[:, None] if b is not None else 0)


def mimi_decode(W, lat):
    """lat (T, 32) normalized latents -> 24 kHz audio (T * 1920,)."""
    z = lat * W["flow_lm.emb_std"] + W["flow_lm.emb_mean"]
    x = W["mimi.quantizer.output_proj.weight"][:, :, 0] @ z.T                        # (512, T)
    x = convtr1d(x, W["mimi.upsample.convtr.convtr.weight"], None, 16, groups=512)  # (512, 16T)
    x = Transformer(W, "mimi.decoder_transformer.transformer", 2, context=250, layer_scale=True)(x.T).T
    d = "mimi.decoder.model."
    x = conv1d(x, W[d + "0.conv.weight"], W[d + "0.conv.bias"])
    for up, res, s in ((2, 3, 6), (5, 6, 5), (8, 9, 4)):
        x = convtr1d(elu(x), W[f"{d}{up}.convtr.weight"], W[f"{d}{up}.convtr.bias"], s)
        v = conv1d(elu(x), W[f"{d}{res}.block.1.conv.weight"], W[f"{d}{res}.block.1.conv.bias"])
        v = conv1d(elu(v), W[f"{d}{res}.block.3.conv.weight"], W[f"{d}{res}.block.3.conv.bias"])
        x = x + v
    x = conv1d(elu(x), W[d + "11.conv.weight"], W[d + "11.conv.bias"])
    return x[0]


# --- generation ---------------------------------------------------------------------------------
def synthesize(W, tokens, voice, temperature=0.0, seed=0, max_frames=400, frames_after_eos=1, trace=None):
    """tokens: text token ids; voice: (F, 512) voice-prompt latents.  Returns (latents (T, 32), audio)."""
    c = "flow_lm.condition_provider.conditioners."
    cond = condition(W)
    tr = Transformer(W, "flow_lm.transformer", 6)
    prompt = np.concatenate([
        voice @ W[c + "speaker_wavs.output_proj.weight"].T,
        W[c + "transcript_in_segment.embed.weight"][tokens],
        (W["flow_lm.bos_emb"] @ W["flow_lm.input_linear.weight"].T + cond)[None],
    ])
    rng = np.random.default_rng(seed)
    h = ln(tr(prompt), W["flow_lm.out_norm.weight"], W["flow_lm.out_norm.bias"])[-1]
    lats, eos_at = [], None
    for g in range(max_frames):
        is_eos = (h @ W["flow_lm.out_eos.weight"].T + W["flow_lm.out_eos.bias"])[0] > 4.0
        if is_eos and eos_at is None:
            eos_at = g
        if eos_at is not None and g >= eos_at + frames_after_eos:
            break
        noise = rng.normal(0, math.sqrt(temperature), 32).astype(np.float32) if temperature > 0 else np.zeros(32, np.float32)
        lat = flow_head(W, h, noise)
        if trace is not None:
            trace.append((h.copy(), lat.copy()))
        lats.append(lat)
        x = lat @ W["flow_lm.input_linear.weight"].T + cond
        h = ln(tr(x[None]), W["flow_lm.out_norm.weight"], W["flow_lm.out_norm.bias"])[-1]
    lats = np.stack(lats)
    return lats, mimi_decode(W, lats)


def load_voice(path=None):
    """The voice frames (F, 512) of a .safetensors voice file: its one tensor, (1, 512, F) of f32."""
    buf = open(path or os.path.join(MODEL, "default-voice.safetensors"), "rb").read()
    (n,) = struct.unpack_from("<Q", buf, 0)
    hdr = json.loads(buf[8:8 + n])
    e = next(v for k, v in hdr.items() if k != "__metadata__")
    v = np.frombuffer(buf, np.float32, int(np.prod(e["shape"])), 8 + n + e["data_offsets"][0]).reshape(e["shape"])
    return v.reshape(-1, v.shape[-2], v.shape[-1])[0].T if v.shape[-2] == 512 else v.reshape(-1, 512)


if __name__ == "__main__":
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    W = load_gguf(os.path.join(MODEL, "model.q8.gguf"))
    voice = load_voice()
    shapes = {}

    def put(name, a):
        a = np.ascontiguousarray(a, np.float32)
        a.tofile(os.path.join(out, name + ".f32"))
        shapes[name] = list(a.shape)

    c = "flow_lm.condition_provider.conditioners."
    put("voice", voice)
    put("prompt_in", np.concatenate([
        voice @ W[c + "speaker_wavs.output_proj.weight"].T,
        W[c + "transcript_in_segment.embed.weight"][TEXT_IDS],
        (W["flow_lm.bos_emb"] @ W["flow_lm.input_linear.weight"].T + condition(W))[None]]))
    lats, audio = synthesize(W, TEXT_IDS, voice, temperature=0.0)
    put("latents", lats)
    put("audio", audio)
    json.dump({"ids": TEXT_IDS, "shapes": shapes}, open(os.path.join(out, "meta.json"), "w"), indent=1)
    print(f"{len(lats)} frames, {len(audio) / 24000:.2f} s of audio, rms {np.sqrt((audio ** 2).mean()):.4f} -> {out}")
