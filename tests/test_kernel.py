#!/usr/bin/env python3
"""Exercise the actual kernel handlers with only hardware I/O substituted."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'kernel.c').read_text()
def replace_function(text, signature, replacement):
    start = text.index(signature + ' {')
    body = text.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + replacement + text[end:]
source = source.replace('volatile uint8_t *video = (uint8_t *)VIDEO_MEMORY;',
                        'uint8_t screen[4000]; volatile uint8_t *video = screen;')
source = replace_function(source, 'uint16_t get_key()', '''uint16_t get_key() {
    assert(keys[key_pos]);
    unsigned char c = keys[key_pos++];
    return c == '\\n' ? 0x1c : c;
}''')
source = replace_function(source, 'void delay_second()', 'void delay_second() { elapsed_seconds++; }')
source = replace_function(source, 'static inline uint8_t inb(uint16_t port)',
                          'static inline uint8_t inb(uint16_t port) { (void)port; return 0; }')
source = replace_function(source, 'static inline void outb(uint16_t port, uint8_t v)',
                          'static inline void outb(uint16_t port, uint8_t v) { (void)port; (void)v; }')
source = source.replace('void print_string(const char *s, uint8_t attr) {', '''void print_string(const char *s, uint8_t attr) {
    strcat(output, s);''')
source = source.replace('void clear_screen() {', 'void clear_screen() { clears++;')
prefix = '''#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef NULL
#define strlen kernel_strlen
#define memset kernel_memset
#define main kernel_main
const char *keys = "";
int key_pos, elapsed_seconds, clears;
char output[65536];
'''
tests = r'''
#undef main
void run(const char *command, const char *input) {
    output[0] = 0; keys = input; key_pos = 0;
    parse_and_execute(command);
    assert(keys[key_pos] == 0);
}
int main(void) {
    fs_init();
    run("allowcommands", "");
    assert(strstr(output, "Requires BetterTrustedInstaller permissions."));
    run("users switch BTI", "FFSSSZSS\n"); assert(current_user == USER_BTI);
    run("allowcommands", ""); assert(strstr(output, "Error: Cannot access system! Need privileges of SYSTEM"));
    run("echo working", ""); assert(strstr(output, "working"));
    int before = clears; run("clear", ""); assert(clears == before + 1);
    run("password", ""); assert(strstr(output, "Permission denied"));
    run("diag -x", ""); assert(current_user == USER_BTI);
    before = clears;
    run("users switch SYSTEM", "bad\nbad\nbad\n");
    assert(current_user == USER_BTI && clears == before + 1);
    before = clears;
    run("users switch SYSTEM", "difficultsafeeeedoco\n\n\n\n\n\n\n\n\n\n\n");
    assert(current_user == USER_TEMP_SYSTEM && clears == before + 11);
    int warnings = 0;
    for (char *p = output; (p = strstr(p, "WARNING:")); p += 8) warnings++;
    assert(warnings == 10);
    run("LS", ""); assert(strstr(output, "boot.img"));
    run("CD /", ""); assert(fs_cwd == 0);
    for (int account = USER_TEMP_SYSTEM; account <= USER_TEMP_JB_SYSTEM; account++) {
        current_user = account;
        run("help", "");
        assert(strstr(output, "help") && strstr(output, "whoami") && strstr(output, "users"));
        assert(strstr(output, "ls [path]") && strstr(output, "cd [path]"));
        assert(!strstr(output, "ceasefire") && !strstr(output, "Ceasefire") && !strstr(output, "sds"));
        assert(!strstr(output, "mkdir") && !strstr(output, "echo TEXT"));
        run("whoami", ""); assert(strstr(output, user_names[account]));
        run("users", ""); assert(strstr(output, "BetterTrustedInstaller"));
        run("allowcommands", ""); assert(strstr(output, "Requires BetterTrustedInstaller permissions."));
        const char *denied[] = {"echo no", "clear", "pwd", "mkdir blocked", "touch blocked", "rm boot.img", "write boot.img no", "cat boot.img", "mv boot.img x", "rmdir /", "password"};
        for (unsigned i = 0; i < sizeof(denied)/sizeof(*denied); i++) {
            run(denied[i], ""); assert(strstr(output, "denied"));
            assert(current_user == account);
        }
        run("users switch User", ""); assert(current_user == USER_USER);
    }
    current_user = USER_TEMP_SYSTEM;
    run("ceasefire switchuser SYSTEM", ""); assert(current_user == USER_TEMP_SYSTEM);
    run("Ceasefire install system jailbreaker 5000", ""); assert(ceasefire_installed);
    assert(strcmp(output, "Package Installed\n") == 0);
    run("ceasefire", "1\n"); assert(current_user == USER_TEMP_SYSTEM);
    run("ceasefire", "9\n"); assert(strstr(output, "Invalid selection"));
    run("ceasefire", "3\n");
    assert(current_user == USER_TEMP_SYSTEM && elapsed_seconds == 12);
    assert(strstr(output, "ERR: NOT ALLOWED"));
    assert(!strstr(output, "sds") && !strstr(output, "Run:"));
    run("sds", ""); assert(sds_enabled);
    run("sds", ""); assert(!sds_enabled);
    run("sds", ""); assert(sds_enabled);
    run("ceasefire", "3\n");
    assert(current_user == USER_TEMP_JB_SYSTEM && elapsed_seconds == 24);
    run("touch blocked", ""); assert(fs_find_child(0, "blocked") == -1);
    run("ls", ""); assert(strstr(output, "kernel.bin"));
    run("cd /", ""); assert(fs_cwd == 0);
    run("ceasefire switchuser SYSTEM", ""); assert(current_user == USER_SYSTEM);
    run("mkdir test", ""); assert(fs_find_child(0, "test") >= 0);
    run("write test/file hello", "");
    run("cat test/file", ""); assert(strstr(output, "hello"));
    run("echo allowed", ""); assert(strcmp(output, "allowed") == 0);
    before = clears; run("clear", ""); assert(clears == before + 1);
    run("users switch User", ""); assert(current_user == USER_USER);
    run("echo denied", ""); assert(strstr(output, "Access denied"));
    before = clears; run("clear", ""); assert(clears == before);
    run("users switch SYSTEM -s", "difficultsafeeeedoco\n");
    assert(current_user == USER_TEMP_SYSTEM && !strstr(output, "WARNING:"));
    assert(!ceasefire_installed && !sds_enabled);
    run("Ceasefire install system jailbreaker 5000", "");
    run("ceasefire", "2\n"); assert(current_user == USER_BTI && elapsed_seconds == 36);
    puts("PASS: authentication, ten warnings, skip flag, permission matrix, all Ceasefire options, sds retry, temporary sessions and SYSTEM filesystem");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'test.c'
    c.write_text(prefix + source + tests)
    exe = Path(tmp) / 'test'
    subprocess.run(['gcc', '-std=gnu11', '-fno-builtin', '-Wno-unused-function', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
