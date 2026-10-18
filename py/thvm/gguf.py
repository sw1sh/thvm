"""`thvm.llm.gguf`: gpt2 imports gguf_load for its gpt2_gguf_* sizes, but thvm has no
GGUF dequant kernels, so it raises."""


def gguf_load(tensor):
    raise NotImplementedError(
        "thvm: GGUF quantized checkpoints (gpt2_gguf_*) are not supported; "
        "use the default --model_size gpt2 / gpt2-medium / ... path "
        "(HuggingFace pytorch_model.bin via torch_load).")


__all__ = ["gguf_load"]
