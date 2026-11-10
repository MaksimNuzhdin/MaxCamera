"""Bounded UDP drain queue so image processing cannot stall socket reception."""
import queue
import socket
import threading


class PacketQueue:
    def __init__(self, receiver, maximum_bytes=16*1024*1024, maximum_packets=16384):
        if maximum_bytes <= 0 or maximum_packets <= 0:
            raise ValueError('Packet queue bounds must be positive')
        self.receiver = receiver
        self.maximum_bytes = maximum_bytes
        self.packets = queue.Queue(maxsize=maximum_packets)
        self.lock = threading.Lock()
        self.stopping = threading.Event()
        self.error = None
        self.queued_bytes = self.high_watermark = self.received = self.delivered = 0
        self.worker = threading.Thread(target=self._drain, daemon=True)
        self.worker.start()

    def _drain(self):
        while not self.stopping.is_set():
            try:
                item = self.receiver.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError as exc:
                if not self.stopping.is_set():
                    self.error = exc
                return
            with self.lock:
                if self.queued_bytes+len(item[0]) > self.maximum_bytes or self.packets.full():
                    self.error = RuntimeError('UDP packet queue overflow; no packets silently discarded')
                    return
                self.queued_bytes += len(item[0])
                self.high_watermark = max(self.high_watermark, self.queued_bytes)
                self.received += 1
                self.packets.put_nowait(item)

    def recvfrom(self, size):
        if self.error is not None:
            raise self.error
        try:
            item = self.packets.get(timeout=.1)
        except queue.Empty:
            if self.error is not None:
                raise self.error
            raise socket.timeout()
        with self.lock:
            self.queued_bytes -= len(item[0])
            self.delivered += 1
        if self.error is not None:
            raise self.error
        return item[0][:size], item[1]

    def close(self):
        self.stopping.set()
        self.receiver.close()
        self.worker.join(timeout=1)
        if self.worker.is_alive():
            raise RuntimeError('UDP drain worker did not stop')

    def statistics(self):
        return dict(received_datagrams=self.received, delivered_datagrams=self.delivered,
                    high_watermark_bytes=self.high_watermark, maximum_bytes=self.maximum_bytes,
                    queued_bytes=self.queued_bytes, error=str(self.error) if self.error else None)
