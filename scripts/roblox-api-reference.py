# Roblox's engine API, as its own documentation declares it, reduced to names.
#
#   python scripts/roblox-api-reference.py [docs dir]
#
# Reads the class reference YAML from Roblox/creator-docs (content/en-us/reference/engine/classes)
# and writes scripts/roblox-api.json: every class with its superclass, tags, and its OWN members --
# properties, methods, events, callbacks -- each with whether it is deprecated and its write
# security. Descriptions are not kept; this is a checklist, not a copy of the docs.
#
# If no docs dir is given, the files are fetched from GitHub first.
#
# tests/api_parity.gd holds the kit's runtime to this list. The point is to stop finding gaps
# one at a time by tripping over them: "we have parity" is a number now, and so is how far off
# it is.
import io, json, os, re, sys, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "roblox-api.json")
RAW = "https://raw.githubusercontent.com/Roblox/creator-docs/main/"

def fetch_all(dest):
    os.makedirs(dest, exist_ok=True)
    tree = json.load(urllib.request.urlopen("https://api.github.com/repos/Roblox/creator-docs/git/trees/main?recursive=1"))
    paths = [e["path"] for e in tree["tree"]
             if e["path"].startswith("content/en-us/reference/engine/classes/") and e["path"].endswith(".yaml")]
    for p in paths:
        target = os.path.join(dest, os.path.basename(p))
        if not os.path.exists(target):
            with urllib.request.urlopen(RAW + p) as r, open(target, "wb") as f:
                f.write(r.read())
    return dest

def section(text, key):
    """The lines of a top-level YAML list (properties:, methods: ...), as raw text."""
    m = re.search(r"^" + key + r":\s*(\[\])?\s*$(.*?)(?=^[a-z_]+:|\Z)", text, re.S | re.M)
    if not m or m.group(1): return ""
    return m.group(2)

def members(text, key):
    out = []
    body = section(text, key)
    for m in re.finditer(r"^  - name: (.+?)\n(.*?)(?=^  - name: |\Z)", body, re.S | re.M):
        full = m.group(1).strip()
        name = re.split(r"[.:]", full)[-1]
        block = m.group(2)
        dep = re.search(r"^    deprecation_message: (.*)$", block, re.M)
        deprecated = bool(dep and dep.group(1).strip() not in ("''", '""', ""))
        tags = re.findall(r"^      - (\w+)$", block[: block.find("deprecation_message")] if "deprecation_message" in block else block, re.M)
        if "Deprecated" in tags: deprecated = True
        write = re.search(r"^      write: (\w+)", block, re.M)
        out.append({"name": name, "deprecated": deprecated, "write": write.group(1) if write else None})
    return out

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else fetch_all(os.path.join(HERE, ".roblox-docs"))
    api = {}
    for fn in sorted(os.listdir(src)):
        if not fn.endswith(".yaml"): continue
        text = io.open(os.path.join(src, fn), encoding="utf-8").read()
        name = re.search(r"^name: (\S+)", text, re.M).group(1)
        inherits = re.search(r"^inherits:\s*\n\s*- (\S+)", text, re.M)
        tags = re.findall(r"^  - (\w+)$", section(text, "tags")) if section(text, "tags") else []
        dep = re.search(r"^deprecation_message: (.*)$", text, re.M)
        api[name] = {
            "inherits": inherits.group(1) if inherits else None,
            "tags": tags,
            "deprecated": bool(dep and dep.group(1).strip() not in ("''", '""', "")) or "Deprecated" in tags,
            "properties": members(text, "properties"),
            "methods": members(text, "methods"),
            "events": members(text, "events"),
            "callbacks": members(text, "callbacks"),
        }
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(api, f, indent=1, sort_keys=True)
    n = lambda k: sum(len(c[k]) for c in api.values())
    print(f"{len(api)} classes, {n('properties')} properties, {n('methods')} methods, {n('events')} events, {n('callbacks')} callbacks -> {OUT}")

main()
