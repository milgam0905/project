"""Loopback-only, read-only world telemetry receiver. Python 3.10+, stdlib only."""

import json
import math
import re
import threading
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

PORT = 8765
MAX_BODY = 1024 * 1024
MAX_WORLDS = 32
MAX_TOTAL = 16 * MAX_BODY
STALE_SECONDS = 5
RETENTION_SECONDS = 3600
DIST = Path(__file__).resolve().parent / "dist"
ID = re.compile(r"[A-Za-z0-9_.-]{1,64}\Z")
KINDS = ("npc", "monster", "wildlife", "player")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def identifier(value):
    require(isinstance(value, str) and ID.fullmatch(value), "Invalid identifier")


def number(value, nullable=False):
    if value is None and nullable:
        return
    require(type(value) in (int, float) and math.isfinite(value) and abs(value) <= 1e15,
            "Expected bounded finite number")


def label(value):
    require(isinstance(value, str) and len(value) <= 256, "Invalid label")


def metrics(value):
    require(isinstance(value, dict) and len(value) <= 64, "Invalid metrics")
    for key, item in value.items():
        identifier(key)
        number(item, nullable=True)


def validate(data):
    require(isinstance(data, dict), "Expected JSON object")
    require(data.get("schema") == "world-monitor.v1" and data.get("type") == "snapshot",
            "Unsupported schema or message type")
    for key in ("game_id", "world_id", "instance_id"):
        identifier(data.get(key))
    for key in ("sequence", "epoch"):
        require(type(data.get(key)) is int and 0 <= data[key] <= 2**53 - 1, "Invalid " + key)
    require(type(data.get("paused")) is bool, "Expected paused boolean")
    number(data.get("world_time_minutes"), nullable=True)
    bounds = data.get("bounds")
    require(isinstance(bounds, dict), "Missing bounds")
    for key in ("min_x", "min_y", "max_x", "max_y"):
        number(bounds.get(key))
    require(bounds["min_x"] < bounds["max_x"] and bounds["min_y"] < bounds["max_y"], "Invalid bounds")
    metrics(data.get("metrics", {}))
    towns, entities = data.get("settlements"), data.get("entities")
    require(isinstance(towns, list) and len(towns) <= 128, "Maximum 128 settlements")
    require(isinstance(entities, list) and len(entities) <= 2000, "Maximum 2000 entities")
    town_ids, entity_ids = set(), set()
    for town in towns:
        require(isinstance(town, dict), "Invalid settlement")
        identifier(town.get("id"))
        require(town["id"] not in town_ids, "Duplicate settlement ID")
        town_ids.add(town["id"])
        label(town.get("name"))
        metrics(town.get("metrics", {}))
    for entity in entities:
        require(isinstance(entity, dict), "Invalid entity")
        identifier(entity.get("id"))
        require(entity["id"] not in entity_ids, "Duplicate entity ID")
        entity_ids.add(entity["id"])
        require(entity.get("kind") in KINDS, "Invalid entity kind")
        for key in ("name", "role", "action", "action_label"):
            label(entity.get(key))
        require(entity.get("settlement_id") is None or entity["settlement_id"] in town_ids,
                "Unknown settlement reference")
        if entity.get("target_id") is not None:
            identifier(entity["target_id"])
        for key in ("alive", "active", "visible", "hostile"):
            require(type(entity.get(key)) is bool, "Expected entity boolean: " + key)
        position = entity.get("position")
        require(isinstance(position, dict), "Missing position")
        number(position.get("x"))
        number(position.get("y"))
        for key in ("health", "max_health"):
            number(entity.get(key), nullable=True)
            require(entity.get(key) is None or entity[key] >= 0, "Negative health")
        metrics(entity.get("metrics", {}))
        metrics(entity.get("personality", {}))
    return data


def reject_constant(value):
    raise ValueError("Non-finite JSON number: " + value)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "Duplicate JSON field")
        result[key] = value
    return result


def decode(body):
    data = json.loads(body.decode("utf-8"), parse_constant=reject_constant,
                      object_pairs_hook=unique_object)
    # Validate extension fields too: GET must always be serializable as finite UTF-8 JSON.
    pending = [(data, 0)]
    while pending:
        value, depth = pending.pop()
        require(depth <= 32, "Maximum JSON nesting depth is 32")
        if isinstance(value, dict):
            pending.extend((v, depth + 1) for v in value.values())
            pending.extend((k, depth + 1) for k in value)
        elif isinstance(value, list):
            pending.extend((v, depth + 1) for v in value)
        elif isinstance(value, str):
            value.encode("utf-8", errors="strict")
        elif type(value) in (int, float):
            require(math.isfinite(value), "Expected finite JSON number")
    return validate(data)


def summarize(entities):
    counts = {kind: {"total": 0, "alive": 0} for kind in KINDS}
    for entity in entities:
        counts[entity["kind"]]["total"] += 1
        counts[entity["kind"]]["alive"] += int(entity["alive"])
    npcs = [e for e in entities if e["kind"] == "npc"]
    return {
        "counts": counts,
        "npc_critical": sum(e["alive"] and e.get("health") is not None and
                            (e.get("max_health") or 0) > 0 and
                            e["health"] / e["max_health"] <= 0.25 for e in npcs),
        "npc_surrendered": sum(e.get("metrics", {}).get("surrendered") == 1 for e in npcs),
        "npc_hostile": sum(e["alive"] and e["hostile"] for e in npcs),
    }


class Store:
    def __init__(self, clock=time.monotonic):
        self.clock = clock
        self.lock = threading.Lock()
        self.worlds = {}

    def accept(self, data, size):
        key = tuple(data[name] for name in ("game_id", "world_id", "instance_id"))
        groups = {town["id"]: [] for town in data["settlements"]}
        for entity in data["entities"]:
            if entity.get("settlement_id") in groups:
                groups[entity["settlement_id"]].append(entity)
        summary = summarize(data["entities"])
        towns = {town_id: summarize(entities) for town_id, entities in groups.items()}
        with self.lock:
            now = self.clock()
            self.worlds = {k: v for k, v in self.worlds.items() if now - v["received"] < RETENTION_SECONDS}
            old = self.worlds.get(key)
            if old and (data["sequence"] <= old["snapshot"]["sequence"] or data["epoch"] < old["snapshot"]["epoch"]):
                return 409
            if not old and len(self.worlds) >= MAX_WORLDS:
                return 429
            total = sum(v["bytes"] for v in self.worlds.values()) - (old["bytes"] if old else 0) + size
            if total > MAX_TOTAL:
                return 429
            self.worlds[key] = {"snapshot": data, "received": now, "bytes": size,
                                "summary": summary, "settlements": towns,
                                "received_at": datetime.now(timezone.utc).isoformat()}
            return 202

    def state(self):
        with self.lock:
            now = self.clock()
            records = list(self.worlds.values())
        worlds = []
        for record in records:
            age = max(0, now - record["received"])
            if age >= RETENTION_SECONDS:
                continue
            snapshot = record["snapshot"]
            worlds.append({"snapshot": snapshot, "age_seconds": round(age, 3),
                           "stale": age > STALE_SECONDS, "received_at": record["received_at"],
                           "summary": record["summary"], "settlements": record["settlements"]})
        return {"schema": "world-monitor.state.v1", "stale_seconds": STALE_SECONDS, "worlds": worlds}


class Handler(BaseHTTPRequestHandler):
    server_version = "WorldMonitor/1"

    def setup(self):
        super().setup()
        self.connection.settimeout(3)

    def allowed(self):
        hosts = {f"127.0.0.1:{PORT}", f"localhost:{PORT}"}
        origin = self.headers.get("Origin")
        return self.headers.get("Host") in hosts and (origin is None or origin == "http://" + self.headers.get("Host", ""))

    def respond(self, status, body, content_type="application/json; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'")
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            pass

    def error(self, status, message):
        self.respond(status, json.dumps({"error": message}).encode())

    def do_GET(self):
        if not self.allowed():
            return self.error(403, "Loopback same-origin requests only")
        if self.path == "/api/state":
            return self.respond(200, json.dumps(self.server.store.state(), ensure_ascii=False,
                                               allow_nan=False, separators=(",", ":")).encode("utf-8"))
        routes = {"/": ("index.html", "text/html; charset=utf-8"),
                  "/app.js": ("app.js", "text/javascript; charset=utf-8"),
                  "/style.css": ("style.css", "text/css; charset=utf-8")}
        if self.path not in routes:
            return self.error(404, "Not found")
        filename, content_type = routes[self.path]
        self.respond(200, (DIST / filename).read_bytes(), content_type)

    def do_POST(self):
        if not self.allowed():
            return self.error(403, "Loopback same-origin requests only")
        if self.path != "/api/snapshots":
            return self.error(404, "Not found")
        if self.headers.get("Content-Type", "").split(";")[0].strip().lower() != "application/json":
            return self.error(415, "Expected application/json")
        length = self.headers.get("Content-Length", "")
        if self.headers.get("Transfer-Encoding") or not length.isdigit() or len(length) > 10:
            return self.error(400, "Content-Length required; chunked requests unsupported")
        size = int(length)
        if not 0 < size <= MAX_BODY:
            return self.error(413, "Maximum body size is 1 MiB")
        try:
            body = self.rfile.read(size)
            require(len(body) == size, "Incomplete body")
            data = decode(body)
        except (ValueError, UnicodeError, RecursionError, TypeError, OverflowError):
            return self.error(400, "Invalid snapshot; see protocol documentation")
        except (TimeoutError, ConnectionError):
            return self.error(408, "Request timeout")
        status = self.server.store.accept(data, size)
        if status != 202:
            return self.error(status, "Out-of-order sequence/epoch" if status == 409 else "Receiver capacity exceeded")
        self.respond(202, b'{"accepted":true}')

    def log_message(self, format, *args):
        # Polling is intentionally quiet; never log entity names or raw request bodies.
        pass


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, address, store):
        self.store = store
        self.slots = threading.BoundedSemaphore(8)
        super().__init__(address, Handler)

    def process_request(self, request, client_address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except Exception:
            self.slots.release()
            raise

    def process_request_thread(self, request, client_address):
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.slots.release()


if __name__ == "__main__":
    with Server(("127.0.0.1", PORT), Store()) as server:
        print(f"World monitor: http://127.0.0.1:{PORT} (Ctrl+C to stop)", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
