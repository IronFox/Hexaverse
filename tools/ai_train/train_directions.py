#!/usr/bin/env python3
"""
Trains the strategic layer of the Hexaverse AI (lua/AI/significance.lua) on what its
directions were worth.

The measure is the game's overall score (the "Stats:" lines of the server log). The score
mostly only rises, so the signal is in its speed: a direction after which the score rose
faster than before was good, one after which it rose more slowly was bad. Ports are left at
their full weight in the score (they decide the game, so they are meant to dominate).

For every direction an AI issued ("Direction: take region 12 ...", some of them explored at
random) the trainer takes the picture of the map at that moment and the change of the score's
speed afterwards, and teaches the network: for a region like this, in a situation like this,
"take" (or "hold") was worth this much. The danger output keeps its old meaning (losses there
in the following rounds).

Input: match folders as the duel runner writes them (ai1.log.gz, ai2.log.gz, server.log.gz).

Usage:
  python train_directions.py --matches <dir with m000, m001, ...> [more dirs] --out <lua/AI/gen_sig.lua>
      [--window 30] [--versus 0.5] [--epochs 60] [--hidden 16,12] [--init weights.npz] [--npz weights.npz]

Only needs numpy.
"""
import argparse
import os
import re
import sys

import numpy as np

from train_significance import Net, open_log, parse_log, targets, write_lua

STATS = re.compile(r'Stats: ([0-9a-f]{8}) round=(\d+) score=(\d+)')
FACTION = re.compile(r"([0-9a-f]{8}):'AI (\d)'")
DIRECTION = re.compile(r'Direction: (take|hold) region (\d+) priority [\d.,]+ \(round (\d+)\)( explored)?')
COLUMN = {'hold': 0, 'take': 1}


def read_scores(path):
    """-> {AI number: {round: score}}"""
    by_id, names = {}, {}
    with open_log(path) as f:
        for line in f:
            if "'AI " in line:
                for fid, n in FACTION.findall(line):
                    names[fid] = int(n)
            if 'Stats: ' in line:
                m = STATS.search(line)
                if m:
                    by_id.setdefault(m.group(1), {})[int(m.group(2))] = int(m.group(3))
    return {names[fid]: rounds for fid, rounds in by_id.items() if fid in names}


def read_directions(path):
    """-> [(kind, region, round, explored)]"""
    out = []
    with open_log(path) as f:
        for line in f:
            if 'Direction: ' in line:
                m = DIRECTION.search(line)
                if m:
                    out.append((m.group(1), int(m.group(2)), int(m.group(3)), bool(m.group(4))))
    return out


def speed_change(score, at, window, last):
    """Score per round over the window after `at`, minus the same before it. None if the match
    ended too soon to tell. A faction that is gone keeps its last score (it earns nothing more)."""
    def value(r):
        r = max(0, r)
        while r > 0 and r not in score:
            r -= 1
        return score.get(r, 0)
    end = min(at + window, last)
    if end - at < window // 2:
        return None
    start = max(0, at - window)
    after = (value(end) - value(at)) / float(end - at)
    before = (value(at) - value(start)) / float(at - start) if at > start else 0.0
    return after - before


def match_samples(folder, window, versus, horizon):
    """-> list of (X, A, Y[N,3], M[N,3], signals) for one match, both AIs"""
    server = os.path.join(folder, 'server.log.gz')
    if not os.path.exists(server):
        return [], None
    scores = read_scores(server)
    names = None
    out = []
    for n in (1, 2):
        log = os.path.join(folder, 'ai%d.log.gz' % n)
        if not os.path.exists(log) or n not in scores:
            continue
        eps = parse_log(log)
        if not eps:
            continue
        ep = max(eps, key=lambda e: len(e['steps']))
        names = ep['names']
        mine, other = scores[n], scores.get(3 - n, {})
        last = max(mine) if mine else 0
        # the danger output keeps its target; hold and take get theirs from the directions
        danger = {}
        tg = targets(ep, horizon)
        for (rnd, _, _), (x, a, y, m) in zip(ep['steps'], tg):
            danger[rnd] = (x, a, y, m)
        ids = None
        with open_log(log) as f:
            for line in f:
                if 'Region graph (id:cells:neighbours): ' in line:
                    body = line.split('Region graph (id:cells:neighbours): ', 1)[1]
                    ids = sorted(int(p.split(':')[0]) for p in body.strip().split(';') if p)
                    break
        if ids is None:
            continue
        pos = {rid: i for i, rid in enumerate(ids)}
        per_round = {}
        for kind, region, rnd, explored in read_directions(log):
            if rnd not in danger or region not in pos:
                continue
            s = speed_change(mine, rnd, window, last)
            if s is None:
                continue
            if versus > 0 and other:
                o = speed_change(other, rnd, window, max(max(other), last))
                if o is not None:
                    s -= versus * o
            per_round.setdefault(rnd, []).append((COLUMN[kind], pos[region], s, explored))
        for rnd, items in per_round.items():
            x, a, y, m = danger[rnd]
            y, m = y.copy(), m.copy()
            m[:, 0:2] = 0
            out.append((x, a, y, m, items))
    return out, names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--matches', nargs='+', required=True, help='folders holding match folders (m000, ...)')
    ap.add_argument('--out', required=True)
    ap.add_argument('--window', type=int, default=30, help='rounds before and after a direction to compare the score speed over')
    ap.add_argument('--versus', type=float, default=0.5, help='how much of the opponent\'s speed change is subtracted (0 = own score only)')
    ap.add_argument('--horizon', type=int, default=5, help='evaluations to look ahead for the danger output')
    ap.add_argument('--epochs', type=int, default=60)
    ap.add_argument('--hidden', default='16,12')
    ap.add_argument('--lr', type=float, default=0.002)
    ap.add_argument('--decay', type=float, default=1e-4)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--npz', default=None)
    ap.add_argument('--init', default=None)
    args = ap.parse_args()

    folders = []
    for top in args.matches:
        if os.path.exists(os.path.join(top, 'server.log.gz')):
            folders.append(top)
        else:
            folders += [os.path.join(top, d) for d in sorted(os.listdir(top)) if os.path.exists(os.path.join(top, d, 'server.log.gz'))]
    rng = np.random.default_rng(args.seed)
    order = rng.permutation(len(folders))
    held = set(order[:max(1, len(folders) // 6)].tolist()) if len(folders) >= 6 else set()
    train, val, names = [], [], None
    for i, folder in enumerate(folders):
        try:
            samples, nm = match_samples(folder, args.window, args.versus, args.horizon)
        except (OSError, EOFError, ValueError) as e:
            print('skipping %s: %s' % (folder, e), file=sys.stderr)
            continue
        if nm is None or (names is not None and nm != names):
            continue
        names = nm
        (val if i in held else train).extend(samples)
    if not train:
        raise SystemExit('no directions with a score to judge them by in %d match folder(s)' % len(folders))

    # the scale of the signal: half of the directions change the speed by less than this
    signals = np.array([s for _, _, _, _, items in train for _, _, s, _ in items], dtype=np.float64)
    scale = float(np.median(np.abs(signals[signals != 0]))) if np.any(signals != 0) else 1.0
    explored = sum(1 for _, _, _, _, items in train for it in items if it[3])
    print('%d matches (%d held back), %d directions to learn from (%d explored), %d inputs' % (len(folders), len(held), len(signals), explored, len(names)))
    print('score speed change after a direction: median %+.1f, quarter below %+.1f, quarter above %+.1f, scale %.1f per round'
          % (np.median(signals), np.percentile(signals, 25), np.percentile(signals, 75), scale))

    def finish(samples):
        out = []
        for x, a, y, m, items in samples:
            y, m = y.copy(), m.copy()
            for col, row, s, _ in items:
                y[row, col] = 1.0 / (1.0 + np.exp(-np.clip(s / scale, -30, 30)))
                m[row, col] = 1.0
            out.append((x, a, y, m))
        return out
    train, val = finish(train), finish(val)

    sizes = [len(names)] + [int(x) for x in args.hidden.split(',') if x] + [3]
    net = Net(sizes, rng)
    if args.init:
        z = np.load(args.init)
        for i, l in enumerate(net.layers):
            for k in l:
                l[k] = z['%d_%s' % (i, k)]

    def grad(pred, y, m):
        # each output on its own scale: a few directions must not drown in the danger of every region
        count = np.maximum(1.0, m.sum(axis=0))
        return m * (pred - y) / count

    def check(samples):
        """-> per output: mean error, error of always guessing the average, correlation"""
        res = []
        for col in range(3):
            p, t = [], []
            for x, a, y, m in samples:
                pred, _ = net.forward(x, a)
                sel = m[:, col] > 0
                p += pred[sel, col].tolist()
                t += y[sel, col].tolist()
            if len(t) < 3:
                res.append((float('nan'),) * 3 + (len(t),))
                continue
            p, t = np.array(p), np.array(t)
            corr = float(np.corrcoef(p, t)[0, 1]) if p.std() > 0 and t.std() > 0 else 0.0
            res.append((float(np.abs(p - t).mean()), float(np.abs(t.mean() - t).mean()), corr, len(t)))
        return res

    for epoch in range(1, args.epochs + 1):
        for i in rng.permutation(len(train)):
            x, a, y, m = train[i]
            p, cache = net.forward(x, a)
            net.step(net.backward(a, cache, grad(p, y, m)), args.lr, args.decay)
        if epoch == 1 or epoch % 10 == 0 or epoch == args.epochs:
            line = 'epoch %3d' % epoch
            for label, samples in (('training', train), ('held back', val)):
                if samples:
                    r = check(samples)
                    line += '  %s: ' % label + ', '.join('%s error %.3f (average guess %.3f) corr %+.2f n=%d' % ((nm,) + r[c])
                                                           for c, nm in enumerate(('hold', 'take', 'danger')))
            print(line)

    note = ('Trained on the score speed after %d directions (%d explored) of %d matches, window %d rounds, opponent weight %.2f.'
            % (len(signals), explored, len(folders), args.window, args.versus))
    write_lua(net, names, args.out, note)
    with open(args.out, 'a', encoding='latin-1', newline='\n') as f:
        f.write('Cfg.useDirections = true;\n')
    if args.npz:
        np.savez(args.npz, **{'%d_%s' % (i, k): v for i, l in enumerate(net.layers) for k, v in l.items()})
    print('wrote', args.out)


if __name__ == '__main__':
    main()
