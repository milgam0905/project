"""Copy an original SQLite DB into a NEW writable runtime DB; never overwrite."""
import argparse,sqlite3,hashlib
from pathlib import Path
parser=argparse.ArgumentParser()
parser.add_argument('source');parser.add_argument('destination')
a=parser.parse_args()
src=Path(a.source).resolve(strict=True);dst=Path(a.destination).resolve()
if src==dst:raise SystemExit('Source and destination must differ')
if not dst.parent.is_dir():raise SystemExit('Create destination parent directory first')
# Exclusive reservation, so an existing DB is never replaced, even after a failed run.
with dst.open('xb'):pass
before=hashlib.sha256(src.read_bytes()).hexdigest()
try:
    with sqlite3.connect(src.as_uri()+'?mode=ro',uri=True) as original, sqlite3.connect(dst) as runtime:
        original.backup(runtime)
        runtime.executescript("""
        PRAGMA foreign_keys=ON;
        BEGIN IMMEDIATE;
        CREATE TABLE bp_runtime_meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);
        INSERT INTO bp_runtime_meta VALUES('format','bp-native-v1');
        CREATE UNIQUE INDEX bp_unique_absorption ON mana_absorptions(save_id,mage_entity_id,beast_entity_id);
        COMMIT;
        """)
        assert runtime.execute('PRAGMA integrity_check').fetchone()[0]=='ok'
        assert not runtime.execute('PRAGMA foreign_key_check').fetchall()
    assert hashlib.sha256(src.read_bytes()).hexdigest()==before
except Exception:
    print('Preparation failed. Destination left for inspection; choose a new destination to retry.')
    raise
print('Prepared runtime copy:',dst)
print('Original SHA256:',before)
