"""Exercise the actual QUERY_3 predicates using synthetic in-memory SQLite rows.

This never connects to MySQL and does not prove live-server ownership enforcement.
"""
import json
from pathlib import Path
import re
import sqlite3


root = Path(__file__).resolve().parents[2]
config = (root / "CacheLib/Config.h").read_text(encoding="utf-8-sig")
query_match = re.search(r'\bQUERY_3\s*=\s*((?:"[^"\n]*"\s*)+);', config)
if query_match is None:
    raise AssertionError("QUERY_3 could not be read from the production source.")
query = "".join(json.loads(part) for part in re.findall(r'"[^"\n]*"', query_match[1]))

with sqlite3.connect(":memory:") as connection:
    connection.executescript("""
        CREATE TABLE profile (profile_id INTEGER PRIMARY KEY, version INTEGER, name TEXT);
        CREATE TABLE characters (
            char_id INTEGER PRIMARY KEY, user_id INTEGER, channel_id INTEGER,
            deleted_at TEXT, profile_id INTEGER, attack INTEGER, level INTEGER,
            exp INTEGER, hp INTEGER, mp INTEGER, max_hp INTEGER, max_mp INTEGER,
            dir INTEGER, zone_id INTEGER, last_pos_x REAL, last_pos_y REAL
        );
        INSERT INTO profile VALUES (201, 4, 'synthetic_own');
        INSERT INTO profile VALUES (202, 1, 'synthetic_deleted');
        INSERT INTO characters VALUES
            (501, 11, 1, NULL, 201, 10, 1, 0, 100, 100, 100, 100, 0, 1, 10, 12),
            (502, 11, 1, '2026-10-18', 202, 10, 1, 0, 100, 100, 100, 100, 0, 1, 10, 12);
    """)
    cases = (
        ("own_active_correct_channel", (501, 11, 1), 1),
        ("wrong_owner", (501, 12, 1), 0),
        ("wrong_channel", (501, 11, 2), 0),
        ("deleted", (502, 11, 1), 0),
        ("missing_character", (999, 11, 1), 0),
    )
    results = []
    for name, parameters, expected in cases:
        rows = connection.execute(query, parameters).fetchall()
        if len(rows) != expected:
            raise AssertionError(f"Unexpected row count for {name}.")
        if expected and rows[0][:3] != (501, 201, 4):
            raise AssertionError("The selected character/profile columns changed.")
        results.append(f"PASS {name}: rows={expected}")

report = [
    "C02-AUTH-01/r1 focused SQL predicate validation",
    "Executed the actual CacheLib/Config.h QUERY_3 in SQLite :memory: with synthetic rows.",
    "No MySQL connection, live-server request, DB mutation, or runtime replacement.",
    "This validates SQL predicate semantics; it is not MySQL Connector or live-server integration evidence.",
    *results,
]
output = root / "UnityClient/Captures/c02-auth-validation.txt"
output.write_text("\n".join(report) + "\n", encoding="utf-8")
print("\n".join(report))
