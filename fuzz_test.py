"""
fuzz_test.py — công cụ KIỂM THỬ LỖI và đo đạc (SV4).

Chạy có board:      python fuzz_test.py --port COM7 --baud 115200
Chạy không có board: python fuzz_test.py --offline
                     (dùng chính parser Python làm "board ảo" — nhóm vẫn có
                      đủ số liệu cho báo cáo khi phần cứng chưa xong)

Sinh ra bảng số liệu dán thẳng vào báo cáo:
  A. Tỉ lệ khung đúng khi đường truyền sạch
  B. Tỉ lệ phát hiện lỗi khi cố tình làm hỏng khung (5 kiểu hỏng)
  C. So sánh khả năng phát hiện lỗi của SUM8 / CRC8 / CRC16
  D. Đo streaming: thông lượng thực, số mẫu mất, tải đường truyền
"""
from __future__ import annotations

import argparse
import random
import struct
import sys
import time

import v01proto as P

random.seed(20260922)


# ===========================================================================
# Các kiểu làm hỏng khung
# ===========================================================================
def corrupt_bitflip(raw: bytes, n: int = 1) -> bytes:
    """Đảo đúng @p n bit KHÁC NHAU (không đụng 2 byte SOF).

    Phải chọn n vị trí phân biệt: nếu bốc trùng một bit hai lần thì hai phép
    đảo triệt tiêu nhau, khung vẫn nguyên vẹn, và phép đo tỉ lệ phát hiện lỗi
    sẽ ra kết quả sai (khung "hỏng" lọt lưới thực ra là khung tốt).
    """
    nbits = (len(raw) - 2) * 8
    picks = random.sample(range(nbits), min(n, nbits))
    b = bytearray(raw)
    for p in picks:
        b[2 + p // 8] ^= 1 << (p % 8)
    return bytes(b)


def corrupt_drop_byte(raw: bytes) -> bytes:
    b = bytearray(raw)
    del b[random.randrange(2, len(b))]
    return bytes(b)


def corrupt_bad_len(raw: bytes) -> bytes:
    b = bytearray(raw)
    b[2] = 0xFF                                   # LEN vượt trần
    return bytes(b)


def corrupt_truncate(raw: bytes) -> bytes:
    return raw[:max(3, len(raw) - random.randint(1, 3))]


def insert_junk(raw: bytes, n: int) -> bytes:
    junk = bytes(random.randrange(256) for _ in range(n))
    return junk + raw


CORRUPTIONS = [
    ("Đảo 1 bit",            lambda r: corrupt_bitflip(r, 1)),
    ("Đảo 2 bit",            lambda r: corrupt_bitflip(r, 2)),
    ("Mất 1 byte giữa khung", corrupt_drop_byte),
    ("LEN sai (0xFF)",        corrupt_bad_len),
    ("Cắt cụt đuôi khung",    corrupt_truncate),
]


# ===========================================================================
# "Board ảo": chỉ chạy parser, không cần phần cứng
# ===========================================================================
class VirtualBoard:
    def __init__(self) -> None:
        self.parser = P.Parser()

    def feed(self, raw: bytes) -> int:
        """@return số khung được CHẤP NHẬN."""
        return sum(1 for _ in self.parser.feed(raw))

    @property
    def stats(self) -> P.Stats:
        return self.parser.stats


# ===========================================================================
def bar(title: str) -> None:
    print("\n" + "=" * 74)
    print(title)
    print("=" * 74)


# ---------------------------------------------------------------------------
def test_clean(board, n: int = 2000) -> None:
    bar(f"A. ĐƯỜNG TRUYỀN SẠCH — gửi {n} khung hợp lệ")
    ok = 0
    for i in range(n):
        data = bytes(random.randrange(256) for _ in range(random.randint(0, 32)))
        raw = P.build(i & 0xFF, P.Cmd.ECHO, data)
        ok += board.feed(raw)
    print(f"  Khung gửi   : {n}")
    print(f"  Khung nhận  : {ok}")
    print(f"  Tỉ lệ đúng  : {100.0 * ok / n:.3f} %")
    print("  -> Kỳ vọng 100.000 %. Nếu thấp hơn là có lỗi phần cứng/baud.")


def test_corruption(board, n: int = 2000) -> None:
    bar(f"B. KHUNG HỎNG — mỗi kiểu {n} khung, đo TỈ LỆ PHÁT HIỆN")
    print(f"  {'Kiểu làm hỏng':<24}{'Lọt lưới':>10}{'Phát hiện':>12}"
          f"{'Tỉ lệ':>12}")
    print("  " + "-" * 58)
    for name, fn in CORRUPTIONS:
        missed = 0
        for i in range(n):
            data = bytes(random.randrange(256) for _ in range(16))
            raw = fn(P.build(i & 0xFF, P.Cmd.ECHO, data))
            # Thêm một khung PING hợp lệ phía sau để parser có cơ hội đồng bộ
            # lại. Mỗi lần thử dùng parser MỚI, nếu không trạng thái dở dang
            # của lần trước sẽ lẫn sang lần sau và làm sai số liệu.
            raw += P.build((i + 1) & 0xFF, P.Cmd.PING, b"")
            got = sum(1 for _ in P.Parser().feed(raw))
            if got > 1:                 # >1 nghĩa là khung hỏng cũng lọt
                missed += got - 1
        print(f"  {name:<24}{missed:>10}{n - missed:>12}"
              f"{100.0 * (n - missed) / n:>11.3f} %")
    print("  -> 'Lọt lưới' là khung ĐÃ HỎNG nhưng vẫn được chấp nhận.")
    print("     Với CRC16 con số này phải xấp xỉ 0.")


def test_resync(board, n: int = 500) -> None:
    bar(f"C. TỰ ĐỒNG BỘ LẠI — chèn rác ngẫu nhiên trước mỗi khung")
    for junk_len in (0, 1, 4, 16, 64):
        ok = 0
        for i in range(n):
            raw = insert_junk(P.build(i & 0xFF, P.Cmd.PING, b"\x01\x02"),
                              junk_len)
            ok += board.feed(raw)
        print(f"  Rác {junk_len:>3} byte/khung  ->  khôi phục "
              f"{ok:>4}/{n}  ({100.0 * ok / n:6.2f} %)")
    print("  -> Parser phải tìm lại được khung dù có bao nhiêu rác phía trước.")


def compare_checksums(n: int = 20000) -> None:
    bar("D. SO SÁNH SUM8 / CRC8 / CRC16 (mô phỏng, đảo 1–4 bit)")
    print(f"  {'Kiểu':<10}{'Số byte':>9}{'Bỏ sót':>10}{'Tỉ lệ phát hiện':>20}")
    print("  " + "-" * 50)
    saved = P.CKS_MODE
    for mode, name in ((P.CKS_SUM8, "SUM-8"),
                       (P.CKS_CRC8, "CRC-8"),
                       (P.CKS_CRC16, "CRC-16")):
        P.CKS_MODE = mode
        P.CKS_LEN = 2 if mode == P.CKS_CRC16 else 1
        missed = 0
        for i in range(n):
            data = bytes(random.randrange(256) for _ in range(16))
            good = P.build(i & 0xFF, P.Cmd.ECHO, data)
            bad = corrupt_bitflip(good, random.randint(1, 4))
            if bad == good:
                continue
            if list(P.Parser().feed(bad)):
                missed += 1
        print(f"  {name:<10}{P.CKS_LEN:>9}{missed:>10}"
              f"{100.0 * (n - missed) / n:>19.4f} %")
    P.CKS_MODE = saved
    P.CKS_LEN = 2 if saved == P.CKS_CRC16 else 1
    print("  -> Đây chính là lý do phần MỞ RỘNG dùng CRC16 thay cho checksum.")


# ===========================================================================
# Phần chỉ chạy được khi CÓ board thật
# ===========================================================================
def test_live_nack(dev: P.Device) -> None:
    bar("E. BOARD THẬT — board phải TRẢ NACK cho khung hỏng")
    got = {"n": 0}
    dev.on_frame = lambda f: got.__setitem__(
        "n", got["n"] + 1) if f.is_nack else None

    cases = [
        ("sai CRC",        corrupt_bitflip(P.build(1, P.Cmd.PING), 1)),
        ("LEN quá lớn",    bytes([P.SOF1, P.SOF2, 0xFF, 2, 1, 0, 0])),
        ("lệnh lạ",        P.build(3, 0x7A)),
        ("tham số sai",    P.build(4, P.Cmd.LED_SET, b"\x09\x09")),
    ]
    for name, raw in cases:
        before = got["n"]
        dev.send_raw(raw)
        time.sleep(0.25)
        print(f"  {name:<16}-> NACK nhận được: {got['n'] - before}")
    dev.on_frame = None
    print("  -> Cả 4 trường hợp phải có NACK (trừ khi rơi vào cửa sổ chặn")
    print("     tốc độ NACK 50 ms — gửi thưa ra rồi thử lại).")


def test_live_stream(dev: P.Device, rate: int, batch: int,
                     seconds: float = 5.0) -> dict:
    got = {"n": 0, "gap": 0, "last": None, "frames": 0}

    def on_stream(idx: int, vals: list) -> None:
        if got["last"] is not None and idx != got["last"]:
            got["gap"] += 1
        got["last"] = idx + len(vals)
        got["n"] += len(vals)
        got["frames"] += 1

    dev.on_stream = on_stream
    dev.stats_reset()
    dev.stream_start(P.STREAM_SRC_TEST, rate, batch)
    t0 = time.monotonic()
    time.sleep(seconds)
    dev.stream_stop()
    dt = time.monotonic() - t0
    dev.on_stream = None
    time.sleep(0.2)

    frame_bytes = P.HDR_LEN + 5 + 2 * batch + P.CKS_LEN
    load = 100.0 * (got["frames"] / dt) * frame_bytes * 10 / dev.ser.baudrate
    return {
        "rate": rate, "batch": batch,
        "meas": got["n"] / dt, "gap": got["gap"],
        "frames_s": got["frames"] / dt, "load": load,
        "stats": dev.stats(),
    }


def test_live_stream_sweep(dev: P.Device) -> None:
    bar("F. BOARD THẬT — đo streaming (nguồn sóng sin, không cần cảm biến)")
    print(f"  {'Đặt (Hz)':>9}{'Gom':>6}{'Thực đo (mẫu/s)':>18}"
          f"{'Khung/s':>10}{'Mất':>7}{'Tải UART':>11}")
    print("  " + "-" * 62)
    for rate, batch in ((100, 1), (100, 20), (1000, 20),
                        (5000, 40), (10000, 60)):
        try:
            r = test_live_stream(dev, rate, batch, 4.0)
        except Exception as e:
            print(f"  {rate:>9}{batch:>6}   LỖI: {e}")
            continue
        print(f"  {r['rate']:>9}{r['batch']:>6}{r['meas']:>18.1f}"
              f"{r['frames_s']:>10.1f}{r['gap']:>7}{r['load']:>10.1f} %")
    print("  -> 'Mất' là số lần chỉ số mẫu bị nhảy (một khung không tới nơi).")
    print("     Khi 'Tải UART' vượt ~80 %, số mất sẽ tăng vọt: đó là giới hạn")
    print("     thật của baud rate đang dùng, hãy ghi lại làm kết luận.")


# ===========================================================================
def main() -> int:
    ap = argparse.ArgumentParser(description="Kiểm thử giao thức V01")
    ap.add_argument("--port", help="cổng COM / /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--offline", action="store_true",
                    help="chạy trên board ảo, không cần phần cứng")
    ap.add_argument("-n", type=int, default=2000, help="số khung mỗi phép thử")
    a = ap.parse_args()

    if not a.offline and not a.port:
        ap.error("cần --port hoặc --offline")

    print("╔" + "═" * 72 + "╗")
    print("║ KIỂM THỬ GIAO THỨC KHUNG UART V01 — v1.1"
          + " " * 31 + "║")
    print("╚" + "═" * 72 + "╝")
    print(f"Chế độ checksum : {['SUM8','CRC8','CRC16'][P.CKS_MODE]}")
    print(f"LEN tối đa      : {P.MAX_DATA} byte")

    board = VirtualBoard()
    test_clean(board, a.n)
    test_corruption(board, a.n)
    test_resync(board, min(a.n, 500))
    compare_checksums(min(a.n * 10, 20000))

    if a.offline:
        print("\n(Chế độ offline: bỏ qua các phép thử cần board thật.)")
        return 0

    try:
        dev = P.Device(a.port, a.baud)
    except Exception as e:
        print(f"\nKhông mở được cổng {a.port}: {e}")
        return 1
    try:
        pv, ck, ma, mi = dev.version()
        print(f"\nBoard: giao thức v{pv >> 4}.{pv & 15}, "
              f"checksum {['SUM8','CRC8','CRC16'][ck]}, fw {ma}.{mi}")
        if ck != P.CKS_MODE:
            print("  !! Firmware và PC đang dùng CHECKSUM KHÁC NHAU — "
                  "sửa CKS_MODE trong v01proto.py")
            return 1
        test_live_nack(dev)
        test_live_stream_sweep(dev)
    finally:
        dev.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
