#!/usr/bin/env python3
"""Explore the early-write hypothesis without assuming a custom-STM advantage."""
import csv
import hashlib
import platform
import json
import os
from pathlib import Path
import random
import re
import subprocess
import time
import tempfile
import numpy as np
os.environ.setdefault('MPLCONFIGDIR', str(Path(tempfile.gettempdir())/'gcc-tm-matplotlib'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def main():
    base = Path(__file__).resolve().parent
    out = base / '.build' / ('experiment-' + time.strftime('%Y%m%d-%H%M%S'))
    out.mkdir(parents=True, exist_ok=False)
    variants = {'directive': 'directive', 'internal': 'internal', 'gcc_tm': 'gnu_tm'}
    configs = [(t, work, v) for t in [2,4,6,8] for work in [16,1024,16384] for v in variants]
    subprocess.run(['make','-C',str(base),'all'],check=True,stdout=subprocess.DEVNULL)
    env=os.environ.copy();env.update(OMP_DYNAMIC='FALSE',OMP_PROC_BIND='close',OMP_PLACES='cores',OMP_WAIT_POLICY='PASSIVE')
    env.pop('LD_LIBRARY_PATH',None);env.pop('LD_PRELOAD',None)
    metadata = dict(platform=platform.platform(), affinity=sorted(os.sched_getaffinity(0)),
                    cpuinfo=Path('/proc/cpuinfo').read_text(), repetitions=30, transactions=128,
                    steps=[16,1024,16384], threads=[2,4,6,8], seed=20261111,
                    environment={k:v for k,v in env.items() if k.startswith(('OMP_','GOMP_'))},
                    binaries={v:hashlib.sha256((base/'.build'/name).read_bytes()).hexdigest() for v,name in variants.items()})
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    rng=random.Random(20261111); rows=[]
    def execute(config):
        threads, work, variant=config
        p=subprocess.run([str(base/'.build'/variants[variant]),str(threads),'128',str(work)],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
        match=re.search(r'^TIMING seconds=([0-9.]+) threads=(\d+)$',p.stdout,re.M)
        if p.returncode or not match or int(match[2])!=threads:raise RuntimeError(p.stdout)
        return float(match[1])
    warm=configs.copy();rng.shuffle(warm)
    for config in warm:execute(config)
    with (out/'raw.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=['round','order','threads','steps','variant','seconds']);writer.writeheader()
        for round_id in range(1,31):
            order=configs.copy();rng.shuffle(order)
            for position,config in enumerate(order):
                threads,work,variant=config
                row=dict(round=round_id,order=position,threads=threads,steps=work,variant=variant,seconds=execute(config))
                rows.append(row);writer.writerow(row);f.flush()
            if round_id%10==0:print(f'Round {round_id}/30',flush=True)
    groups={config:np.array([r['seconds'] for r in rows if (r['threads'],r['steps'],r['variant'])==config])for config in configs}
    rng_np=np.random.default_rng(20261111);summary=[];comparisons=[]
    for (threads,work,variant),x in groups.items():
        means=rng_np.choice(x,(5000,30)).mean(axis=1);lo,hi=np.quantile(means,[.025,.975])
        summary.append(dict(threads=threads,steps=work,variant=variant,n=30,mean_s=x.mean(),median_s=np.median(x),std_s=x.std(ddof=1),mean_ci95_low_s=lo,mean_ci95_high_s=hi))
    for threads in [2,4,6,8]:
        for work in [16,1024,16384]:
            for variant in ['directive','internal']:
                x=groups[threads,work,variant];y=groups[threads,work,'gcc_tm'];ix=rng_np.integers(0,30,(5000,30));lo,hi=np.quantile(x[ix].mean(axis=1)/y[ix].mean(axis=1),[.025,.975])
                comparisons.append(dict(threads=threads,steps=work,variant=variant,time_ratio_to_gcc_tm=x.mean()/y.mean(),ratio_ci95_low=lo,ratio_ci95_high=hi))
    for filename,data in [('summary.csv',summary),('comparisons.csv',comparisons)]:
        with (out/filename).open('w',newline='')as f:
            writer=csv.DictWriter(f,fieldnames=list(data[0]));writer.writeheader();writer.writerows(data)
    fig,axes=plt.subplots(1,4,figsize=(16,4.7),layout='constrained')
    for ax,threads in zip(axes,[2,4,6,8]):
        for variant in variants:
            rs=sorted([r for r in summary if r['threads']==threads and r['variant']==variant],key=lambda r:r['steps'])
            ax.plot([r['steps']for r in rs],[r['mean_s']*1000 for r in rs],'o-',label=variant)
            ax.fill_between([r['steps']for r in rs],[r['mean_ci95_low_s']*1000 for r in rs],[r['mean_ci95_high_s']*1000 for r in rs],alpha=.15)
        ax.set(xscale='log',yscale='log',xlabel='Compute steps per transaction',ylabel='Mean wall time (ms)',title=f'{threads} threads');ax.legend();ax.grid(alpha=.2)
    fig.suptitle('128 transactions: 30 randomized rounds, pointwise bootstrap 95% intervals')
    for ext in ['png','svg']:fig.savefig(out/f'timing.{ext}',dpi=160)
    plt.close(fig)
    (out/'index.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8"><title>Compute transaction experiment</title><h1>Early-write compute transaction experiment</h1><p>1,080 measured executions plus 36 discarded warmups; 30 randomized rounds; 128 transactions each. Ratios below one favor the custom implementation; inspect bootstrap intervals before claiming an advantage. No outliers removed. Timing includes startup and excludes independent serial validation. Compare only this workload; a crossing is not assumed. Different optimization and linking paths can affect results.</p><p><a href="raw.csv">Raw</a> · <a href="summary.csv">Summary</a> · <a href="comparisons.csv">Paired mean ratios and pointwise bootstrap intervals</a> · <a href="timing.svg">SVG</a></p><img style="width:100%" src="timing.png" alt="Time versus computation per transaction"></html>''')
    print(f'Results: {out}',flush=True)
    for variant in ['directive','internal']:
        rs=[r for r in comparisons if r['variant']==variant]
        print(f'{variant}: ratio range {min(r["time_ratio_to_gcc_tm"]for r in rs):.3f}..{max(r["time_ratio_to_gcc_tm"]for r in rs):.3f}; intervals below 1: {sum(r["ratio_ci95_high"]<1 for r in rs)}/12')


if __name__=='__main__':main()
