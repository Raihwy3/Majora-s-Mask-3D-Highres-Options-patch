#!/usr/bin/env python3
"""Execute the built loader's integer ARM instructions against mocked kernel calls.

This checks register/stack contracts, chaining and failure paths. It is not a
3DS emulator and cannot validate real memory protection, caches or rendering.
SVC numbers are extracted independently from the installed libctru binary.
"""
from io import BytesIO
from pathlib import Path
import struct
import subprocess
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]


def libctru_svcs():
    obj = subprocess.check_output([
        '/opt/devkitpro/devkitARM/bin/arm-none-eabi-ar', 'p',
        '/opt/devkitpro/libctru/lib/libctru.a', 'svc.o'])
    elf = ELFFile(BytesIO(obj))
    result = {}
    for name in ('svcGetProcessId', 'svcOpenProcess', 'svcControlProcessMemory',
                 'svcCloseHandle', 'svcExitProcess'):
        data = elf.get_section_by_name('.text.' + name).data()
        calls = [w & 0xffffff for (w,) in struct.iter_unpack('<I', data)
                 if w >> 24 == 0xef]
        assert len(calls) == 1, (name, calls)
        result[name] = calls[0]
    return result


class Cpu:
    def __init__(self, boot, symbols, svcs, restoration, fail):
        self.reg = [0x12340000 + i for i in range(16)]
        self.reg[13] = 0x10000000
        self.reg[15] = symbols['mm_bootstrap']
        self.initial = self.reg[:]
        self.memory = {}
        for i, byte in enumerate(boot):
            self.memory[symbols['mm_bootstrap'] + i] = byte
        self.write(0x61cecc, 0xe92d5fff if restoration else 0)
        self.flags = 0xa0000000
        self.symbols, self.svcs = symbols, svcs
        self.restoration, self.fail = restoration, fail
        self.calls = []
        self.initialized = False
        self.closed = False
        self.ended = None

    def read(self, address):
        return sum(self.memory.get(address + i, 0) << (8 * i) for i in range(4))

    def write(self, address, value):
        for i in range(4):
            self.memory[address + i] = value >> (8 * i) & 255

    def value(self, index, pc):
        return pc + 8 if index == 15 else self.reg[index]

    def call(self, number):
        r, svc = self.reg, self.svcs
        self.calls.append(number)
        if number == svc['svcGetProcessId']:
            assert r[1] == 0xffff8001, 'GetProcessId handle must be in r1'
            r[0], r[1] = (0xd8e007f7, 0) if self.fail == 'pid' else (0, 39)
        elif number == svc['svcOpenProcess']:
            assert r[1] == 39, 'OpenProcess PID must be in r1'
            r[0], r[1] = (0xd8e007f7, 0) if self.fail == 'open' else (0, 0x123)
        elif number == svc['svcControlProcessMemory']:
            assert r[:6] == [0x123, self.symbols['__module_start'], 0,
                             self.symbols['__module_rx_size'], 6, 5], r[:6]
            r[0] = 0xd8e007f7 if self.fail == 'protect' else 0
        elif number == svc['svcCloseHandle']:
            assert r[0] == 0x123
            self.closed = True
            r[0] = 0
        elif number == svc['svcExitProcess']:
            assert self.fail and not self.initialized
            self.ended = 'exit'
        else:
            raise AssertionError(f'Unexpected SVC {number:#x}')

    def run(self):
        for _ in range(200):
            if self.ended:
                return
            pc = self.reg[15]
            if pc == self.symbols['mm_native_init']:
                assert self.closed and not self.fail
                assert self.reg[13] % 8 == 0
                self.initialized = True
                for i in (0, 1, 2, 3, 12):
                    self.reg[i] = 0xdead0000 + i
                self.reg[15] = self.reg[14]
                continue
            if pc == self.symbols.get('mm_restoration_start',-1):
                assert self.restoration and self.initialized
                assert self.reg[:15] == self.initial[:15]
                self.ended = 'restoration'
                return
            if pc == self.symbols['mm_original_clear_bss']:
                assert not self.restoration and self.initialized
                assert self.reg[:14] == self.initial[:14]
                self.reg[15] = self.reg[14]
                continue
            if pc == self.symbols['mm_original_entry_continue']:
                assert not self.restoration and self.initialized
                assert self.reg[:14] == self.initial[:14]
                self.ended = 'stock'
                return
            w = self.read(pc)
            self.reg[15] = pc + 4
            cond = w >> 28
            z, n = bool(self.flags & (1 << 30)), bool(self.flags & (1 << 31))
            if not ({0: z, 4: n, 14: True}.get(cond, False)):
                assert cond in (0, 4, 14), hex(w)
                continue
            if w & 0x0f000000 == 0x0f000000:
                self.call(w & 0xffffff)
            elif w & 0x0e000000 == 0x0a000000:
                delta = w & 0xffffff
                if delta & 0x800000:
                    delta -= 0x1000000
                if w & (1 << 24):
                    self.reg[14] = pc + 4
                self.reg[15] = pc + 8 + delta * 4
            elif w & 0xffff0000 == 0xe92d0000:
                indices = [i for i in range(16) if w & (1 << i)]
                self.reg[13] -= len(indices) * 4
                for j, i in enumerate(indices):
                    self.write(self.reg[13] + j * 4, self.reg[i])
            elif w & 0xffff0000 == 0xe8bd0000:
                indices = [i for i in range(16) if w & (1 << i)]
                sp = self.reg[13]
                for j, i in enumerate(indices):
                    self.reg[i] = self.read(sp + j * 4)
                self.reg[13] = sp + len(indices) * 4
            elif w & 0x0fbf0fff == 0x010f0000:
                self.reg[w >> 12 & 15] = self.flags
            elif w & 0x0ffffff0 == 0x0128f000:
                self.flags = self.reg[w & 15] & 0xf0000000
            elif w & 0x0c100000 == 0x04100000:
                assert w & (1 << 24) and not w & (1 << 25) and not w & (1 << 21)
                offset = w & 4095
                if not w & (1 << 23):
                    offset = -offset
                self.reg[w >> 12 & 15] = self.read(self.value(w >> 16 & 15, pc) + offset)
            elif w & 0x0c000000 == 0:
                if w & (1 << 25):
                    v, rotation = w & 255, ((w >> 8) & 15) * 2
                    value = ((v >> rotation) | (v << (32 - rotation))) & 0xffffffff if rotation else v
                else:
                    assert w & 0xff0 == 0, hex(w)
                    value = self.value(w & 15, pc)
                opcode = w >> 21 & 15
                if opcode == 13:
                    self.reg[w >> 12 & 15] = value
                elif opcode == 10:
                    left = self.value(w >> 16 & 15, pc)
                    diff = (left - value) & 0xffffffff
                    self.flags = (diff & 0x80000000) | (0x40000000 if diff == 0 else 0)
                else:
                    raise AssertionError(hex(w))
            else:
                raise AssertionError(f'Unsupported instruction {w:#x} at {pc:#x}')
        raise AssertionError('Loader failed to terminate')


def check(boot=None):
    with (ROOT / 'build/native.elf').open('rb') as handle:
        elf = ELFFile(handle)
        symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
    boot = boot if boot is not None else (ROOT / 'build/bootstrap.bin').read_bytes()
    svcs = libctru_svcs()
    for restoration in (False,):
        for failure in (None, 'pid', 'open', 'protect'):
            cpu = Cpu(boot, symbols, svcs, restoration, failure)
            cpu.run()
            assert cpu.ended == ('exit' if failure else 'restoration' if restoration else 'stock')
            assert cpu.closed == (failure in (None, 'protect'))
    return svcs


if __name__ == '__main__':
    svcs=check()
    print('PASS: USA standalone ARM loader, real libctru SVC numbers, native continuation, register/stack/flags restoration and PID/open/protect failures.')
