"""Populate the English Qt translation file using the TSV mapping."""

import argparse
import codecs
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


ROOT = Path(__file__).resolve().parent
MAP_FILE = ROOT / "en_us_map.tsv"
TS_FILE = ROOT / "SimpleLabel_en_US.ts"


def load_mapping(path: Path) -> Dict[str, str]:
	mapping: Dict[str, str] = {}
	if not path.exists():
		raise FileNotFoundError(f"Mapping file not found: {path}")

	for lineno, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
		if not raw_line.strip():
			continue
		stripped = raw_line.lstrip()
		if stripped.startswith("#"):
			continue
		if "\t" not in raw_line:
			raise ValueError(f"Line {lineno} in {path} is missing a TAB separator: {raw_line!r}")
		source_raw, translation_raw = raw_line.split("\t", 1)
		source_raw = source_raw.lstrip("\ufeff ")
		source_raw = source_raw.rstrip("\r\n")
		translation_raw = translation_raw.lstrip()
		translation_raw = translation_raw.rstrip("\r\n")

		# Support escaped control characters such as \n or \t in the TSV file
		source_raw = source_raw.replace(r"\n", "\n").replace(r"\t", "\t")
		translation_raw = translation_raw.replace(r"\n", "\n").replace(r"\t", "\t")

		def decode_value(value: str) -> str:
			if "\\" in value:
				return codecs.decode(value.encode("utf-8"), "unicode_escape")
			return value

		source = decode_value(source_raw)
		translation = decode_value(translation_raw)
		mapping[source] = translation
	return mapping


def indent(elem: ET.Element, level: int = 0) -> None:
	spaces = "  "
	i = "\n" + level * spaces
	if len(elem):
		if not elem.text or not elem.text.strip():
			elem.text = i + spaces
		last_child: Optional[ET.Element] = None
		for child in elem:
			indent(child, level + 1)
			if not child.tail or not child.tail.strip():
				child.tail = i + spaces
			last_child = child
		if last_child is not None and (not last_child.tail or not last_child.tail.strip()):
			last_child.tail = i
	if level and (not elem.tail or not elem.tail.strip()):
		elem.tail = "\n" + (level - 1) * spaces


def apply_mapping(ts_path: Path, mapping: Dict[str, str]) -> Tuple[bool, Set[str]]:
	if not ts_path.exists():
		raise FileNotFoundError(f"Translation file not found: {ts_path}")

	tree = ET.parse(ts_path)
	root = tree.getroot()
	changed = False
	missing: Set[str] = set()

	for message in root.iter("message"):
		source_elem = message.find("source")
		if source_elem is None:
			continue
		source_text = source_elem.text or ""
		translation_elem = message.find("translation")
		if translation_elem is None:
			translation_elem = ET.SubElement(message, "translation")

		current_text = (translation_elem.text or "").strip()
		if source_text in mapping:
			new_text = mapping[source_text]
			if translation_elem.get("type") == "unfinished" or translation_elem.text != new_text:
				translation_elem.text = new_text
				translation_elem.attrib.pop("type", None)
				changed = True
		else:
			if current_text:
				translation_elem.attrib.pop("type", None)
			else:
				translation_elem.set("type", "unfinished")
				missing.add(source_text)

	if changed:
		indent(root)
		tree.write(ts_path, encoding="utf-8", xml_declaration=True)

	return changed, missing


def main(argv: Optional[List[str]] = None) -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--ts", type=Path, default=TS_FILE, help="Path to the target .ts file")
	parser.add_argument("--map", type=Path, default=MAP_FILE, help="Path to the TSV mapping file")
	args = parser.parse_args(argv)

	mapping = load_mapping(args.map)
	changed, missing = apply_mapping(args.ts, mapping)

	status = "updated" if changed else "up to date"
	print(f"{args.ts} is {status}; {len(mapping)} mapped strings.")

	if missing:
		print(f"{len(missing)} source strings missing from mapping:")
		for text in sorted(missing):
			print(f"  {text}")
		return 1

	return 0


if __name__ == "__main__":
	sys.exit(main())
