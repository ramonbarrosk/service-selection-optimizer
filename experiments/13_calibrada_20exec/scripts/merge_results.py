"""Junta os CSVs por lote de results/raw/<job>/ em results/<job>.csv."""
import csv
import sys
from pathlib import Path

RAW = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("experiments/13_calibrada_20exec/results/raw")
OUT = RAW.parent

for job in sorted(p for p in RAW.iterdir() if p.is_dir()):
    rows, header = [], None
    for part in sorted(job.glob("*.csv")):
        with part.open() as handle:
            reader = csv.DictReader(handle)
            header = header or reader.fieldnames
            rows.extend(reader)
    if not rows:
        continue
    rows.sort(key=lambda r: (int(r["instance"]), int(r["repetition"])))
    with (OUT / f"{job.name}.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=header)
        writer.writeheader()
        writer.writerows(rows)
    print(f"{job.name}: {len(rows)} linhas, {len({r['instance'] for r in rows})} instâncias")
