# Self-authored 7z fixtures

These archives contain the same authored patches and metadata as `../rar/input`.
Run `python3 generate.py /path/to/7zz` to regenerate with 7-Zip 26.06. The encoder
is a development tool only; tests and Synth2 need no 7-Zip executable.

Positive cases exercise LZMA, LZMA2, solid LZMA2, stored and PPMd entries,
compressed headers, natural numeric order, Unicode names and exact patch bytes.
Negative cases cover encrypted headers/data, unsupported BZip2/Deflate codecs,
empty or absent patches, oversized patch/non-patch data, a 4-GiB decoder
dictionary, and an entry count above 65536. Header mutations retain valid CRCs.
Payload corruption and truncation are generated at test runtime.
