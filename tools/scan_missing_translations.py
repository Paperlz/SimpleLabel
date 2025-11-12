#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import codecs
import io
import os
import re

BASE_DIR = os.path.join(os.path.dirname(__file__), "..", "src")
TARGET_DIRS = [
	os.path.join(BASE_DIR, "panels"),
	os.path.join(BASE_DIR, "dialogs"),
]


TR_PATTERN = re.compile(
	r'tr\s*\(\s*((?:"(?:\\.|[^\"])*"\s*)+)\)',
	re.DOTALL,
)


def iter_tr_strings():
	for root in TARGET_DIRS:
		if not os.path.isdir(root):
			continue
		for dirpath, _, filenames in os.walk(root):
			for filename in filenames:
				if not filename.endswith((".cpp", ".h", ".ui", ".qml")):
					continue
				path = os.path.join(dirpath, filename)
				try:
					content = io.open(path, "r", encoding="utf-8").read()
				except UnicodeDecodeError:
					continue
				for match in TR_PATTERN.finditer(content):
					literal_block = match.group(1)
					segments = re.findall(r'"((?:\\.|[^\"])*)"', literal_block)
					if not segments:
						continue
					decoded_segments = []
					for seg in segments:
						try:
							decoded = codecs.decode(
								seg.encode("utf-8", "backslashreplace"),
								"unicode_escape",
							)
						except Exception:
							decoded = seg
						decoded_segments.append(decoded)
					text = "".join(decoded_segments)
					if not any("\u4e00" <= ch <= "\u9fff" for ch in text):
						continue
					yield text, path


def load_mapping():
	mapping = {}
	map_path = os.path.join(os.path.dirname(BASE_DIR), "translations", "en_us_map.tsv")
	with io.open(map_path, "r", encoding="utf-8") as fh:
		for line in fh:
			if "\t" not in line:
				continue
			src, dst = line.rstrip("\r\n").split("\t", 1)
			mapping.setdefault(src, dst)
			stripped = src.strip()
			if stripped and stripped != src:
				mapping.setdefault(stripped, dst)
	return mapping


def main():
	mapping = load_mapping()
	missing = {}
	for text, path in iter_tr_strings():
		if text in mapping:
			continue
		rel = os.path.relpath(path, BASE_DIR)
		missing.setdefault(text, set()).add(rel)
	if not missing:
		print("No missing translations found.")
		return
	for text in sorted(missing):
		print(text)
		for rel in sorted(missing[text]):
			print("  - {}".format(rel))
		print()


if __name__ == "__main__":
	main()

