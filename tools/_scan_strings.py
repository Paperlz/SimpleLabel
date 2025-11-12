import io
import os
import re

FILES = [
    r'd:/Projects/FOLabel/src/panels/labelpropswidget.cpp',
    r'd:/Projects/FOLabel/src/panels/databaseprintwidget.cpp',
    r'd:/Projects/FOLabel/src/dialogs/printcenterdialog.cpp',
    r'd:/Projects/FOLabel/src/dialogs/templatecenterdialog.cpp',
]

pattern = re.compile(r'tr\("([^\"]*[\u4e00-\u9fff][^\"]*)"\)')

for path in FILES:
    if not os.path.exists(path):
        print('Missing: {}'.format(path))
        continue
    with io.open(path, 'r', encoding='utf-8') as fh:
        content = fh.read()
    matches = pattern.findall(content)
    if not matches:
        continue
    print('[{}]'.format(os.path.basename(path)))
    seen = set()
    for text in matches:
        if text not in seen:
            seen.add(text)
            print(text)
    print()
