#!/usr/bin/env python3
"""
Trainer for the strategic layer of the Hexaverse AI (lua/AI/significance.lua).

Reads AI logs of finished matches, learns to predict per region what the following rounds
showed, and writes the weights as a variant script the game can load.

What the network learns (one value per region, 0..1):
  hold    for a region I hold now: how much it gives me over the next rounds
          (cells held there, mines working there), and 0 if I lose it
  take    for a region I do not hold now: how much it gives me over the next rounds
          (so: regions that turn out worth having, weighted towards AIs that did well)
  danger  whether I lose units or structures there, or it turns hostile, in the next rounds

This is learning from what happened in recorded matches (the scripts played them). It
gives the layer a sensible start; self-play refines it afterwards.

Usage:
  python train_significance.py --logs <dir or files...> --out <lua/AI/gen_sig.lua>
      [--epochs 40] [--hidden 16,12] [--horizon 5] [--seed 1] [--init <weights.npz>]

Only needs numpy. Logs may contain several AIs (told apart by their log prefix).
"""
import argparse
import glob
import os
import re
import sys

import numpy as np

LINE = re.compile(r'([^\r\n:]{1,40}): (Region inputs|Region outputs|Region graph \(id:cells:neighbours\)|Regions r=(\d+)|Reward): ?(.*)')
TOTAL = re.compile(r'total (-?[\d.]+)')


def parse_log(path):
    """-> list of episodes: {names, cells[N], adj[N][N], steps[(round, holder[N], X[N,F])], total}"""
    eps = {}
    with open(path, 'r', encoding='latin-1', errors='replace') as f:
        for line in f:
            if 'Region' not in line and 'Reward' not in line:
                continue
            m = LINE.search(line)
            if not m:
                continue
            # the prefix is whatever precedes the keyword; keep its last token group
            prefix = m.group(1).strip().split('  ')[-1].split('\t')[-1]
            prefix = prefix.split('] ')[-1]
            e = eps.setdefault(prefix, {'names': None, 'graph': None, 'steps': [], 'total': 0.0})
            kind, body = m.group(2), m.group(4).strip()
            if kind == 'Region inputs':
                e['names'] = body.split(',')
            elif kind.startswith('Region graph'):
                g = {}
                for part in body.split(';'):
                    if not part:
                        continue
                    rid, cells, nb = (part.split(':') + ['', ''])[:3]
                    g[int(rid)] = (int(cells), [int(x) for x in nb.split(',') if x])
                e['graph'] = g
            elif kind == 'Reward':
                t = TOTAL.search(body)
                if t:
                    e['total'] = float(t.group(1))
            elif kind.startswith('Regions r='):
                rnd = int(m.group(3))
                rows = {}
                ok = True
                for part in body.split(';'):
                    bits = part.split(':')
                    if len(bits) != 4:
                        ok = False
                        break
                    try:
                        rows[int(bits[0])] = (bits[1], [float(x) for x in bits[3].split(',')])
                    except ValueError:
                        ok = False
                        break
                if ok and rows:
                    e['steps'].append((rnd, rows))
    out = []
    for prefix, e in eps.items():
        if not e['graph'] or not e['names'] or len(e['steps']) < 3:
            continue
        ids = sorted(e['graph'].keys())
        pos = {rid: i for i, rid in enumerate(ids)}
        n, fcount = len(ids), len(e['names'])
        adj = np.zeros((n, n), dtype=np.float32)
        for rid, (_, nbs) in e['graph'].items():
            nbs = [x for x in nbs if x in pos]
            for x in nbs:
                adj[pos[rid], pos[x]] = 1.0 / len(nbs)
        steps = []
        for rnd, rows in e['steps']:
            if set(rows.keys()) != set(ids):
                continue
            x = np.zeros((n, fcount), dtype=np.float32)
            holder = []
            bad = False
            for rid in ids:
                h, feats = rows[rid]
                if len(feats) != fcount:
                    bad = True
                    break
                x[pos[rid]] = feats
                holder.append(h)
            if not bad:
                steps.append((rnd, holder, x))
        if len(steps) >= 3:
            out.append({'source': path, 'prefix': prefix, 'names': e['names'], 'adj': adj,
                        'steps': steps, 'total': e['total']})
    return out


def targets(ep, horizon):
    """-> list of (X, A, Y[N,3], M[N,3]) for steps that have a future to look at"""
    names = ep['names']
    ix = {name: i for i, name in enumerate(names)}
    need = ['held', 'myMines', 'myStructures', 'deaths', 'staticThreat', 'hostileStructures', 'myMobiles']
    for k in need:
        if k not in ix:
            raise SystemExit('log lacks the input "%s" (old AI version?)' % k)
    steps = ep['steps']
    out = []
    for t in range(len(steps) - 1):
        fut = steps[t + 1:t + 1 + horizon]
        if not fut:
            break
        _, holder, x = steps[t]
        n = x.shape[0]
        mine_now = np.array([h == 'm' for h in holder])
        # what the region gives me afterwards: cells held and mines working, averaged
        gain = np.zeros(n, dtype=np.float32)
        kept = np.ones(n, dtype=np.float32)
        hurt = np.zeros(n, dtype=np.float32)
        prev = x
        for _, fh, fx in fut:
            gain += 0.6 * fx[:, ix['held']] + 0.4 * fx[:, ix['myMines']]
            f_mine = np.array([h == 'm' for h in fh])
            kept = np.minimum(kept, f_mine.astype(np.float32))
            lost = (fx[:, ix['deaths']] > prev[:, ix['deaths']] + 1e-6) \
                | (fx[:, ix['myStructures']] < prev[:, ix['myStructures']] - 1e-6) \
                | ((fx[:, ix['staticThreat']] > 0) & (prev[:, ix['staticThreat']] <= 0)) \
                | ((fx[:, ix['hostileStructures']] > 0) & (prev[:, ix['hostileStructures']] <= 0))
            hurt = np.maximum(hurt, lost.astype(np.float32))
            prev = fx
        gain /= len(fut)
        y = np.zeros((n, 3), dtype=np.float32)
        m = np.zeros((n, 3), dtype=np.float32)
        y[:, 0] = np.clip(gain * kept, 0, 1)
        m[:, 0] = mine_now
        y[:, 1] = np.clip(gain, 0, 1)
        m[:, 1] = ~mine_now
        y[:, 2] = hurt
        m[:, 2] = 1.0
        out.append((x, ep['adj'], y, m))
    return out


class Net:
    def __init__(self, sizes, rng):
        self.layers = []
        for a, b in zip(sizes[:-1], sizes[1:]):
            s = np.sqrt(1.0 / a)
            self.layers.append({'ws': rng.normal(0, s, (b, a)).astype(np.float32),
                                'wn': rng.normal(0, s, (b, a)).astype(np.float32),
                                'b': np.zeros(b, dtype=np.float32)})
        self.m = [{k: np.zeros_like(v) for k, v in l.items()} for l in self.layers]
        self.v = [{k: np.zeros_like(v) for k, v in l.items()} for l in self.layers]
        self.t = 0

    def forward(self, x, adj):
        cache = []
        h = x
        for i, l in enumerate(self.layers):
            ah = adj @ h
            z = h @ l['ws'].T + ah @ l['wn'].T + l['b']
            out = np.tanh(z) if i < len(self.layers) - 1 else 1.0 / (1.0 + np.exp(-z))
            cache.append((h, ah, out))
            h = out
        return h, cache

    def backward(self, adj, cache, dz_last):
        grads = [None] * len(self.layers)
        g = dz_last
        for i in range(len(self.layers) - 1, -1, -1):
            l = self.layers[i]
            h, ah, out = cache[i]
            if i < len(self.layers) - 1:
                g = g * (1.0 - out * out)
            grads[i] = {'ws': g.T @ h, 'wn': g.T @ ah, 'b': g.sum(axis=0)}
            g = g @ l['ws'] + adj.T @ (g @ l['wn'])
        return grads

    def step(self, grads, lr, decay):
        self.t += 1
        b1, b2 = 0.9, 0.999
        for l, g, m, v in zip(self.layers, grads, self.m, self.v):
            for k in l:
                gk = g[k] + (decay * l[k] if k != 'b' else 0)
                m[k] = b1 * m[k] + (1 - b1) * gk
                v[k] = b2 * v[k] + (1 - b2) * gk * gk
                mh = m[k] / (1 - b1 ** self.t)
                vh = v[k] / (1 - b2 ** self.t)
                l[k] -= lr * mh / (np.sqrt(vh) + 1e-8)


def loss_and_grad(pred, y, m, weight):
    p = np.clip(pred, 1e-6, 1 - 1e-6)
    count = max(1.0, float(m.sum()))
    loss = -(m * (y * np.log(p) + (1 - y) * np.log(1 - p))).sum() / count
    dz = weight * m * (pred - y) / count      # logistic output with cross-entropy
    return loss, dz


def write_lua(net, names, path, note):
    def rows(w):
        return '{' + ','.join('{' + ','.join('%.4g' % x for x in r) + '}' for r in w) + '}'
    parts = []
    for l in net.layers:
        parts.append('\t{ws=%s,\n\t wn=%s,\n\t b={%s}}' % (rows(l['ws']), rows(l['wn']), ','.join('%.4g' % x for x in l['b'])))
    name = os.path.splitext(os.path.basename(path))[0]
    with open(path, 'w', encoding='latin-1', newline='\n') as f:
        f.write('--[[\nStrategic layer weights, written by tools/ai_train/train_significance.py\n%s\nInputs: %s\n]]\n' % (note, ','.join(names)))
        f.write('--#require "player.lua"\n')
        f.write("Cfg.logPrefix = '%s: ';\n" % name)
        f.write('Significance.net = {layers = {\n' + ',\n'.join(parts) + '\n}};\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--logs', nargs='+', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--epochs', type=int, default=40)
    ap.add_argument('--hidden', default='16,12')
    ap.add_argument('--horizon', type=int, default=5, help='evaluations to look ahead (x10 rounds)')
    ap.add_argument('--lr', type=float, default=0.003)
    ap.add_argument('--decay', type=float, default=1e-4)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--npz', default=None, help='also save the weights here (to continue later)')
    ap.add_argument('--init', default=None)
    args = ap.parse_args()

    files = []
    for p in args.logs:
        if os.path.isdir(p):
            for root, _, fs in os.walk(p):
                files += [os.path.join(root, f) for f in fs if f.lower().endswith(('.log', '.txt'))]
        else:
            files += glob.glob(p) or [p]
    episodes = []
    for f in sorted(files):
        try:
            episodes += parse_log(f)
        except OSError as e:
            print('skipping %s: %s' % (f, e), file=sys.stderr)
    if not episodes:
        raise SystemExit('no usable "Regions r=" data in %d file(s)' % len(files))
    names = episodes[0]['names']
    episodes = [e for e in episodes if e['names'] == names]

    # AIs that did well count more (their expansion is the better example)
    totals = np.array([e['total'] for e in episodes], dtype=np.float32)
    lo, hi = float(totals.min()), float(totals.max())
    rng = np.random.default_rng(args.seed)
    order = rng.permutation(len(episodes))
    n_val = max(1, len(episodes) // 6) if len(episodes) >= 6 else 0
    val_ids = set(order[:n_val].tolist())
    train, val = [], []
    for i, e in enumerate(episodes):
        w = 0.5 + ((e['total'] - lo) / (hi - lo) if hi > lo else 0.5)
        for s in targets(e, args.horizon):
            (val if i in val_ids else train).append(s + (w,))
    print('%d AI logs, %d training samples, %d held back for checking, %d inputs, %d regions in the first'
          % (len(episodes), len(train), len(val), len(names), episodes[0]['adj'].shape[0]))

    sizes = [len(names)] + [int(x) for x in args.hidden.split(',') if x] + [3]
    net = Net(sizes, rng)
    if args.init:
        z = np.load(args.init)
        for i, l in enumerate(net.layers):
            for k in l:
                l[k] = z['%d_%s' % (i, k)]

    def evaluate(samples):
        if not samples:
            return float('nan'), [float('nan')] * 3, [float('nan')] * 3
        tot, err, base, cnt = 0.0, np.zeros(3), np.zeros(3), np.zeros(3)
        ymean = np.zeros(3)
        ycnt = np.zeros(3)
        for x, a, y, m, w in samples:
            ymean += (y * m).sum(axis=0)
            ycnt += m.sum(axis=0)
        ymean /= np.maximum(1, ycnt)
        for x, a, y, m, w in samples:
            p, _ = net.forward(x, a)
            l, _ = loss_and_grad(p, y, m, 1.0)
            tot += l
            err += (m * np.abs(p - y)).sum(axis=0)
            base += (m * np.abs(ymean - y)).sum(axis=0)
            cnt += m.sum(axis=0)
        return tot / len(samples), (err / np.maximum(1, cnt)).tolist(), (base / np.maximum(1, cnt)).tolist()

    for epoch in range(1, args.epochs + 1):
        total = 0.0
        for i in rng.permutation(len(train)):
            x, a, y, m, w = train[i]
            p, cache = net.forward(x, a)
            l, dz = loss_and_grad(p, y, m, w)
            total += l
            net.step(net.backward(a, cache, dz), args.lr, args.decay)
        if epoch == 1 or epoch % 5 == 0 or epoch == args.epochs:
            vl, ve, vb = evaluate(val)
            print('epoch %3d  loss %.4f  check loss %.4f  error hold/take/danger %.3f/%.3f/%.3f  (always guessing the average: %.3f/%.3f/%.3f)'
                  % (epoch, total / max(1, len(train)), vl, ve[0], ve[1], ve[2], vb[0], vb[1], vb[2]))

    note = 'Trained on %d AI logs, %d samples, %d epochs, horizon %d evaluations.' % (len(episodes), len(train), args.epochs, args.horizon)
    write_lua(net, names, args.out, note)
    if args.npz:
        np.savez(args.npz, **{'%d_%s' % (i, k): v for i, l in enumerate(net.layers) for k, v in l.items()})
    print('wrote', args.out)


if __name__ == '__main__':
    main()
