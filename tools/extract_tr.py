import pathlib, re

root = pathlib.Path("src")
pattern = re.compile(r'tr\("([^"]+)"\)')  

strings = set()
for p in list(root.rglob("*.cpp")) + list(root.rglob("*.h")):
    try:
        t = p.read_text(encoding="utf-8", errors="ignore")
    except Exception:
        continue
    for m in pattern.finditer(t):
        strings.add(m.group(1))


for s in sorted(strings):
    print(s)
