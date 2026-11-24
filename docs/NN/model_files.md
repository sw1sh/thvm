# Model files

The example models (FLUX, Phonon, Whisper) do not ship their weights with the
paclet. Each fetches its files from its HuggingFace repository on first use,
through one piece of code:
[wl/THVMLink/Kernel/NN/Examples/Models.wl](../../wl/THVMLink/Kernel/NN/Examples/Models.wl).

## Sources

A model names its source as an association: the repository, a revision (the
commit the port was checked against), and for each file its size in bytes and
its SHA-256. The address of a file is
`https://huggingface.co/<repository>/resolve/<revision>/<file>`, so the bytes
cannot change under the port.

| Model | Repository | Files | Bytes | Directory under `~/.cache/thvm` |
| --- | --- | --- | --- | --- |
| FLUX.2 klein 4B | `black-forest-labs/FLUX.2-klein-4B` | 7 | 15968662007 | `flux2-klein-4b` |
| Phonon 31M | `gradium/phonon-31m-en` | 3 | 65046455 | `phonon-31m-en` |
| Phonon's voices | `gradium/phonon-voices` | 1 per voice | 256136 each | `phonon-31m-en/voices` |
| Whisper | `openai/whisper-` and the model's name | 2 | 152 MB to 968 MB | `whisper-` and the model's name |

A file keeps the name it has in its repository, directories included
(`transformer/diffusion_pytorch_model.safetensors`), so a model directory is
the repository's own layout.

FLUX's seven files are the transformer (7.75 GB), the two halves of the
Qwen3-4B text encoder (4.97 GB and 3.08 GB), the VAE (168 MB), and three
tokenizer tables. The small ones are fetched first, so a source that cannot be
reached fails at once.

## The fetch

`hubFetch[sym, source, dir]` gives `True` when `dir` holds the source's files,
fetching the missing ones first, or a `Failure` of the function `sym` (by its
message `sym::fetch`) that names the file, its address and the reason.

- A file already in the directory counts when it has its pinned size, so a
  directory filled by hand works offline. When all are there, each is opened
  once: a file whose size shows but which the system refuses to open, as on a
  drive this process may not read, is a `Failure` rather than a model loaded
  from nothing.
- Before anything is downloaded, what is still to arrive is compared with the
  free space of the disk (`df`); a fetch that would not fit fails at once and
  says how much is needed and how much is free.
- A download goes to a `.part` file beside its place. A `.part` of the full
  size is checked against the SHA-256: it becomes the file when it matches and
  is deleted when it does not. The hash is taken by the system's tool
  (`shasum`, `sha256sum`) where there is one, since it reads the file in
  pieces: a kernel's `FileHash` may read all of a 7 GB file into memory, as the
  v2 kernel's does.
- A download that stops short is kept, and the `Failure` says so; the next call
  continues it from where it stopped (`curl -C -`). A dropped connection in the
  middle of 15 GB costs only what was in flight.
- The download is `curl` in the background, its growing file polled five times
  a second for the progress, because not every kernel can download
  asynchronously. Without `curl` it is one `URLDownload`, from the start.
- Progress goes through `hubReport[text, fraction]`: the file, the source and
  the megabytes so far of the total, then `Checking` and the file while a large
  file is hashed. `hubWithProgress` shows it by the kernel's progress display
  (`EvaluateWithProgress` of the Progress context); FLUX binds it to its own
  panel (`fxWithProgress`).

## Unloading

Unloading a model resets the engine (`TReset`): the kernel table keeps every
tensor a captured step read, so nothing short of a reset frees a model. The
reset invalidates the tensors of every other loaded model too. So each model
registers what it drops (`modelOnReset`), every unload goes through
`modelEngineReset`, and the others load again on their next call.

## Tests

[wl/THVMLink/Tests/models.wlt](../../wl/THVMLink/Tests/models.wlt) checks the
sources as data and, with huggingface.co in reach, runs the fetch on FLUX's
three tokenizer files: subdirectories, a stopped download continued, a stopped
download kept, wrong content refused. It loads no model and fetches no weights.
