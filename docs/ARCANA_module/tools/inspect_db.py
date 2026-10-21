"""Read-only schema report. No game content or WBG invented."""
import argparse,json,sqlite3
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('database')
args=parser.parse_args()
p=Path(args.database).resolve(strict=True)
with sqlite3.connect(p.as_uri()+'?mode=ro',uri=True) as db:
    db.execute('PRAGMA query_only=ON')
    rows=db.execute("SELECT type,name,sql FROM sqlite_schema WHERE type IN ('table','view') AND name NOT LIKE 'sqlite_%' ORDER BY name").fetchall()
    print(json.dumps([dict(type=t,name=n,sql=s) for t,n,s in rows],ensure_ascii=False,indent=2))
