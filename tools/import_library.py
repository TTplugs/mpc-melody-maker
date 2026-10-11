#!/usr/bin/env python3
"""Pack privately supplied SMF files into a local bank. Python standard library only."""
import argparse
import collections
import csv
import json
import math
import os
import struct
from pathlib import Path

PPQ = 960
MAX_NOTES = 2048
PROFILES = (
    (6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88),
    (6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17),
)


def vlq(data, pos):
    value = 0
    for _ in range(4):
        if pos >= len(data):
            raise ValueError('truncated MIDI VLQ')
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 127)
        if not byte & 128:
            return value, pos
    raise ValueError('invalid MIDI VLQ')


def read_midi(path):
    data = path.read_bytes()
    if len(data) < 14 or data[:4] != b'MThd':
        raise ValueError('not an SMF file')
    header_size = struct.unpack_from('>I', data, 4)[0]
    fmt, ntracks, division = struct.unpack_from('>HHH', data, 8)
    if header_size < 6 or fmt > 1 or division == 0 or division & 0x8000:
        raise ValueError('unsupported MIDI format or SMPTE timing')
    cursor, notes, end_tick = 8 + header_size, [], 0
    for _ in range(ntracks):
        if cursor + 8 > len(data) or data[cursor:cursor + 4] != b'MTrk':
            raise ValueError('missing MIDI track')
        size = struct.unpack_from('>I', data, cursor + 4)[0]
        track = data[cursor + 8:cursor + 8 + size]
        if len(track) != size:
            raise ValueError('truncated MIDI track')
        cursor += 8 + size
        pos, tick, running = 0, 0, None
        active = collections.defaultdict(collections.deque)
        while pos < len(track):
            delta, pos = vlq(track, pos)
            tick += delta
            if pos >= len(track):
                raise ValueError('missing MIDI event')
            status = track[pos]
            if status & 128:
                pos += 1
                if status < 0xf0:
                    running = status
            elif running is not None:
                status = running
            else:
                raise ValueError('MIDI running status without prior channel event')
            if status == 0xff:
                if pos >= len(track):
                    raise ValueError('missing MIDI meta type')
                kind = track[pos]
                pos += 1
                length, pos = vlq(track, pos)
                if pos + length > len(track):
                    raise ValueError('truncated MIDI meta event')
                pos += length
                if kind == 0x2f:
                    break
                continue
            if status in (0xf0, 0xf7):
                running = None
                length, pos = vlq(track, pos)
                if pos + length > len(track):
                    raise ValueError('truncated MIDI sysex')
                pos += length
                continue
            if status >= 0xf0:
                raise ValueError('unsupported system event in SMF')
            count = 1 if status & 0xf0 in (0xc0, 0xd0) else 2
            if pos + count > len(track):
                raise ValueError('truncated MIDI channel event')
            values = track[pos:pos + count]
            if any(v > 127 for v in values):
                raise ValueError('invalid MIDI data byte')
            pos += count
            kind, channel = status & 0xf0, status & 15
            if kind == 0x90 and values[1]:
                active[channel, values[0]].append((tick, values[1]))
            elif kind == 0x80 or (kind == 0x90 and not values[1]):
                stack = active[channel, values[0]]
                if stack:
                    start, velocity = stack.popleft()
                    if tick > start:
                        notes.append((start, tick - start, values[0], velocity))
        for (_, pitch), stack in active.items():
            for start, velocity in stack:
                if tick > start:
                    notes.append((start, tick - start, pitch, velocity))
        end_tick = max(end_tick, tick)
    converted = sorted((round(s * PPQ / division), max(1, round(d * PPQ / division)), p, v)
                       for s, d, p, v in notes)
    if len(converted) > MAX_NOTES:
        raise ValueError(f'too many notes ({len(converted)})')
    end_tick = max(round(end_tick * PPQ / division), max((s + d for s, d, _, _ in converted), default=0))
    if end_tick > PPQ * 256:
        raise ValueError('clip exceeds 64 bars')
    return converted, max(PPQ * 4, math.ceil(end_tick / (PPQ * 4)) * PPQ * 4)


def estimate_key(notes):
    histogram = [0.0] * 12
    for _, duration, pitch, _ in notes:
        histogram[pitch % 12] += min(duration, PPQ * 4)
    average = sum(histogram) / 12
    best = (-math.inf, 0, 1)
    for scale, profile in enumerate(PROFILES):
        mean = sum(profile) / 12
        norm = math.sqrt(sum((v - mean) ** 2 for v in profile))
        for key in range(12):
            score = sum((histogram[(i + key) % 12] - average) * (profile[i] - mean) for i in range(12)) / norm
            best = max(best, (score, key, scale))
    return best[1:]


def fixed_text(text, size):
    return text.encode('utf-8')[:size - 1].ljust(size, b'\0')


def parse_small_int(row, key, maximum):
    try:
        return min(maximum, max(0, int(row.get(key) or 0)))
    except ValueError:
        return 0


def pack(source, output):
    source = source.resolve(strict=True)
    output = output.resolve()
    if source == output or source in output.parents:
        raise ValueError('output must be outside the source library')
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + '.new')
    count, issues = 0, []
    speed_counts, styles = collections.Counter(), collections.Counter()
    rows, seen = [], set()
    for filename in ('index_global.csv', 'index_extra_clasificadas.csv'):
        index_path = source / filename
        if not index_path.exists():
            continue
        with index_path.open(encoding='utf-8-sig', newline='') as index:
            for row in csv.DictReader(index):
                melody = row.get('melody', '').strip().replace('\\', '/')
                if melody in seen:
                    continue
                seen.add(melody)
                if filename != 'index_global.csv':
                    row['speed_folder'] = row.get('velocidad_mas_probable') or row.get('velocidad_estimada') or 'medium'
                    # Additional annotations are uncertain; retain music without claiming pad labels are reliable.
                    row['parametros_fiables'] = 'no'
                rows.append(row)
    with temporary.open('wb') as bank:
        bank.write(struct.pack('<8sII', b'TTMMB001', PPQ, 0))
        for row in rows:
            try:
                tracks, loop = [], 0
                for part in ('melody', 'chord', 'bass'):
                    relative = row.get(part, '').strip()
                    if not relative:
                        tracks.append([])
                        continue
                    path = (source / relative).resolve(strict=True)
                    if not path.is_relative_to(source):
                        raise ValueError('MIDI path escapes source directory')
                    notes, length = read_midi(path)
                    tracks.append(notes)
                    loop = max(loop, length)
                if not tracks[0]:
                    raise ValueError('empty melody')
                key, scale = estimate_key(tracks[0])
                speed = row.get('speed_folder', row.get('speed', 'medium'))
                if speed not in ('slow', 'medium', 'fast'):
                    raise ValueError('unknown speed category')
                reliable = row.get('parametros_fiables', 'no') == 'yes'
                # The source uses "si" in some index versions.
                reliable = reliable or row.get('parametros_fiables', '').lower() in ('si', 'sí', 'true', '1')
                props = bytes((('slow', 'medium', 'fast').index(speed),
                               parse_small_int(row, 'pad_simple_complex', 4),
                               parse_small_int(row, 'pad_light_dark', 3), int(reliable), key, scale,
                               int(row.get('groove_mode') == 'swing'), 0))
                bank.write(fixed_text(row['id'], 48) + fixed_text(row.get('style', ''), 32) + props)
                bank.write(struct.pack('<IIII', loop, *(len(t) for t in tracks)))
                for notes in tracks:
                    for start, duration, pitch, velocity in notes:
                        bank.write(struct.pack('<IIBBH', start, duration, pitch, velocity, 0))
                count += 1
                speed_counts[speed] += 1
                styles[row.get('style', '')] += 1
            except (OSError, ValueError, KeyError, struct.error) as exc:
                issues.append({'id': row.get('id', '?'), 'error': str(exc)})
        bank.seek(12)
        bank.write(struct.pack('<I', count))
    if not count:
        temporary.unlink()
        raise ValueError('no valid melody clips were imported')
    os.replace(temporary, output)
    report = {'format': 'TTMMB001', 'preset': 'Hip Hop', 'clips': count, 'speed': dict(speed_counts),
              'substyles': dict(sorted(styles.items())), 'skipped': issues, 'bytes': output.stat().st_size,
              'scope': 'index_global.csv and index_extra_clasificadas.csv; deduplicated by melody path',
              'key_detection': 'estimated major/minor, not verified source annotations'}
    output.with_suffix('.report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('preset', 'clips', 'speed', 'bytes')}, ensure_ascii=False))
    print(f'Skipped: {len(issues)}. Bank and report contain private library data.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    pack(args.source, args.output)
