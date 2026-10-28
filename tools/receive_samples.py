"""Receive TXS1 UDP packets, report continuity, optionally save raw datagrams.

PC's wired adapter must have 192.168.10.2/24; this script changes no PC settings.
Example: python receive_samples.py --seconds 20 --output capture.txs
"""
import argparse
import json
import socket
import struct
import time


def decode(data):
    """Validate the wire header and return raw ADC/digital records, without resampling."""
    if len(data) < 48 or data[:4] != b'TXS1' or data[4:6] != b'\x01\x30':
        raise ValueError('Unknown packet format')
    count = struct.unpack_from('<H', data, 6)[0]
    if not 1 <= count <= 256 or len(data) != 48 + count * 4:
        raise ValueError('Invalid packet length')
    session, sequence = struct.unpack_from('<II', data, 8)
    first, period = struct.unpack_from('<QI', data, 16)
    records = struct.unpack_from('<' + 'I' * count, data, 48)
    if any(v & 0xF8000000 for v in records):
        raise ValueError('Reserved bits set')
    return dict(session=session, sequence=sequence, first_sample=first,
                period_ns=period, pwm_hz=struct.unpack_from('<III', data, 28),
                adc_rank_skew_ns=struct.unpack_from('<H', data, 46)[0], records=records)


def main():
    """Listen on UDP 5000 and distinguish packet gaps from local receive errors."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=20)
    parser.add_argument('--bind', default='0.0.0.0')
    parser.add_argument('--output')
    args = parser.parse_args()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8 * 1024 * 1024)
    sock.bind((args.bind, 5000))
    sock.settimeout(0.5)
    stats = dict(packets=0, samples=0, bytes=0, missing=0, reordered=0, invalid=0, sessions=0)
    previous = session = None
    output = open(args.output, 'wb') if args.output else None
    start = time.monotonic()
    try:
        while time.monotonic() - start < args.seconds:
            try:
                data, peer = sock.recvfrom(2048)
            except socket.timeout:
                continue
            try:
                packet = decode(data)
            except ValueError:
                stats['invalid'] += 1
                continue
            if packet['session'] != session:
                session, previous = packet['session'], None
                stats['sessions'] += 1
            sequence = packet['sequence']
            if previous is not None:
                delta = (sequence - previous) & 0xFFFFFFFF
                if 0 < delta < 0x80000000:
                    stats['missing'] += delta - 1
                else:
                    stats['reordered'] += 1
            if previous is None or 0 < ((sequence - previous) & 0xFFFFFFFF) < 0x80000000:
                previous = sequence
            stats['packets'] += 1
            stats['samples'] += len(packet['records'])
            stats['bytes'] += len(data)
            stats['last_pwm_hz'] = packet['pwm_hz']
            stats['peer'] = peer[0]
            if output:
                output.write(struct.pack('<H', len(data)) + data)
    finally:
        if output:
            output.close()
        sock.close()
    elapsed = time.monotonic() - start
    stats.update(seconds=elapsed, received_samples_per_second=stats['samples']/elapsed,
                 udp_payload_mbit_per_second=stats['bytes']*8/elapsed/1e6)
    print(json.dumps(stats, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
