#!/usr/bin/env python3
# Unisce Samples/*.flac in un solo file Samples.pdcpak
import json, struct, sys, os
src = sys.argv[1] if len(sys.argv) > 1 else 'Samples'
out = sys.argv[2] if len(sys.argv) > 2 else 'Samples.pdcpak'
lib = json.load(open(os.path.join(src, 'library.json')))
blobs = []; off = 0
for s in lib['samples']:
    data = open(os.path.join(src, s['file']), 'rb').read()
    s['offset'] = off; s['size'] = len(data); off += len(data); blobs.append(data)
idx = json.dumps(lib, ensure_ascii=False).encode('utf-8')
with open(out, 'wb') as f:
    f.write(b'PDCPAK1\n'); f.write(struct.pack('<q', len(idx))); f.write(idx)
    for b in blobs: f.write(b)
print(out, len(lib['samples']), 'stab', os.path.getsize(out) // 1024, 'KB')
