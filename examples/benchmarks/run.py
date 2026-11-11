#!/usr/bin/env python3
"""Run a randomized repeated-measures timing experiment and create its analysis."""
import argparse
import csv
import hashlib
import itertools
import json
import os
from pathlib import Path
import platform
import random
import re
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
EXAMPLES = ROOT / 'examples'
VARIANTS = {'directive': 'directive', 'gcc_tm': 'gnu_tm', 'internal': 'internal'}


def create_output_directory(requested=None, base=None):
    """Reserve a fresh directory atomically, preserving every previous run."""
    if requested is not None:
        output = requested.resolve()
        output.mkdir(parents=True, exist_ok=False)
        return output
    base = (base or EXAMPLES / 'benchmarks/results').resolve()
    output = base
    stamp = time.strftime('%Y%m%d-%H%M%S')
    suffix = 0
    while True:
        try:
            output.mkdir(parents=True, exist_ok=False)
            return output
        except FileExistsError:
            suffix += 1
            name = f'{base.name}-{stamp}'
            if suffix > 1:
                name += f'-{suffix}'
            output = base.with_name(name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repetitions', type=int, default=30)
    parser.add_argument('--sizes', type=int, nargs=3, default=[1000, 10000, 100000])
    parser.add_argument('--threads', type=int, nargs='+', default=[2, 4, 6, 8])
    parser.add_argument('--seed', type=int, default=20261111)
    parser.add_argument('--output', type=Path, help='New output directory; default selects a fresh results directory automatically.')
    args = parser.parse_args()
    if args.repetitions < 2 or any(n < 1 or n > 1000000 for n in args.sizes) or any(t < 1 or t > 8 for t in args.threads):
        parser.error('Use repetitions >= 2, sizes 1..1000000 and threads 1..8.')
    if len(set(args.sizes)) != 3 or len(set(args.threads)) != len(args.threads):
        parser.error('Sizes and thread counts must be distinct.')
    try:
        output = create_output_directory(args.output)
    except FileExistsError:
        parser.error(f'Output directory already exists: {args.output}. Choose a new --output path or omit --output for an automatic name.')
    print(f'Output directory: {output}', flush=True)
    cases = [('counter', '03-concurrent-counter', None)] + [
        (f'workload-{w}-{r}', '04-workload', (w, r)) for w, r in [(1, 9), (1, 1), (9, 1)]]
    for directory in ['03-concurrent-counter', '04-workload']:
        subprocess.run(['make', '-C', str(EXAMPLES / directory), 'all'], check=True,
                       stdout=subprocess.DEVNULL)
    configs = list(itertools.product(cases, args.sizes, args.threads, VARIANTS))
    env = os.environ.copy()
    env.update(OMP_DYNAMIC='FALSE', OMP_PROC_BIND='close', OMP_PLACES='cores',
               OMP_WAIT_POLICY='PASSIVE')
    # Prevent a parent shell from silently overriding the selected local libraries.
    env.pop('LD_PRELOAD', None)
    env.pop('LD_LIBRARY_PATH', None)
    metadata = {'started': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
                'platform': platform.platform(), 'cpu_count': os.cpu_count(),
                'affinity': sorted(os.sched_getaffinity(0)), 'seed': args.seed,
                'sizes': args.sizes, 'threads': args.threads, 'repetitions': args.repetitions,
                'environment': {k: v for k, v in env.items() if k.startswith('OMP_') or k.startswith('GOMP_')},
                'timer': 'CLOCK_MONOTONIC wall seconds; parallel team startup and final barrier included',
                'warmups': 'One discarded fresh-process run per configuration; runtime initialization remains timed.',
                'binaries': {}}
    for _, directory, _ in cases:
        for binary in VARIANTS.values():
            path = EXAMPLES / directory / '.build' / binary
            metadata['binaries'][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    metadata['cpuinfo'] = Path('/proc/cpuinfo').read_text()
    metadata['load_average'] = os.getloadavg()
    metadata['python'] = platform.python_version()
    metadata['build_rules'] = (EXAMPLES / 'common/rules.mk').read_text()
    metadata['runtime_hashes'] = {}
    for lib in ['libgomp', 'libitm']:
        for path in (ROOT / 'gcc-build').glob(f'*/{lib}/.libs/{lib}.so'):
            metadata['runtime_hashes'][str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    metadata['compiler'] = subprocess.check_output([str(ROOT / 'gcc-build/gcc/xgcc'), '--version'], text=True)
    for name in ['cpu.max', 'cpuset.cpus.effective']:
        path = Path('/sys/fs/cgroup') / name
        if path.exists(): metadata[name] = path.read_text().strip()
    (output / 'metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    rng = random.Random(args.seed)
    def execute(config):
        (case, directory, ratio), size, threads, variant = config
        cmd = [str(EXAMPLES / directory / '.build' / VARIANTS[variant]), str(threads), str(size)]
        if ratio: cmd += [str(x) for x in ratio]
        result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, timeout=120)
        timings = re.findall(r'^TIMING seconds=([0-9.]+) threads=(\d+)$', result.stdout, re.M)
        results = re.findall(r'^RESULT (.*)$', result.stdout, re.M)
        expected = (f'counter={size} operations={size}' if ratio is None else
                    f'operations={size} writes={size*ratio[0]//sum(ratio)} reads={size-size*ratio[0]//sum(ratio)} value={size*ratio[0]//sum(ratio)}')
        if result.returncode or len(timings) != 1 or results != [expected] or int(timings[0][1]) != threads or float(timings[0][0]) <= 0 or '[newtm]' in result.stdout:
            raise RuntimeError(f'Invalid benchmark execution: {cmd}\n{result.stdout}')
        return {'case': case, 'operations': size, 'threads': threads, 'variant': variant,
                'seconds': float(timings[0][0])}
    print(f'Warmup: {len(configs)} configurations', flush=True)
    warmups = configs.copy(); rng.shuffle(warmups)
    for config in warmups: execute(config)
    with (output / 'raw.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=['round', 'order', 'case', 'operations', 'threads', 'variant', 'seconds'])
        writer.writeheader()
        for repetition in range(args.repetitions):
            order = configs.copy(); rng.shuffle(order)
            for position, config in enumerate(order):
                row = execute(config)
                writer.writerow({'round': repetition + 1, 'order': position + 1, **row})
                stream.flush()
            print(f'Round {repetition + 1}/{args.repetitions} complete ({len(configs)} runs)', flush=True)
    subprocess.run(['python3', str(Path(__file__).with_name('analyze.py')), str(output)], check=True)
    print(f'Results: {output / "index.html"}', flush=True)


if __name__ == '__main__':
    main()
