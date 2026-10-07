#!/usr/bin/env python3
"""Benchmark runner for BentoDBMS.

Generates SQL workloads, runs them through the engine in a temporary HOME
(real data is never touched), and reports throughput, latency and the
engine's own --stats counters. The same SQL is optionally run through the
sqlite3 command line tool as a reference point.

Usage:
    python3 bench/run_bench.py --bin ./minidb
    python3 bench/run_bench.py --bin ./minidb --sizes 1000,2000 --runs 5 --no-sqlite
"""

import argparse
import csv
import os
import random
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

TABLE = "create table t (id int, v int, name char(16), primary key (id));"
WIDE_TABLE = "create table w (id int, pad char(2000), primary key (id));"
INDEX = "create index ti on t (id);"
OPS = 500  # statements in each measured read/update/delete workload

STAT_KEYS = [
    "block_reads", "block_writes", "cache_hits", "cache_misses", "evictions",
    "catalog_writes", "node_visits", "latency_p50_us", "latency_p99_us",
]


def inserts(ids):
    return ["insert into t values (%d, %d, 'n%d');" % (i, i % 100, i) for i in ids]


def shuffled_ids(n, seed):
    ids = list(range(1, n + 1))
    random.Random(seed).shuffle(ids)
    return ids


def workloads(n):
    """Returns (name, indexed, setup statements, measured statements) for a table of n rows."""
    ids = shuffled_ids(n, 1)
    picks = random.Random(2).sample(ids, min(OPS, n))
    result = []
    for indexed in (False, True):
        schema = [TABLE] + ([INDEX] if indexed else [])
        loaded = schema + inserts(ids)
        result.append(("bulk insert", indexed, schema, inserts(ids)))
        result.append(("point select", indexed, loaded,
                       ["select * from t where id = %d;" % i for i in picks]))
        result.append(("update by key", indexed, loaded,
                       ["update t set v = 7 where id = %d;" % i for i in picks]))
        result.append(("delete by key", indexed, loaded,
                       ["delete from t where id = %d;" % i for i in picks]))
    # A full scan does not use the index, so it is only measured once.
    result.append(("full scan", False, [TABLE] + inserts(ids),
                   ["select * from t where v < %d;" % (i % 100) for i in range(20)]))
    # Delete half of the rows, then insert the same number again (reuses freed space).
    half = ids[: n // 2]
    result.append(("delete half + reinsert", False, [TABLE] + inserts(ids),
                   ["delete from t where id = %d;" % i for i in half]
                   + inserts(range(n + 1, n + 1 + len(half)))))
    return result


def cache_workloads():
    """Full scans over a table that fits in the 300-block cache and one that does not."""
    result = []
    for rows in (300, 1200):  # two rows per block: 150 and 600 blocks
        setup = [WIDE_TABLE] + ["insert into w values (%d, 'p');" % i for i in range(1, rows + 1)]
        measured = ["select * from w where id = %d;" % (1 + (i * 37) % rows) for i in range(50)]
        result.append(("wide table scan", rows, setup, measured))
    return result


def run_engine(binary, home, statements, stats=False):
    """Feeds the statements to the engine. Returns (seconds, stats dict)."""
    script = "\n".join(statements + ["quit"]) + "\n"
    args = [binary, "-q"] + (["--stats"] if stats else [])
    env = dict(os.environ, HOME=home)
    start = time.perf_counter()
    proc = subprocess.run(args, input=script, env=env, text=True, errors="replace",
                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    seconds = time.perf_counter() - start
    if proc.returncode != 0:
        sys.exit("engine exited with code %d:\n%s" % (proc.returncode, proc.stderr[-2000:]))
    values = {}
    in_stats = False
    for line in proc.stderr.splitlines():
        if line.startswith("--- stats ---"):
            in_stats = True
        elif in_stats and ": " in line:
            key, value = line.split(": ", 1)
            values[key] = int(value)
        elif line.strip():
            sys.exit("engine reported an error: " + line)
    return seconds, values


def dir_bytes(path):
    total = 0
    for root, _, files in os.walk(path):
        for name in files:
            total += os.path.getsize(os.path.join(root, name))
    return total


def run_sqlite(setup, measured, workdir):
    """Runs the same SQL through sqlite3, one transaction per statement. Returns seconds."""
    db = os.path.join(workdir, "ref.sqlite")
    if os.path.exists(db):
        os.remove(db)
    subprocess.run(["sqlite3", db], input="\n".join(setup) + "\n", text=True,
                   stdout=subprocess.DEVNULL, check=True)
    start = time.perf_counter()
    subprocess.run(["sqlite3", db], input="\n".join(measured) + "\n", text=True,
                   stdout=subprocess.DEVNULL, check=True)
    return time.perf_counter() - start


def measure(binary, name, rows, indexed, setup, measured, runs, use_sqlite, workdir):
    # Build the starting database once, then copy it for every run.
    base = os.path.join(workdir, "base")
    shutil.rmtree(base, ignore_errors=True)
    os.makedirs(os.path.join(base, "MiniDBData"))
    prefix = ["use d;"]
    run_engine(binary, base, ["create database d;", "use d;"] + setup)

    times, last_stats, size = [], {}, 0
    for _ in range(runs):
        home = os.path.join(workdir, "run")
        shutil.rmtree(home, ignore_errors=True)
        shutil.copytree(base, home)
        seconds, last_stats = run_engine(binary, home, prefix + measured, stats=True)
        times.append(seconds)
        size = dir_bytes(os.path.join(home, "MiniDBData", "d"))
    seconds = statistics.median(times)

    row = {
        "workload": name, "rows": rows, "indexed": "yes" if indexed else "no",
        "statements": len(measured), "seconds": round(seconds, 4),
        "ops_per_sec": round(len(measured) / seconds),
        "data_bytes": size,
    }
    for key in STAT_KEYS:
        row[key] = last_stats.get(key, 0)
    if use_sqlite:
        ref = statistics.median(run_sqlite(setup, measured, workdir) for _ in range(runs))
        row["sqlite_seconds"] = round(ref, 4)
        row["sqlite_ops_per_sec"] = round(len(measured) / ref)
    print("  %-24s rows=%-6d indexed=%-3s %8.3fs %8d ops/s" %
          (name, rows, row["indexed"], seconds, row["ops_per_sec"]), file=sys.stderr)
    return row


def markdown(rows, use_sqlite):
    head = ["Workload", "Rows", "Index", "Statements", "Seconds", "Ops/s", "p50 (us)",
            "p99 (us)", "Block reads", "Block writes", "Evictions", "Data size (KB)"]
    if use_sqlite:
        head.append("SQLite ops/s")
    lines = ["| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for r in rows:
        cells = [r["workload"], r["rows"], r["indexed"], r["statements"], "%.3f" % r["seconds"],
                 r["ops_per_sec"], r["latency_p50_us"], r["latency_p99_us"], r["block_reads"],
                 r["block_writes"], r["evictions"], r["data_bytes"] // 1024]
        if use_sqlite:
            cells.append(r["sqlite_ops_per_sec"])
        lines.append("| " + " | ".join(str(c) for c in cells) + " |")
    return "\n".join(lines) + "\n"


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description="Run the BentoDBMS benchmarks.")
    parser.add_argument("--bin", default="./minidb", help="path to the engine binary")
    parser.add_argument("--sizes", default="1000,2000,4000",
                        help="comma separated table sizes in rows")
    parser.add_argument("--runs", type=int, default=3, help="runs per workload (median is kept)")
    parser.add_argument("--no-sqlite", action="store_true", help="skip the sqlite3 reference")
    parser.add_argument("--out", default=os.path.join(here, "results.csv"))
    parser.add_argument("--markdown", default=os.path.join(here, "results.md"))
    args = parser.parse_args()

    binary = os.path.abspath(args.bin)
    if not os.path.isfile(binary):
        sys.exit("engine binary not found: " + binary)
    use_sqlite = not args.no_sqlite and shutil.which("sqlite3") is not None
    sizes = [int(s) for s in args.sizes.split(",")]

    rows = []
    workdir = tempfile.mkdtemp(prefix="bentobench-")
    try:
        for n in sizes:
            for name, indexed, setup, measured in workloads(n):
                rows.append(measure(binary, name, n, indexed, setup, measured,
                                    args.runs, use_sqlite, workdir))
        for name, n, setup, measured in cache_workloads():
            rows.append(measure(binary, name, n, False, setup, measured,
                                args.runs, use_sqlite, workdir))
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    with open(args.out, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    table = markdown(rows, use_sqlite)
    with open(args.markdown, "w") as f:
        f.write(table)
    print(table)
    print("wrote %s and %s" % (args.out, args.markdown), file=sys.stderr)


if __name__ == "__main__":
    main()
