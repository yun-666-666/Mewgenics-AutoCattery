"""AutoCattery local breeding workbench. Run with Python; no standalone EXE."""

import argparse
from collections import deque
from dataclasses import asdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import secrets
import threading
import time
from urllib.parse import urlparse
import webbrowser

from breeding_web_preferences import load_assists, save_assists

from breeding_daily import NativeDailySimulation, SimulationConfig
from breeding_save import copy_database, discover_saves, export_population, read_snapshot
from breeding_save_application import (apply_result, game_is_running, require_game_closed,
                                      result_preview, save_stamp)


class Workbench:
    def __init__(self, game, output):
        self.game = Path(game).resolve()
        self.output = Path(output).resolve()
        self.preferences_path = self.output / "workbench-preferences.json"
        self.preferences = load_assists(self.preferences_path)
        self.token = secrets.token_urlsafe(24)
        self.saves = discover_saves()
        self.lock = threading.Lock()
        self.cancel = threading.Event()
        self.job = {"status": "idle", "history": []}
        self.thread = None

    def state(self):
        with self.lock:
            return {"game": str(self.game), "game_running": game_is_running(), "saves": [
                {"id": i, "name": f"{s.parent.parent.name} / {s.name}", "path": str(s),
                 "modified": s.stat().st_mtime} for i, s in enumerate(self.saves)],
                "job": json.loads(json.dumps(self.job)), "token": self.token,
                "preferences": dict(self.preferences)}

    def remember_assists(self, options):
        with self.lock:
            self.preferences = save_assists(self.preferences_path, options)

    def snapshot(self, index):
        source = self.saves[int(index)]
        snapshot = read_snapshot(source, self.game / "resources.gpak")
        snapshot["source_path"] = str(source)
        return snapshot

    def start(self, options):
        with self.lock:
            require_game_closed()
            if self.thread and self.thread.is_alive():
                raise ValueError("已有培养任务正在运行，请先取消或等待完成")
            source = self.saves[int(options["save"])]
            days = int(options.get("days", 365))
            pairs = int(options.get("pairs", 2))
            refill = int(options.get("food_refill", 0))
            assist = options.get("assist", self.preferences["assist"])
            food_assist = options.get("food_assist", self.preferences["food_assist"])
            population_limit = int(options.get("population_limit", 150))
            if not 1 <= days <= 10000 or not 1 <= pairs <= 5 or not 0 <= refill <= 1000000:
                raise ValueError("培养天数、配对数或补食量超出范围")
            if type(assist) is not bool or type(food_assist) is not bool:
                raise ValueError("辅助开关必须为布尔值")
            if not 4 <= population_limit <= 1000:
                raise ValueError("猫群数量上限须在4到1000之间")
            self.cancel.clear()
            self.job = {"status": "running", "history": [], "elapsed": 0,
                        "max_days": days, "source": str(source), "assist": assist,
                        "food_assist": food_assist,
                        "population_limit": population_limit,
                        "pairs": pairs,
                        "food_refill": refill, "message": "正在读取存档和游戏资源"}
            self.thread = threading.Thread(target=self.run,
                args=(source, days, pairs, refill, assist, food_assist),
                kwargs={"population_limit": population_limit}, daemon=True)
            self.thread.start()

    def apply(self, options):
        if options.get("confirmed") is not True:
            raise ValueError("请先确认预览中的存档、增加猫和移出猫")
        with self.lock:
            if options.get("result") != self.job.get("report"):
                raise ValueError("结果已改变，请重新查看当前预览后确认")
            applied = apply_result(self.job)
            self.job["message"] = "已写回对应存档；现在可以启动游戏加载该槽位"
            return applied

    def update(self, **values):
        with self.lock:
            self.job.update(values)

    def run(self, source, days, pairs, refill, assist, food_assist=False, stop_when_stable=True,
            population_limit=150):
        simulation = None
        folder = self.output / (time.strftime("%Y%m%d-%H%M%S") + "-" + secrets.token_hex(3))
        folder.mkdir(parents=True)
        try:
            require_game_closed()
            stamp = save_stamp(source)
            working = folder / "source.sav"
            copy_database(source, working)
            if save_stamp(source) != stamp:
                raise ValueError("复制期间原存档发生变化，请退出游戏并重新培养")
            self.update(source=str(Path(source).resolve()), source_stamp=stamp)
            snapshot = read_snapshot(working, self.game / "resources.gpak")
            config = SimulationConfig(breeding_pairs=pairs, daily_food_refill=refill or None,
                                      offspring_all_seven_assist=assist, food_supply_assist=food_assist,
                                      population_limit=population_limit)
            simulation = NativeDailySimulation(self.game / "Mewgenics.exe", self.game / "resources.gpak",
                                               working, snapshot, config)
            self.update(message="按真实家具效果逐日繁育", initial_day=simulation.day,
                        initial_population=len(simulation.cats), config=asdict(config),
                        rooms=snapshot["rooms"])
            window = deque(maxlen=30)
            stable = False
            history = []
            for elapsed in range(1, days + 1):
                require_game_closed()
                if self.cancel.is_set():
                    break
                result = simulation.step()
                replacements = simulation.replacement_pairs()
                if replacements >= 2:
                    window.append((len(result.births), result.all_seven_births))
                else:
                    window.clear()
                births = sum(x for x, _ in window)
                all7 = sum(y for _, y in window)
                ratio = all7 / births if births else 0
                item = asdict(result)
                item.update(elapsed=elapsed, population=len(simulation.cats), replacement_pairs=replacements,
                            planned_pairs=len(result.selected_pairs), target_pairs=pairs,
                            all_seven_population=sum(simulation._all_seven(c) and not simulation._dead(c)
                                                     for c in simulation.cats.values()),
                            window_days=len(window), window_births=births, window_ratio=ratio)
                history.append(item)
                self.update(elapsed=elapsed, latest=item, history=list(history),
                            message=f"已模拟 {elapsed} 天；原存档未修改")
                if len(window) == 30 and births > 0 and ratio >= .95:
                    stable = True
                    if stop_when_stable:
                        break
            exported = None
            original_ids = {cat["id"] for cat in snapshot["cats"]}
            if any(simulation._all_seven(c) and key not in original_ids
                   for key, c in simulation.cats.items()):
                try:
                    exported = export_population(simulation, folder / "trained.sav", naming_source=source)
                except ValueError as error:
                    self.update(export_error=str(error))
            status = "cancelled" if self.cancel.is_set() else "completed"
            preview = result_preview(source, snapshot, exported, self.game) if exported else None
            report = {"status": status, "passed": stable, "criterion": "30日窗口内后代至少95%遗传全七，每日保有2对可替换种猫",
                      "config": asdict(config), "history": history, "export": exported,
                      "source": str(source), "rooms": snapshot["rooms"], "preview": preview,
                      "game_validation": "尚需玩家加载导出存档验证；模拟不等于游戏验收"}
            (folder / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            self.update(status=status, passed=stable, export=exported, preview=preview,
                        report=str(folder / "report.json"),
                        message="模拟窗口达标" if stable else "模拟已停止；尚未达到持续繁育标准")
        except Exception as error:
            self.update(status="failed", message=str(error))
        finally:
            if simulation is not None:
                simulation.close()


def make_handler(app):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, format, *args):
            pass

        def send(self, code, body, content_type="application/json; charset=utf-8"):
            if not isinstance(body, bytes):
                body = json.dumps(body, ensure_ascii=False).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def local_request(self):
            return self.headers.get("Host") == f"127.0.0.1:{self.server.server_port}"

        def do_GET(self):
            if not self.local_request():
                self.send(403, {"error": "仅允许本机界面访问"})
                return
            path = urlparse(self.path).path
            try:
                if path == "/":
                    self.send(200, Path(__file__).with_name("breeding_web.html").read_bytes(), "text/html; charset=utf-8")
                elif path == "/api/state":
                    self.send(200, app.state())
                elif path.startswith("/api/snapshot/"):
                    self.send(200, app.snapshot(int(path.rsplit("/", 1)[1])))
                elif path in ("/download/save", "/download/report"):
                    job = app.state()["job"]
                    file = (job.get("export") or {}).get("file") if path.endswith("save") else job.get("report")
                    if not file:
                        raise ValueError("没有可下载的结果")
                    self.send(200, Path(file).read_bytes(), "application/octet-stream")
                else:
                    self.send(404, {"error": "页面不存在"})
            except (ValueError, IndexError, OSError, KeyError, TypeError) as error:
                self.send(400, {"error": str(error)})

        def do_POST(self):
            if not self.local_request() or self.headers.get("X-AutoCattery-Token") != app.token:
                self.send(403, {"error": "请从本机培养页面操作"})
                return
            try:
                size = int(self.headers.get("Content-Length", 0))
                if not 0 <= size <= 16384:
                    raise ValueError("请求过长")
                options = json.loads(self.rfile.read(size) or b"{}")
                if self.path == "/api/start":
                    app.start(options)
                elif self.path == "/api/preferences":
                    app.remember_assists(options)
                elif self.path == "/api/cancel":
                    app.cancel.set()
                elif self.path == "/api/apply":
                    applied = app.apply(options)
                    self.send(200, {"ok": True, "application": applied})
                    return
                else:
                    raise ValueError("未知操作")
                self.send(200, {"ok": True})
            except (ValueError, IndexError, KeyError, TypeError, OSError) as error:
                self.send(400, {"error": str(error)})
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    default_game = next((p for p in Path(__file__).resolve().parents if (p / "Mewgenics.exe").is_file()), Path.cwd())
    parser.add_argument("--game", type=Path, default=default_game)
    parser.add_argument("--output", type=Path, default=Path.home() / "Documents" / "AutoCattery")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()
    if not (args.game / "Mewgenics.exe").is_file() or not (args.game / "resources.gpak").is_file():
        parser.error("未找到游戏目录，请使用 --game 指定包含 Mewgenics.exe 的目录")
    app = Workbench(args.game, args.output)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(app))
    url = f"http://127.0.0.1:{server.server_port}"
    print(url, flush=True)
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        app.cancel.set()
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
