#!/usr/bin/env python3
"""Convert Hexaverse DokuWiki-style documentation (documentation/documentation) to Markdown (docs/game)."""
import os, re, sys

SRC = 'documentation/documentation'
IMG = 'documentation/images'
OUT = 'docs/game'

def slug(path):
    return '/'.join(p.strip().lower().replace(' ', '-') for p in path.split('/'))

# collect pages
pages = {}
for root, _, files in os.walk(SRC):
    for f in files:
        base, ext = os.path.splitext(f)
        if ext not in ('.txt', '.prefix'):
            continue
        rel = os.path.relpath(os.path.join(root, base), SRC).replace(os.sep, '/')
        pages.setdefault(rel.lower(), {'rel': rel})[ext] = os.path.join(root, f)

images = {}
for root, _, files in os.walk(IMG):
    for f in files:
        rel = os.path.relpath(os.path.join(root, f), IMG).replace(os.sep, '/')
        images[rel.lower()] = rel

def out_path(key):
    rel = pages[key]['rel']
    if rel.lower() == 'home':
        return os.path.join(OUT, 'README.md')
    return os.path.join(OUT, slug(rel) + '.md')

missing = set()

# Fix broken/typo links found in the original wiki
ALIASES = {
    'concept/commands': 'concepts/commands',
    'concepts/attack commands': 'concepts/commands',
    'concepts/phase': 'concepts/round',
    'concepts/resource': 'concepts/resources',
    'config/save game': 'config/save games',
    'controls/command': 'concepts/commands',
    'units/turret/surveyor': 'units/turrets/surveyor',
    'units/turrets/canon': 'units/turrets/cannon',
    'units/vbase/heavy base': 'units/vbases/heavy base',
    'units/vbase/hoverbase': 'units/vbases/hoverbase',
}

def link_to(target, here_out):
    t = target.strip().lower()
    t = ALIASES.get(t, t)
    if t not in pages:
        missing.add(target)
        return None
    return os.path.relpath(out_path(t), os.path.dirname(here_out)).replace(os.sep, '/').replace(' ', '%20')

def img_to(path, here_out):
    p = path.strip()
    if p.lower().startswith('img:'):
        p = p[4:]
    p = p.split('?')[0].strip()
    real = images.get(p.lower())
    if not real:
        missing.add('image:' + p)
        real = p
    return os.path.relpath(os.path.join(IMG, real), os.path.dirname(here_out)).replace(os.sep, '/').replace(' ', '%20')

def inline(text, here_out):
    # protect links/images by tokens
    tokens = []
    def keep(s):
        tokens.append(s)
        return '\x00%d\x00' % (len(tokens) - 1)

    def img(m):
        inner = m.group(1)
        path, _, cap = inner.partition('|')
        url = img_to(path, here_out)
        alt = cap.strip() or os.path.splitext(os.path.basename(re.sub(r'^\s*img:', '', path).split('?')[0].strip()))[0]
        return keep('![%s](%s)' % (alt, url))
    text = re.sub(r'\{\{(.*?)\}\}', img, text)

    def lnk(m):
        target, _, label = m.group(1).partition('|')
        target = target.strip()
        if re.match(r'https?://', target):
            return keep('[%s](%s)' % (label.strip() or target, target))
        label = label.strip() or target.split('/')[-1].strip()
        url = link_to(target, here_out)
        return keep('[%s](%s)' % (label, url) if url else label)
    text = re.sub(r'\[\[(.*?)\]\]', lnk, text)

    def res(m):
        name = m.group(1)
        real = images.get((name + '.png').lower())
        if not real:
            return m.group(0)
        url = os.path.relpath(os.path.join(IMG, real), os.path.dirname(here_out)).replace(os.sep, '/').replace(' ', '%20')
        return keep('![%s](%s)' % (name.capitalize(), url))
    text = re.sub(r'%([A-Za-z]+)%', res, text)

    # monospace: ''x''
    def mono(m):
        inner = m.group(1)
        if '\x00' in inner:
            return '*' + inner.replace('\\', '\\\\') + '*'
        return '`' + inner + '`'
    text = re.sub(r"''(.+?)''", mono, text)
    # italic //x//  (avoid :// in leftover urls)
    text = re.sub(r'(?<!:)//(.+?)(?<!:)//', r'*\1*', text)

    # escape backslashes before markdown punctuation outside code spans
    parts = re.split(r'(`[^`]*`)', text)
    for k in range(0, len(parts), 2):
        parts[k] = re.sub(r'(?<!\\)\\(?=[*_\[\]`]|$)', r'\\\\', parts[k])
    text = ''.join(parts)
    for i in range(len(tokens) - 1, -1, -1):
        text = text.replace('\x00%d\x00' % i, tokens[i])
    return text

def convert(lines, here_out):
    out = []
    in_list = False
    for raw in lines:
        line = raw.rstrip('\n').rstrip()
        h = re.match(r'^\s*(=+)\s*(.*?)\s*=*\s*$', line)
        if h and line.strip().startswith('=') and len(h.group(2)) > 0:
            level = min(len(h.group(1)), 6)
            if out and out[-1] != '':
                out.append('')
            out.append('#' * level + ' ' + inline(h.group(2), here_out))
            out.append('')
            in_list = False
            continue
        if re.match(r'^\s*-{4,}\s*$', line):
            out += ['', '---', '']
            in_list = False
            continue
        l = re.match(r'^(\s*)([*#]+|-)\s*(\S.*)$', line)
        if l and not re.match(r'^\s*\*\*\S', line):
            marker = l.group(2)
            depth = len(marker) if marker != '-' else 1
            depth += len(l.group(1).replace('\t', '  ')) // 2 if marker != '-' and len(marker) == 1 and l.group(1) and depth == 1 and False else 0
            bullet = '1.' if marker.startswith('#') else '-'
            if not in_list and out and out[-1] != '':
                out.append('')
            out.append('   ' * (depth - 1) + bullet + ' ' + inline(l.group(3), here_out))
            in_list = True
            continue
        if line.strip() == '':
            out.append('')
            in_list = False
            continue
        if in_list:
            out.append('')
            in_list = False
        out.append(inline(line.strip(), here_out))
    # collapse blank lines
    res = []
    for x in out:
        if x == '' and (not res or res[-1] == ''):
            continue
        res.append(x)
    while res and res[-1] == '':
        res.pop()
    return '\n'.join(res) + '\n'

NOTE = '<!-- Generated from documentation/documentation/{src} (DokuWiki syntax). -->\n\n'

for key, p in sorted(pages.items()):
    dst = out_path(key)
    text = ''
    for ext in ('.prefix', '.txt'):
        if ext in p:
            with open(p[ext], encoding='utf-8', errors='replace') as fh:
                t = fh.read()
            if text and not text.endswith('\n'):
                text += '\n'
            text += t
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    md = convert(text.splitlines(), dst)
    srcs = ' + '.join(p['rel'] + e for e in ('.prefix', '.txt') if e in p)
    with open(dst, 'w', encoding='utf-8') as fh:
        fh.write(NOTE.format(src=srcs) + md)

if missing:
    print('unresolved:', sorted(missing), file=sys.stderr)
print(len(pages), 'pages')

# navigation footer + index of all pages
index_path = os.path.join(OUT, 'README.md')
groups = {}
for key, p in sorted(pages.items()):
    dst = out_path(key)
    if dst != index_path:
        with open(dst, 'a', encoding='utf-8') as fh:
            fh.write('\n---\n\n[Game documentation index](%s)\n' % os.path.relpath(index_path, os.path.dirname(dst)).replace(os.sep, '/'))
    title = None
    for line in open(dst, encoding='utf-8'):
        if line.startswith('# '):
            title = line[2:].strip(); break
    folder = os.path.dirname(os.path.relpath(dst, OUT)).replace(os.sep, '/')
    groups.setdefault(folder, []).append((title or key, os.path.relpath(dst, OUT).replace(os.sep, '/')))

NAMES = {'': 'General', 'concepts': 'Concepts', 'config': 'Configuration and tools',
         'units': 'Units', 'units/structures': 'Structures', 'units/turrets': 'Turrets',
         'units/vbases': 'Vehicle bases', 'units/planes': 'Planes'}
with open(index_path, 'a', encoding='utf-8') as fh:
    fh.write('\n---\n\n## All pages\n')
    for folder in ['', 'concepts', 'config', 'units', 'units/structures', 'units/turrets', 'units/vbases', 'units/planes']:
        items = [i for i in groups.get(folder, []) if i[1] != 'README.md']
        if not items:
            continue
        fh.write('\n### %s\n\n' % NAMES[folder])
        for title, rel in sorted(items, key=lambda x: x[0].lower()):
            fh.write('- [%s](%s)\n' % (title, rel.replace(' ', '%20')))
    leftovers = set(groups) - set(NAMES)
    assert not leftovers, leftovers
