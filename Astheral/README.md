# Astheral

A local-first AI workspace with Chat and Code modes, streamed language-model responses, real downloadable files, ZIP export, sandboxed HTML previews, text attachments, conversation history, and light/dark themes.

Live private app: https://astheral-ai.whitemoth3.chatgpt.site

## Run

From this folder, run `python server.py`, then open http://127.0.0.1:8000. Python 3.10+ is sufficient; there are no required JavaScript dependencies. Use HTTP rather than opening `index.html` directly, because modules and trained weights are loaded as separate files. A static HTTPS host can serve the folder or `dist/`.

- `npm test` runs the logic and generated-source checks with Node 20+.
- `python -m unittest discover -s tests -p 'test_*.py'` runs HTTP integration tests.
- `npm run build` copies deployable browser assets to `dist/`.

## Engines and defined skills

| Engine | Chat | Code | Requirements |
| --- | --- | --- | --- |
| Local skills | Greetings, basic explanations, arithmetic, extractive summaries, general planning outlines, simple brainstorming and app help | Eight complete built-in projects selected by a trained classifier | Included weights; no network after page assets load |
| Browser AI | Open-ended conversation with Qwen2.5-Instruct | Freeform coding with Qwen2.5-Coder-Instruct; fenced code becomes downloadable files | WebGPU, a compatible browser, HTTPS or localhost, initial model download |
| Optional server | OpenAI-compatible chat completions | OpenAI-compatible code completions | An endpoint and model IDs configured by the owner |

The eight built-in code projects are a persistent todo app, calculator, notepad, Pomodoro timer, Snake game, landing page, JSON utility, and Python CSV profiler. Local generation selects and parameterizes authored recipes; it is not freeform language-model generation. A quoted name after `called`, `named`, or `titled` customizes the HTML title.

Browser AI uses WebLLM 0.2.86 with distinct Chat and Code models. Light uses 0.5B models; Balanced uses 1.5B models. q4f32 variants avoid requiring shader-f16. Expect roughly 1–2 GB of free GPU memory, plus the model download. Hardware and browser limits vary. Only one browser model is resident at a time. Switching Chat/Code loads that mode’s model and uses cached assets when available. Generation runs on the user’s device. Browser AI has no API key or paid service requirement.

**What was actually trained:** two independent, small neural softmax classifiers from random initialization, one per mode. Chat has 120 training and 24 held-out examples; Code has 100 training and 20 held-out examples. Both use normalized unigram/bigram text features and a learned dense softmax layer. These are skill-routing models, not newly trained general-purpose LLMs. Qwen generation weights are pretrained upstream and were not trained or fine-tuned in this build.

## Reproduce both training runs

Install NumPy, then run:

```sh
python training/create_dataset.py
OPENBLAS_NUM_THREADS=1 python training/train.py
npm run build
```

`training/dataset.json` contains the original human-authored training and evaluation examples. `training/train.py` implements softmax regression, cross-entropy loss, and full-batch gradient descent, without a machine-learning framework. `models.json` contains the learned weights consumed by browser inference. `training/report.json` records seeds, parameter counts, losses, predictions, sample counts, dataset hash, and measured accuracy. The tiny, hand-authored test set is a smoke evaluation, not a broad capability benchmark.

| Run | Training / held out | Held-out accuracy | Initial → final loss |
| --- | --- | --- | --- |
| Chat | 120 / 24 | 91.7% | 2.484031 → 0.049106 |
| Code | 100 / 20 | 100% | 2.299078 → 0.038113 |

## Optional model server

Set `ASTHERAL_API_URL` to the full OpenAI-compatible `/v1/chat/completions` endpoint, `ASTHERAL_CHAT_MODEL` and `ASTHERAL_CODE_MODEL` to model IDs that endpoint supports, and optionally `ASTHERAL_API_KEY`. Then run `python server.py`. The server option appears automatically in Models. Supply secrets through environment variables, never committed files. There are no credentials in the browser bundle. The server binds to loopback. A static Sites deployment uses local skills and Browser AI; it does not run this optional Python server.

## Files, privacy, and limits

- Conversations are stored in this browser’s localStorage, up to 30 conversations. Use the sidebar export action to save a Markdown copy. Deleting a conversation removes that local record.
- Attachments are text only, limited to 24 KB. They are passed as conversation context; they are never executed.
- Code blocks with `filename=...` become files. Plain language-tagged code fences get an inferred filename. For safety, downloads flatten nested paths to base filenames and deduplicate names.
- The file panel keeps the latest file of each name in the current conversation. Download individual files or a standards-compliant ZIP. Generated programs run only when you preview or open them yourself.
- HTML preview uses an iframe sandbox with scripts allowed and no same-origin permission. Browser storage is unavailable inside the sandbox; downloaded HTML programs use localStorage when opened normally. Browser sandboxing does not block all network calls from generated HTML.
- Models can make mistakes, and generated code should be reviewed. Browser models have a 4K-token context; the app bounds retained context and output length. Very long code can be cut off; request compact files or one file at a time.
- Browser AI initially fetches its runtime from esm.run, model assets from Hugging Face, and WebAssembly from the MLC GitHub distribution. Fonts use Google Fonts with system fallbacks. Network restrictions, unavailable GPU support, or download failures are shown explicitly. Local skills remain available.
- Browser model inference could not be exercised in this build environment; the managed preview daemon was unavailable. Logic, training, generated JavaScript, ZIP integrity, and local HTTP behavior were tested. See `VALIDATION.md` for the final verification record.

## Research

Primary implementation references consulted on 29 October 2026:

- WebLLM basic usage and streaming: https://webllm.mlc.ai/docs/user/basic_usage.html
- WebLLM worker API: https://webllm.mlc.ai/docs/user/advanced_usage.html
- Official WebLLM model registry: https://github.com/mlc-ai/web-llm/blob/main/src/config.ts
- Official Qwen model card: https://huggingface.co/Qwen/Qwen2.5-0.5B-Instruct
- WebLLM package and CDN installation: https://webllm.mlc.ai/docs/user/get_started.html

Original app code and original training examples: MIT license. Qwen model downloads retain their upstream licenses (Apache 2.0 for the selected variants). WebLLM has its own Apache 2.0 license. No third-party model weights are committed to this repository.
