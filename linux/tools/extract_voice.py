#!/usr/bin/env python3
"""Extract CDI_VOICE_NNN Opus clips from an edition's resource-only voice DLL.

  ShizukuVoice.dll  -> <out>/Shizuku/CDI_VOICE_007.opus ...
  AoiVoice-en.dll   -> <out>/Aoi-en/...
"""
import os
import sys

import pefile


def main(dll, out_root):
    base = os.path.basename(dll)
    name = os.path.splitext(base)[0].replace("Voice", "")
    out = os.path.join(out_root, name)
    pe = pefile.PE(dll, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    count = 0
    if not hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
        sys.stderr.write("%s: no resources\n" % base)
        return
    for rtype in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tname = str(rtype.name) if rtype.name else ""
        if tname.upper() != "OPUS":
            continue
        for entry in rtype.directory.entries:
            rname = str(entry.name) if entry.name else "ID_%d" % entry.id
            for lang in entry.directory.entries:
                data = pe.get_data(lang.data.struct.OffsetToData, lang.data.struct.Size)
                os.makedirs(out, exist_ok=True)
                with open(os.path.join(out, rname + ".opus"), "wb") as f:
                    f.write(data)
                count += 1
                break
    sys.stderr.write("%s: %d clips -> %s\n" % (base, count, out))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
