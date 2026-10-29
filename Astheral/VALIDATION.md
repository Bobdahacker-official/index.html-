# Verification record

- Both training jobs completed. Exported JavaScript inference reproduced at least 90% accuracy on each mode’s held-out split.
- 12 Node checks passed: classifier inference, unknown vocabulary, arithmetic parser, safe filenames, all eight projects’ generated source syntax, code routing, explicit local limitations, extractive summaries, context budgeting, fragmented SSE, server response handling, and ZIP structure.
- ZIP filenames, decoded Unicode contents, and CRC integrity were independently verified with Python’s zipfile.
- HTTP integration checks cover asset serving, server configuration, private path rejection, unconfigured provider errors, and cross-origin request rejection.
- App JavaScript and generated project JavaScript were parsed by Node; Python server was parsed by Python.
- The managed local preview could not start: sites-previewd mailbox is unavailable. No replacement preview infrastructure was installed.
- Actual browser WebGPU inference is unverified in this environment. Browser model integration follows the official WebLLM API and model registry. No claim is made that downloaded Qwen models were trained in this session.
