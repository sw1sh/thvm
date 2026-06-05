"""thvm's `llm.gguf` surface.

gpt2.py imports `gguf_load` for its `--model_size gpt2_gguf_*` quantized
path.  thvm's bridge has no GGUF dequant kernels yet, so loading a GGUF
checkpoint is unsupported; the default `GPT2.build` (HuggingFace
pytorch_model.bin via torch_load) path is fully supported.  Re-exported
under the dotted `thvm.llm.gguf` name in __init__.py."""


def gguf_load(tensor):
    raise NotImplementedError(
        "thvm: GGUF quantized checkpoints (gpt2_gguf_*) are not supported; "
        "use the default --model_size gpt2 / gpt2-medium / ... path "
        "(HuggingFace pytorch_model.bin via torch_load).")


__all__ = ["gguf_load"]
