#!/usr/bin/env python3
"""Analyze raw timings without discarding outliers; save CSV, HTML, PNG and SVG."""
from pathlib import Path
import csv
import html
import itertools
import json
import sys
import os
import tempfile
import numpy as np
from scipy import stats
import scipy
os.environ.setdefault('MPLCONFIGDIR', str(Path(tempfile.gettempdir()) / 'gcc-tm-matplotlib'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def write_csv(path, rows):
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)


def main():
    directory = Path(sys.argv[1])
    with (directory / 'raw.csv').open() as f: raw = list(csv.DictReader(f))
    groups = {}
    for row in raw:
        key = row['case'], int(row['operations']), int(row['threads']), row['variant']
        groups.setdefault(key, []).append((int(row['round']), float(row['seconds'])))
    metadata = json.loads((directory / 'metadata.json').read_text())
    expected_n = metadata['repetitions']
    if len(groups) != 4 * len(metadata['sizes']) * len(metadata['threads']) * 3:
        raise ValueError('Incomplete factorial design')
    for values in groups.values():
        if sorted(r for r, _ in values) != list(range(1, expected_n + 1)):
            raise ValueError('Missing or duplicated randomized rounds')
    rng = np.random.default_rng(20261111)
    summary = []
    for (case, size, threads, variant), values in sorted(groups.items()):
        x = np.array([v for _, v in sorted(values)])
        samples = rng.choice(x, (5000, len(x)), replace=True).mean(axis=1)
        low, high = np.quantile(samples, [.025, .975])
        summary.append(dict(case=case, operations=size, threads=threads, variant=variant, n=len(x),
                            mean_s=x.mean(), median_s=np.median(x), std_s=x.std(ddof=1),
                            cv=x.std(ddof=1)/x.mean(), q1_s=np.quantile(x,.25), q3_s=np.quantile(x,.75),
                            min_s=x.min(), max_s=x.max(), mean_ci95_low_s=low, mean_ci95_high_s=high,
                            throughput_ops_s=size/x.mean()))
    write_csv(directory / 'summary.csv', summary)
    comparisons = []
    for case, size, threads in sorted({k[:3] for k in groups}):
        for a,b in itertools.combinations(['directive', 'gcc_tm', 'internal'],2):
            av=sorted(groups[case,size,threads,a]); bv=sorted(groups[case,size,threads,b])
            if [r for r,_ in av] != [r for r,_ in bv]: raise ValueError('Unpaired rounds')
            x=np.array([v for _,v in av]); y=np.array([v for _,v in bv])
            ix=rng.integers(0,len(x),(5000,len(x)))
            ratios=x[ix].mean(axis=1)/y[ix].mean(axis=1)
            low,high=np.quantile(ratios,[.025,.975])
            delta=np.log(x/y)
            p=1.0 if np.all(delta==0) else float(stats.wilcoxon(delta,alternative='two-sided',method='auto').pvalue)
            comparisons.append(dict(case=case,operations=size,threads=threads,n=len(x),a=a,b=b,
                                    mean_time_ratio_a_over_b=x.mean()/y.mean(),ratio_ci95_low=low,ratio_ci95_high=high,
                                    wilcoxon_log_ratio_p=p))
    # Holm family-wise correction across every reported comparison.
    order=np.argsort([r['wilcoxon_log_ratio_p'] for r in comparisons]); adjusted=0.0
    for rank,index in enumerate(order):
        adjusted=max(adjusted,min(1.0,(len(order)-rank)*comparisons[index]['wilcoxon_log_ratio_p']))
        comparisons[index]['holm_adjusted_p']=adjusted
    write_csv(directory / 'comparisons.csv', comparisons)
    figures=[]
    colors={'directive':'#1976a3','gcc_tm':'#b35b12','internal':'#47813b'}
    for case in sorted({r['case'] for r in summary}):
        sizes=sorted({r['operations'] for r in summary if r['case']==case})
        fig,axes=plt.subplots(1,3,figsize=(15,4.7),layout='constrained')
        for ax,size in zip(axes,sizes):
            for variant,color in colors.items():
                rows=sorted([r for r in summary if r['case']==case and r['operations']==size and r['variant']==variant],key=lambda r:r['threads'])
                x=[r['threads'] for r in rows]; y=np.array([r['mean_s'] for r in rows])*1000
                ax.plot(x,y,'o-',label=variant,color=color)
                ax.fill_between(x,[r['mean_ci95_low_s']*1000 for r in rows],[r['mean_ci95_high_s']*1000 for r in rows],color=color,alpha=.18)
            ax.set(title=f'{size:,} operations',xlabel='Threads',ylabel='Mean elapsed time (ms)',xticks=x)
            ax.set_yscale('log');ax.grid(alpha=.25);ax.legend()
        fig.suptitle(f'{case}: mean and pointwise 95% bootstrap confidence interval (log y)')
        stem=f'{case}-time'
        for suffix in ['png','svg']:fig.savefig(directory/f'{stem}.{suffix}',dpi=170)
        plt.close(fig);figures.append(stem)
        fig,axes=plt.subplots(1,3,figsize=(15,4.7),layout='constrained')
        for ax,size in zip(axes,sizes):
            threads=sorted({k[2] for k in groups if k[:2]==(case,size)})
            for offset,(variant,color) in enumerate(colors.items()):
                data=[[v for _,v in sorted(groups[case,size,t,variant])] for t in threads]
                positions=np.arange(len(threads))*4+offset
                bp=ax.boxplot(data,positions=positions,widths=.7,patch_artist=True,showfliers=True)
                for patch in bp['boxes']:patch.set_facecolor(color);patch.set_alpha(.65)
                ax.plot([],[],color=color,label=variant)
            ax.set(title=f'{size:,} operations',xlabel='Threads',ylabel='Elapsed time (seconds)',xticks=np.arange(len(threads))*4+1,xticklabels=threads)
            ax.set_yscale('log');ax.grid(alpha=.25);ax.legend()
        fig.suptitle(f'{case}: distributions; median, IQR, 1.5×IQR whiskers and outliers (all retained)')
        stem=f'{case}-distribution'
        for suffix in ['png','svg']:fig.savefig(directory/f'{stem}.{suffix}',dpi=170)
        plt.close(fig);figures.append(stem)
    meta=json.loads((directory/'metadata.json').read_text())
    meta['analysis_packages'] = {'numpy': np.__version__, 'scipy': scipy.__version__, 'matplotlib': matplotlib.__version__}
    (directory/'metadata.json').write_text(json.dumps(meta, indent=2)+'\n')
    page='''<!doctype html><html lang="en"><meta charset="utf-8"><title>Transactional timing experiment</title><style>body{font:16px/1.6 system-ui;max-width:1400px;margin:auto;padding:2rem}img{width:100%}table{border-collapse:collapse}td,th{padding:.4rem;border:1px solid #ccc}</style><h1>Transactional timing experiment</h1>'''
    page+=f'<p>{len(raw)} measured executions; {meta["repetitions"]} randomized rounds. Sizes: {meta["sizes"]}; threads: {meta["threads"]}.</p>'
    page+='''<p><a href="raw.csv">Raw observations</a> · <a href="summary.csv">Descriptive statistics</a> · <a href="comparisons.csv">Paired comparisons</a> · <a href="metadata.json">Environment and binary hashes</a></p><p>Monotonic wall time includes parallel team startup, transaction/runtime initialization and the final barrier, but excludes result printing and validation. One separate-process warmup per configuration precedes measurement. GNU TM is labeled gcc_tm; its executable is gnu_tm. All programs validate their result and actual team size. The measured computational load is the total number of transactions, not independent work inside each transaction.</p><p>Means have percentile-bootstrap 95% confidence intervals (5,000 resamples). Also reported: median, sample standard deviation, coefficient of variation, quartiles and range. No observations are removed. Intervals are pointwise, not simultaneous. Paired comparisons resample matching randomized rounds, reporting ratios of arithmetic mean times (a/b above 1 means a is slower). Wilcoxon tests use paired log-time ratios, with Holm correction over all comparisons. P-values do not establish practical importance; read effect sizes and intervals first. Pairing controls broad round effects but not all within-round machine drift. Autocorrelation, shared-host noise and CPU quota can limit inference. Small cold-start workloads can be dominated by startup. These are single-host contention benchmarks, not general STM rankings.</p>'''
    page+='<h2>Observed ratios: directive / GNU TM</h2><table><tr><th>Case</th><th>Operations</th><th>Threads</th><th>Ratio</th><th>95% CI</th></tr>'
    for r in comparisons:
        if r['a']=='directive' and r['b']=='gcc_tm':page+=f'<tr><td>{html.escape(r["case"])}</td><td>{r["operations"]}</td><td>{r["threads"]}</td><td>{r["mean_time_ratio_a_over_b"]:.3f}</td><td>[{r["ratio_ci95_low"]:.3f}, {r["ratio_ci95_high"]:.3f}]</td></tr>'
    page+='</table>'
    for stem in figures:page+=f'<h2>{html.escape(stem)}</h2><a href="{stem}.svg">SVG</a><img src="{stem}.png" alt="{html.escape(stem)}">'
    (directory/'index.html').write_text(page+'</html>\n')
    print(f'Analysis: {len(summary)} groups, {len(comparisons)} comparisons, {len(figures)} plots.')


if __name__=='__main__':main()
