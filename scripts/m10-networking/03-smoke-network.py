#!/usr/bin/env python3
"""QMP launch smoke test: confirms the user NAT backend can initialise.
Does not prove connectivity within the guest OS or test the firewall/DNS.
"""
import json, os, re, socket, subprocess, sys, tempfile, time

if len(sys.argv) != 2:
    sys.exit('Usage: 03-smoke-network.py /opt/sailqemu/qemu-10.2.4-net/bin/qemu-system-x86_64')
qemu = os.path.abspath(sys.argv[1])
help_result = subprocess.run([qemu, '-netdev', 'help'], capture_output=True, text=True)
if not re.search(r'(?m)^\s*user\s*$', help_result.stdout + help_result.stderr):
    sys.exit('FAIL: user backend not advertised by QEMU')
print('PASS: QEMU advertises user NAT backend')
with tempfile.TemporaryDirectory(prefix='sailqemu-m10-') as temp:
    sock_path = os.path.join(temp, 'qmp.sock')
    args = [qemu, '-display', 'none', '-machine', 'pc', '-m', '64', '-S',
            '-netdev', 'user,id=slirp0', '-device', 'rtl8139,netdev=slirp0',
            '-qmp', 'unix:' + sock_path + ',server=on,wait=off']
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        for attempt in range(50):
            if proc.poll() is not None:
                sys.exit('FAIL: QEMU exited early: ' + proc.stderr.read().decode(errors='replace'))
            if os.path.exists(sock_path):
                break
            time.sleep(0.1)
        else:
            sys.exit('FAIL: QMP socket unavailable')
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as conn:
            conn.settimeout(4)
            conn.connect(sock_path)
            reader = conn.makefile('rb')
            def read_reply():
                for _ in range(50):
                    msg = json.loads(reader.readline())
                    if 'return' in msg or 'error' in msg:
                        return msg
                raise RuntimeError('No QMP reply')
            greeting = json.loads(reader.readline())
            assert 'QMP' in greeting, greeting
            conn.sendall(b'{"execute":"qmp_capabilities"}\n')
            assert 'return' in read_reply()
            conn.sendall(b'{"execute":"query-status"}\n')
            result = read_reply()
            assert result['return']['running'] is False, result
            print('PASS: D-Bus-independent QMP startup with NIC + SLIRP; VM is stopped:', result['return']['status'])
            conn.sendall(b'{"execute":"quit"}\n')
            # QEMU can close the QMP socket immediately on quit.
    finally:
        if proc.poll() is None:
            proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
print('PASS: QEMU NAT backend initialises without replacing the old QEMU')
