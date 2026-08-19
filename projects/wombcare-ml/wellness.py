# *Licensed to the Apache Software Foundation (ASF) under one
# *or more contributor license agreements.  See the NOTICE file
# *distributed with this work for additional information
# *regarding copyright ownership.  The ASF licenses this file
# *to you under the Apache License, Version 2.0 (the
# *"License"); you may not use this file except in compliance
# *with the License.  You may obtain a copy of the License at
#
# *  http://www.apache.org/licenses/LICENSE-2.0
#
# *Unless required by applicable law or agreed to in writing,
# *software distributed under the License is distributed on an
# *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# *KIND, either express or implied.  See the License for the
# *specific language governing permissions and limitations
# *under the License.
"""Wellness score, tiers, alert rule and BLE packet (FR-ML-7, Section 8).

This module is the *shared definition* of the dashboard number and the alert
logic. The firmware mirrors it in C; the Android app parses the packet it emits.
Keeping it in one place keeps the deck, firmware and app telling the same story.

  score   = calibrated P(well) in [0,1]  =  1 - P(abnormal)
  tiers   = Good / Watch / Alert  (thresholds from config)
  alert   = (low score) AND (sufficient signal quality) AND (persistence)
"""
from __future__ import annotations

import struct
from collections import deque
from dataclasses import dataclass
from enum import IntEnum

import config as C


class Tier(IntEnum):
    GOOD = 0
    WATCH = 1
    ALERT = 2


def wellness_score(p_abnormal: float) -> float:
    """Model outputs P(abnormal); the dashboard shows P(well)."""
    return float(min(1.0, max(0.0, 1.0 - p_abnormal)))


def tier_for(score: float) -> Tier:
    if score >= C.TIER_WATCH:
        return Tier.GOOD
    if score >= C.TIER_ALERT:
        return Tier.WATCH
    return Tier.ALERT


@dataclass
class AlertConfig:
    min_signal_quality: float = 0.60     # pct_valid floor to trust a window
    persistence: int = 2                  # consecutive low-score windows required


class AlertEngine:
    """Raises an Alert only when a low score *persists* over the resting window
    AND the signal is trustworthy -- WombCare's "prolonged below-threshold" rule,
    designed to avoid false alarms from a single noisy window (FR-ML-7)."""

    def __init__(self, cfg: AlertConfig | None = None):
        self.cfg = cfg or AlertConfig()
        self._recent: deque[bool] = deque(maxlen=self.cfg.persistence)

    def update(self, score: float, signal_quality: float) -> dict:
        tier = tier_for(score)
        trustworthy = signal_quality >= self.cfg.min_signal_quality
        is_low = tier == Tier.ALERT and trustworthy
        self._recent.append(is_low)
        alert = (len(self._recent) == self.cfg.persistence
                 and all(self._recent))
        return {
            "score": round(score, 3),
            "tier": tier.name,
            "signal_ok": trustworthy,
            "alert": bool(alert),
        }


# --- BLE packet (Section 8 integration contract) ----------------------------
PACKET_VERSION = 1

# bitfield layout for the flags byte
_TIER_MASK = 0b11           # bits 0-1: tier
_ALERT_BIT = 1 << 2         # bit 2: alert latched
_SIGLOW_BIT = 1 << 3        # bit 3: signal low
_MODE_BIT = 1 << 4          # bit 4: mode (0=sleep, 1=active)


def crc8(data: bytes, poly: int = 0x07) -> int:
    """Dallas/Maxim-style CRC-8 over the first 7 bytes (link integrity)."""
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ poly) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def encode_packet(score: float, tier: Tier, *, fhr_bpm: int, signal_quality: int,
                  timestamp: int, alert: bool, signal_low: bool,
                  active: bool) -> bytes:
    """Compact 8-byte wellness packet, versioned so firmware can evolve (S8).

    Layout: ver(1) score(1) flags(1) fhr(1) sigq(1) timestamp(2, LE) crc8(1)
    """
    flags = (int(tier) & _TIER_MASK)
    flags |= _ALERT_BIT if alert else 0
    flags |= _SIGLOW_BIT if signal_low else 0
    flags |= _MODE_BIT if active else 0
    body = struct.pack(
        "<BBBBBH",
        PACKET_VERSION,
        max(0, min(255, round(score * 255))),
        flags,
        max(0, min(255, int(fhr_bpm))),
        max(0, min(100, int(signal_quality))),
        timestamp & 0xFFFF,
    )
    return body + bytes([crc8(body)])


def decode_packet(buf: bytes) -> dict:
    """Reference decoder (the Android app mirrors this in Kotlin)."""
    body, crc = buf[:7], buf[7]
    ver, score_b, flags, fhr, sigq, ts = struct.unpack("<BBBBBH", body)
    return {
        "version": ver,
        "score": score_b / 255.0,
        "tier": Tier(flags & _TIER_MASK).name,
        "alert": bool(flags & _ALERT_BIT),
        "signal_low": bool(flags & _SIGLOW_BIT),
        "mode": "active" if flags & _MODE_BIT else "sleep",
        "fhr_bpm": fhr,
        "signal_quality": sigq,
        "timestamp": ts,
        "crc_ok": crc == crc8(body),
    }


if __name__ == "__main__":
    eng = AlertEngine()
    # Simulate a record sliding toward distress with one noisy blip.
    seq = [(0.85, 0.95), (0.30, 0.30), (0.25, 0.92), (0.20, 0.92), (0.80, 0.95)]
    for i, (sc, q) in enumerate(seq):
        st = eng.update(sc, q)
        pkt = encode_packet(sc, Tier[st["tier"]], fhr_bpm=120, signal_quality=int(q*100),
                            timestamp=i, alert=st["alert"], signal_low=not st["signal_ok"],
                            active=True)
        dec = decode_packet(pkt)
        assert len(pkt) == 8 and abs(dec["score"] - sc) < 0.01
        print(f"  score={sc:.2f} q={q:.2f} -> {st}  | packet {pkt.hex()} -> tier {dec['tier']}")
    print("OK packet round-trips, 8 bytes, alert persistence enforced")
