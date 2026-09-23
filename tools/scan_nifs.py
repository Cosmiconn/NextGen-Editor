"""Inventory loose/nested ZIP NIFs, persist each result, hard-kill timed-out probes.

Each distinct SHA-256 is parsed once; inventory.tsv retains EVERY occurrence.
Standard and recovery results are separate; recovery never hides a standard error.
"""
import argparse
import collections
import concurrent.futures
import csv
import hashlib
import io
import json
import re
import struct
import subprocess
import shutil
import tempfile
import time
import zipfile
from pathlib import Path

FIELDS = ['sha256', 'archive', 'filename', 'version', 'status', 'parts', 'vertices',
          'triangles', 'textures', 'blockindex', 'blocktype', 'error', 'seconds', 'warnings']


def writer(path, fields):
    f = path.open('w', newline='', encoding='utf-8')
    w = csv.DictWriter(f, fieldnames=fields, delimiter='\t')
    w.writeheader()
    f.flush()
    return f, w


def inventory(roots, out):
    cache = out / 'inputs'
    cache.mkdir(parents=True, exist_ok=True)
    f, w = writer(out / 'inventory.tsv', ['sha256', 'archive', 'filename', 'version'])
    af, aw = writer(out / 'archives.tsv', ['archive', 'status', 'error'])
    unique = {}
    count = 0

    def add(data, archive, name):
        nonlocal count
        sha = hashlib.sha256(data).hexdigest()
        off = data.find(b'\n') + 1
        version = '.'.join(map(str, struct.pack('>I', struct.unpack_from('<I', data, off)[0]))) if off and off + 4 <= len(data) else 'UNKNOWN'
        row = dict(sha256=sha, archive=archive, filename=name, version=version)
        if sha not in unique:
            (cache / (sha + '.nif')).write_bytes(data)
            unique[sha] = row
        w.writerow(row)
        f.flush()
        count += 1

    def archive(source, label):
        try:
            with zipfile.ZipFile(source) as z:
                for entry in z.infolist():
                    if entry.is_dir():
                        continue
                    suffix = Path(entry.filename).suffix.lower()
                    if suffix not in ('.nif', '.zip'):
                        continue
                    try:
                        if suffix == '.nif':
                            add(z.read(entry), label, entry.filename)  # verifies CRC
                        else:
                            # Client ZIPs contain multi-GB nested archives. Stream to disk.
                            with tempfile.TemporaryFile(dir=out) as tmp:
                                with z.open(entry) as src:
                                    shutil.copyfileobj(src, tmp, 1024 * 1024)
                                tmp.seek(0)
                                archive(tmp, label + '!' + entry.filename)
                    except (zipfile.BadZipFile, RuntimeError, OSError, EOFError) as e:
                        aw.writerow(dict(archive=label + '!' + entry.filename, status='UNREADABLE_ENTRY', error=str(e)))
                        af.flush()
                aw.writerow(dict(archive=label, status='READABLE', error=''))
        except (zipfile.BadZipFile, RuntimeError, OSError, EOFError) as e:
            aw.writerow(dict(archive=label, status='ARCHIVE_NOT_TESTABLE', error=str(e)))
        af.flush()

    seen = set()
    for root in roots:
        root = Path(root).resolve()
        files = sorted(root.rglob('*')) if root.is_dir() else [root]
        for p in files:
            if not p.is_file() or p.suffix.lower() not in ('.nif', '.zip') or p in seen:
                continue
            seen.add(p)
            if p.suffix.lower() == '.zip':
                archive(p, str(p))
            else:
                add(p.read_bytes(), str(root), str(p.relative_to(root)) if root.is_dir() else p.name)
    f.close()
    af.close()
    print(f'Inventory: {count} occurrences, {len(unique)} distinct SHA-256 inputs', flush=True)
    return list(unique.values())


def probe(exe, cache, row, mode, timeout):
    result = {k: '' for k in FIELDS}
    result.update(row)
    start = time.monotonic()
    try:
        p = subprocess.run([str(exe), '--' + mode, str(cache / (row['sha256'] + '.nif'))],
                           capture_output=True, timeout=timeout)
        def decode(data):
            try:
                return data.decode('utf-8')
            except UnicodeDecodeError:
                return data.decode('cp1252', errors='replace')
        p.stdout, p.stderr = decode(p.stdout), decode(p.stderr)
        result['warnings'] = p.stderr.strip().replace('\r', ' ').replace('\n', ' ').replace('\t', ' ')
        columns = p.stdout.strip('\r\n').split('\t', 5)
        if len(columns) == 6 and p.returncode in (0, 1):
            result.update(zip(['status', 'parts', 'vertices', 'triangles', 'textures', 'error'], columns))
            if mode == 'recovery' and result['status'].startswith('OK_'):
                result['status'] = 'RECOVERY_OK'
        else:
            result.update(status='ERROR', error=f'Process exit {p.returncode}: {p.stderr[:1000]} {p.stdout[:1000]}')
    except subprocess.TimeoutExpired:
        # subprocess.run kills and waits for the child before raising on both Windows and POSIX.
        result.update(status='TIMEOUT', error=f'Hard OS timeout {timeout}s')
    error = result['error']
    match = re.search(r'Block (\d+) \(([^)]+)\)', error)
    if match:
        result['blockindex'], result['blocktype'] = match.groups()
    else:
        match = re.search(r"Block-Typ '([^']+)' bei Block (\d+)", error)
        if match:
            result['blocktype'], result['blockindex'] = match.groups()
    result['seconds'] = f'{time.monotonic() - start:.3f}'
    return result


def run_phase(rows, args, mode):
    f, w = writer(args.out / (mode + '.tsv'), FIELDS)
    counts = collections.Counter()
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(probe, args.probe.resolve(), args.out / 'inputs', row, mode, args.timeout) for row in rows]
        for future in concurrent.futures.as_completed(futures):
            row = future.result()
            w.writerow(row)
            f.flush()
            counts[row['status']] += 1
            results.append(row)
            if len(results) % 250 == 0:
                print(mode, len(results), '/', len(rows), dict(counts), flush=True)
    f.close()
    print(mode, dict(counts), flush=True)
    groups = collections.Counter((r['blocktype'] or 'UNKNOWN', re.sub(r'\d+', '#', r['error'])) for r in results if r['status'] in ('ERROR', 'TIMEOUT'))
    gf, gw = writer(args.out / (mode + '-groups.tsv'), ['count', 'blocktype', 'cause'])
    for (typ, cause), n in groups.most_common():
        gw.writerow(dict(count=n, blocktype=typ, cause=cause))
    gf.close()
    (args.out / (mode + '-summary.json')).write_text(json.dumps(dict(counts), indent=2), encoding='utf-8')
    return results


def summarize(out):
    def read(name):
        with (out / name).open(encoding='utf-8', newline='') as f:
            return list(csv.DictReader(f, delimiter='\t'))
    standard = {r['sha256']: r for r in read('standard.tsv')}
    recovery = {r['sha256']: r for r in read('recovery.tsv')}
    fields = FIELDS + ['standard_status', 'standard_error', 'recovery_error']
    combined = {}
    for sha, s in standard.items():
        r = dict(recovery.get(sha, s))
        r.update(standard_status=s['status'], standard_error=s['error'],
                 recovery_error=recovery.get(sha, {}).get('error', ''))
        # Retain the standard block diagnosis even when recovery times out.
        if s['status'] in ('ERROR', 'TIMEOUT'):
            r['blockindex'], r['blocktype'] = s['blockindex'], s['blocktype']
        combined[sha] = r
    f, w = writer(out / 'results.tsv', fields)
    w.writerows(combined.values())
    f.close()
    occurrences = read('inventory.tsv')
    of, ow = writer(out / 'occurrence-results.tsv', fields)
    counts = collections.Counter()
    for source in occurrences:
        r = dict(combined[source['sha256']])
        r.update(source)
        ow.writerow(r)
        counts[r['status']] += 1
    of.close()
    archive_rows = read('archives.tsv')
    summary = dict(occurrences=len(occurrences), distinct_inputs=len(combined),
                   standard=dict(collections.Counter(r['status'] for r in standard.values())),
                   recovery=dict(collections.Counter(r['status'] for r in recovery.values())),
                   final_distinct=dict(collections.Counter(r['status'] for r in combined.values())),
                   final_occurrences=dict(counts),
                   archive_status=dict(collections.Counter(r['status'] for r in archive_rows)),
                   distinct_inputs_with_warnings=sum(bool(r.get('warnings')) for r in combined.values()))
    (out / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2), flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--probe', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--root', action='append', default=[])
    ap.add_argument('--jobs', type=int, default=8)
    ap.add_argument('--timeout', type=float, default=3)
    ap.add_argument('--phase', choices=['all', 'inventory', 'standard', 'recovery'], default='all')
    args = ap.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    if args.phase in ('inventory', 'all'):
        rows = inventory(args.root, args.out)
        if args.phase == 'inventory':
            return
    else:
        with (args.out / 'inventory.tsv').open(encoding='utf-8', newline='') as f:
            rows = list({r['sha256']: r for r in csv.DictReader(f, delimiter='\t')}.values())
    if args.phase in ('all', 'standard'):
        results = run_phase(rows, args, 'standard')
    else:
        with (args.out / 'standard.tsv').open(encoding='utf-8', newline='') as f:
            results = list(csv.DictReader(f, delimiter='\t'))
    if args.phase in ('all', 'recovery'):
        failed = [r for r in results if r['status'] in ('ERROR', 'TIMEOUT')]
        run_phase(failed, args, 'recovery')
        summarize(args.out)


if __name__ == '__main__':
    main()
