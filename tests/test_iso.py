#!/usr/bin/env python3
"""Boot the built ISO and exercise the real PS/2 keyboard and VGA screen."""
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    command = [os.environ.get('QEMU', 'qemu-system-i386'), '-accel', 'tcg',
               '-m', '32', '-display', 'none', '-nic', 'none', '-no-reboot',
               '-cdrom', str(root / 'CookieOS.iso'), '-boot', 'd',
               '-qmp', f'unix:{tmp}/qmp,server=on,wait=off']
    if os.environ.get('QEMU_DATA'):
        command += ['-L', os.environ['QEMU_DATA']]
    if os.environ.get('QEMU_BIOS'):
        command += ['-bios', os.environ['QEMU_BIOS']]
    with (tmp / 'stderr').open('w+') as errors:
        process = subprocess.Popen(command, stderr=errors)
        try:
            deadline = time.monotonic() + 10
            while not (tmp / 'qmp').exists():
                if process.poll() is not None:
                    errors.seek(0)
                    raise RuntimeError(errors.read())
                assert time.monotonic() < deadline, 'QMP startup timeout'
                time.sleep(.05)
            connection = socket.socket(socket.AF_UNIX)
            while True:
                try:
                    connection.connect(str(tmp / 'qmp'))
                    break
                except ConnectionRefusedError:
                    if process.poll() is not None:
                        errors.seek(0)
                        raise RuntimeError(errors.read())
                    assert time.monotonic() < deadline, 'QMP connection timeout'
                    time.sleep(.05)
            stream = connection.makefile('rwb', buffering=0)
            json.loads(stream.readline())
            def qmp(execute, arguments=None):
                stream.write((json.dumps({'execute': execute, 'arguments': arguments or {}}) + '\n').encode())
                while True:
                    reply = json.loads(stream.readline())
                    if 'error' in reply:
                        raise RuntimeError(reply)
                    if 'return' in reply:
                        return reply['return']
            qmp('qmp_capabilities')
            def monitor(text):
                return qmp('human-monitor-command', {'command-line': text})
            def screen():
                monitor(f'pmemsave 0xb8000 4000 "{tmp}/screen"')
                data = (tmp / 'screen').read_bytes()[::2].decode('ascii', errors='replace')
                return '\n'.join(data[i:i+80] for i in range(0, 2000, 80))
            def expect(text, timeout=5):
                deadline = time.monotonic() + timeout
                while True:
                    current = screen()
                    if text in current:
                        return
                    if time.monotonic() > deadline:
                        raise AssertionError(f'Missing {text!r}:\n{current}')
                    time.sleep(.05)
            def key(name):
                monitor('sendkey ' + name + ' 10')
                time.sleep(.025)
            def type_text(text):
                for c in text:
                    if c == '\n': name = 'ret'
                    elif c == ' ': name = 'spc'
                    elif c == '-': name = 'minus'
                    elif c == '/': name = 'slash'
                    elif c.isupper(): name = 'shift-' + c.lower()
                    else: name = c
                    key(name)
            expect('User>')
            type_text('allowcommands\n')
            expect('Requires BetterTrustedInstaller permissions.')
            type_text('users switch BTI\n'); expect('Enter Password')
            type_text('FFSSSZSS\n'); expect('BetterTrustedInstaller>')
            type_text('clear\n'); type_text('echo verified\n'); expect('verified')
            type_text('allowcommands\n'); expect('Need privileges of SYSTEM')
            type_text('users switch SYSTEM\n'); type_text('bad\nbad\nbad\n')
            expect('Access denied.')
            assert 'CookieOS 1.0' not in screen(), 'Three failed passwords did not clear screen'
            type_text('users switch SYSTEM\n'); type_text('difficultsafeeeedoco\n')
            for warning in ['this could hurt your PC!!!!!', 'SYSTEM privileges can destroy',
                            'Incorrect commands', 'Deleting system files',
                            'Unrestricted changes', 'Recovery may require',
                            'Verify your backups', 'Do not run commands',
                            'Privileged operations', 'You accept responsibility']:
                expect(warning); key('ret')
            expect('TEMP_SYSTEM>')
            type_text('help\n'); expect('ls [path]')
            assert 'Ceasefire' not in screen() and 'ceasefire' not in screen()
            type_text('whoami\n'); expect('TEMP_SYSTEM>')
            type_text('ls\n'); expect('boot.img')
            type_text('cd /\n'); type_text('touch blocked\n'); expect('Access denied.')
            type_text('Ceasefire install system jailbreaker 5000\n'); expect('Package Installed')
            type_text('ceasefire\n'); expect('3) Jailbreak SYSTEM account')
            start = time.monotonic()
            type_text('3\n'); expect('JAILBREAKING'); expect('ERR: NOT ALLOWED', timeout=18)
            elapsed = time.monotonic() - start
            assert 11 <= elapsed <= 16, f'Unexpected jailbreak duration: {elapsed:.2f}s'
            print(f'PASS: real ISO boot, keyboard, permissions, passwords, ten warnings and jailbreak timing ({elapsed:.2f}s)', flush=True)
            type_text('sds\n'); expect('sds ON.')
            type_text('ceasefire\n'); type_text('3\n')
            expect('TEMP-JB-SYSTEM>', timeout=18)
            type_text('ceasefire switchuser SYSTEM\n'); expect('SYSTEM session started.')
            type_text('echo system-console-works\n'); expect('system-console-works')
            type_text('clear\n')
            assert 'Finalizing request' not in screen()
            type_text('mkdir test\n'); type_text('cd test\n'); expect('SYSTEM:/test>')
            type_text('write file works\n'); type_text('cat file\n'); expect('works')
            type_text('users switch User\n'); expect('User>')
            type_text('users switch SYSTEM -s\n'); type_text('difficultsafeeeedoco\n')
            expect('TEMP_SYSTEM:/test>')
            print('PASS: sds retry, TEMP-JB-SYSTEM, final SYSTEM switch, filesystem and -s in the booted ISO', flush=True)
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
