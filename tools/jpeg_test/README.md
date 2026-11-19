# jpeg_small tests

Checks `src/jpeg_small.cpp`, the photo decoder, on a Linux PC (or WSL).

```bash
pip install numpy pillow mozjpeg-lossless-optimization   # plus ImageMagick for `convert`
./build.sh
```

- **test.py** encodes test pictures in many sizes and chroma layouts as baseline, progressive
  (libjpeg), progressive (mozjpeg scan script) and with restart markers, and decodes each into
  the 88x59 photo box. Baseline and progressive copies of the same picture must give identical
  pixels (`DIFF` lines are failures). `LOW` lines only note a lower similarity score against a
  full-size reference: the decoder works at reduced size, which softens or slightly sharpens
  edges.
- **fuzz.py** feeds 8,000 truncated, corrupted and random files through the decoder built with
  AddressSanitizer and UBSan. Any `CRASH` line is a failure.
