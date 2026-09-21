# Declared-but-never-read properties: what a creator can set that nothing anywhere reads.
#
# The dangerous gap is not a missing API that errors -- you find that the first time you
# call it. It is a present one that silently does nothing: you set it, see no effect, and
# cannot tell whether you used it wrong or it was never wired up.
#
# rbx_instance.cpp declares classes two ways, and an earlier version of this script only
# understood one of them:
#
#     auto& bp = r.add("BasePart", "Instance", false);
#     bp.props = { P("Position", ...), ... };
#
#     r.add("EqualizerSoundEffect", "SoundEffect").props = { P("HighGain", ...), ... };
#
# Missing the second form hid every sound effect class -- nine of them, none implemented --
# and made the total look smaller than it was. If this ever reports a number that seems
# reassuring, check it understands how the thing it is counting is written.
import io, re, glob, collections, os

schema = io.open('src/rbx_instance.cpp', encoding='utf-8').read()

def block_at(text, brace_index):
    """The {...} starting at brace_index, as a string."""
    depth, j = 0, brace_index
    while j < len(text):
        if text[j] == '{':
            depth += 1
        elif text[j] == '}':
            depth -= 1
            if depth == 0:
                return text[brace_index:j]
        j += 1
    return ''

declared = []

# form 1: a variable, assigned later
var_class = {}
for m in re.finditer(r'auto&\s+(\w+)\s*=\s*r\.add\("([A-Za-z0-9_]+)"', schema):
    var_class[m.group(1)] = m.group(2)
for m in re.finditer(r'(\w+)\.props\s*=\s*\{', schema):
    cls = var_class.get(m.group(1))
    if not cls:
        continue
    for pm in re.finditer(r'\bP(?:Enum)?\("([A-Za-z0-9_]+)"', block_at(schema, m.end() - 1)):
        declared.append((cls, pm.group(1)))

# form 2: declared and assigned in one expression
for m in re.finditer(r'r\.add\("([A-Za-z0-9_]+)"[^;]*?\)\.props\s*=\s*\{', schema):
    cls = m.group(1)
    for pm in re.finditer(r'\bP(?:Enum)?\("([A-Za-z0-9_]+)"', block_at(schema, m.end() - 1)):
        declared.append((cls, pm.group(1)))

# ---- how a property gets read -----------------------------------------------------
#
# Usually by name: `c.name == "Transparency"`. But a family of properties whose names differ
# only by a prefix is read by its SUFFIX, one branch for all of them:
#
#     n.compare(n.size() - 6, 6, "ParamB")
#
# and searching for the literal "BackParamB" finds nothing. That made this report twelve
# BasePart properties as dead when all twelve were implemented -- SurfaceInput and ParamB
# both drive the legacy Rotate joints, and have since before this script existed.
#
# An audit that overcounts is exactly as useless as one that undercounts: the point of the
# number is that somebody acts on it, and twelve false entries is twelve pieces of work
# somebody does twice. So: collect the suffixes the code compares against, and count a
# property as read if its name ends with one of them.
SUFFIX_IDIOMS = [
    r'compare\(\s*\w+\.size\(\)\s*-\s*\d+\s*,\s*\d+\s*,\s*"([A-Za-z0-9_]+)"\s*\)',
    r'\bends_with\(\s*"([A-Za-z0-9_]+)"\s*\)',
]

# Anything that could read a property, minus the declarations themselves.
blob = ''
for pat in ('src/*.cpp', 'src/*.h', 'gdextension/src/*.cpp', 'gdextension/src/*.h'):
    for f in glob.glob(pat):
        text = io.open(f, encoding='utf-8', errors='ignore').read()
        if f.endswith('rbx_instance.cpp'):
            text = re.sub(r'\bP(?:Enum)?\("[A-Za-z0-9_]+"', '', text)
        blob += '\n' + text

suffixes = set()
for pat in SUFFIX_IDIOMS:
    for m in re.finditer(pat, blob):
        suffixes.add(m.group(1))

def is_read(prop):
    if re.search(r'"%s"' % re.escape(prop), blob):
        return True
    # Read as one of a family: TopParamB, BottomParamB and the rest are one branch.
    return any(prop != suf and prop.endswith(suf) for suf in suffixes)

seen, dead = set(), []
for cls, prop in declared:
    if (cls, prop) in seen:
        continue
    seen.add((cls, prop))
    if not is_read(prop):
        dead.append((cls, prop))

by_class = collections.defaultdict(list)
for cls, prop in dead:
    by_class[cls].append(prop)

print('declared %d, never read %d  (%d suffix families understood)\n' % (len(seen), len(dead), len(suffixes)))
for cls in sorted(by_class, key=lambda c: (-len(by_class[c]), c)):
    print('  %-26s %s' % (cls, ', '.join(sorted(by_class[cls]))))
