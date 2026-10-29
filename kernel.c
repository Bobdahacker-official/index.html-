#define NULL ((void*)0)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;

int strlen(const char *s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

void *memset(void *s, int c, int n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = c;
    return s;
}

uint16_t get_key();
void parse_and_execute(const char *cmd);

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define VIDEO_MEMORY 0xB8000

volatile uint8_t *video = (uint8_t *)VIDEO_MEMORY;
int cursor_x = 0, cursor_y = 0;
int current_user = 0;

#define USER_USER 0
#define USER_BTI 1
#define USER_SYSTEM 2
#define USER_TEMP_SYSTEM 3
#define USER_TEMP_JB_SYSTEM 4
int ceasefire_installed = 0;
int sds_enabled = 0;

const char *user_names[] = {"User", "BetterTrustedInstaller", "SYSTEM", "TEMP_SYSTEM", "TEMP-JB-SYSTEM"};
const char *user_passwords[] = {"", "FFSSSZSS", "difficultsafeeeedoco"};
char input_buffer[256];
int input_pos = 0;

void scroll_up() {
    for (int i = 0; i < (SCREEN_HEIGHT - 1) * SCREEN_WIDTH * 2; i++)
        video[i] = video[i + SCREEN_WIDTH * 2];
    for (int i = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH * 2; i < SCREEN_HEIGHT * SCREEN_WIDTH * 2; i += 2) {
        video[i] = ' ';
        video[i + 1] = 0x07;
    }
    cursor_y = SCREEN_HEIGHT - 1;
}

void print_char(char c, uint8_t attr) {
    if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
        return;
    }
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= SCREEN_HEIGHT) scroll_up();
        return;
    }
    video[cursor_y * SCREEN_WIDTH * 2 + cursor_x * 2] = c;
    video[cursor_y * SCREEN_WIDTH * 2 + cursor_x * 2 + 1] = attr;
    cursor_x++;
    if (cursor_x >= SCREEN_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= SCREEN_HEIGHT) scroll_up();
    }
}

void print_string(const char *s, uint8_t attr) {
    while (*s) print_char(*s++, attr);
}

void clear_screen() {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT * 2; i += 2) {
        video[i] = ' ';
        video[i + 1] = 0x07;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void newline() {
    print_char('\n', 0x07);
}

#define FS_MAX 40
#define NAME_MAX 16
#define DATA_MAX 96
typedef struct { char name[NAME_MAX]; int parent; int is_dir; int used; char data[DATA_MAX]; } fs_node;
fs_node fs[FS_MAX];
int fs_cwd = 0;

int strcmp_custom(const char *a, const char *b);
int str_starts_with(const char *s, const char *prefix);

void str_copy(char *d, const char *s, int max) {
    int i = 0;
    while (s[i] && i < max - 1) { d[i] = s[i]; i++; }
    d[i] = 0;
}

int fs_find_child(int dir, const char *name) {
    for (int i = 1; i < FS_MAX; i++)
        if (fs[i].used && fs[i].parent == dir && strcmp_custom(fs[i].name, name) == 0) return i;
    return -1;
}

int fs_alloc(int parent, const char *name, int is_dir) {
    for (int i = 1; i < FS_MAX; i++) {
        if (!fs[i].used) {
            fs[i].used = 1;
            fs[i].parent = parent;
            fs[i].is_dir = is_dir;
            fs[i].data[0] = 0;
            str_copy(fs[i].name, name, NAME_MAX);
            return i;
        }
    }
    return -1;
}

void fs_init() {
    fs[0].used = 1; fs[0].is_dir = 1; fs[0].parent = 0; fs[0].name[0] = 0;
    int a = fs_alloc(0, "boot.img", 0);   str_copy(fs[a].data, "CookieOS boot image", DATA_MAX);
    int b = fs_alloc(0, "kernel.bin", 0); str_copy(fs[b].data, "CookieOS kernel binary", DATA_MAX);
    int c = fs_alloc(0, "system.cfg", 0); str_copy(fs[c].data, "version=1.0", DATA_MAX);
}

int fs_resolve(const char *path) {
    int cur = (path[0] == '/') ? 0 : fs_cwd;
    char part[NAME_MAX + 1];
    while (*path) {
        while (*path == '/') path++;
        if (!*path) break;
        int n = 0;
        while (*path && *path != '/') {
            if (n < NAME_MAX) part[n] = *path;
            n++; path++;
        }
        if (n > NAME_MAX - 1) return -1;
        part[n] = 0;
        if (strcmp_custom(part, ".") == 0) continue;
        if (strcmp_custom(part, "..") == 0) { cur = fs[cur].parent; continue; }
        if (!fs[cur].is_dir) return -1;
        int c = fs_find_child(cur, part);
        if (c < 0) return -1;
        cur = c;
    }
    return cur;
}

/* returns parent dir index (or -1) and the new entry's name in `name` (>= NAME_MAX bytes) */
int fs_split(const char *path, char *name) {
    char buf[64];
    int len = 0;
    while (path[len] && len < 63) { buf[len] = path[len]; len++; }
    buf[len] = 0;
    while (len > 1 && buf[len - 1] == '/') buf[--len] = 0;
    int slash = -1;
    for (int i = 0; i < len; i++) if (buf[i] == '/') slash = i;
    const char *base = buf + slash + 1;
    int parent;
    if (slash < 0) parent = fs_cwd;
    else if (slash == 0) parent = 0;
    else { buf[slash] = 0; parent = fs_resolve(buf); }
    if (parent < 0 || !fs[parent].is_dir) return -1;
    if (base[0] == 0 || strlen(base) > NAME_MAX - 1) return -1;
    if (strcmp_custom(base, ".") == 0 || strcmp_custom(base, "..") == 0) return -1;
    str_copy(name, base, NAME_MAX);
    return parent;
}

void fs_path(int idx, char *out) {
    int stack[FS_MAX];
    int depth = 0, o = 0;
    while (idx != 0 && depth < FS_MAX) { stack[depth++] = idx; idx = fs[idx].parent; }
    if (depth == 0) out[o++] = '/';
    while (depth > 0) {
        out[o++] = '/';
        const char *n = fs[stack[--depth]].name;
        while (*n) out[o++] = *n++;
    }
    out[o] = 0;
}

int fs_is_within(int node, int anc) {
    while (1) {
        if (node == anc) return 1;
        if (node == 0) return 0;
        node = fs[node].parent;
    }
}

void fs_free(int idx) {
    for (int i = 1; i < FS_MAX; i++)
        if (fs[i].used && fs[i].parent == idx && i != idx) fs_free(i);
    fs[idx].used = 0;
}

int require_system() {
    if (current_user == USER_SYSTEM) return 1;
    if (current_user == USER_BTI)
        print_string("Error: Cannot access system! Need privileges of SYSTEM\n", 0x07);
    else
        print_string("Access denied. Only BetterTrustedInstaller Can access this. Try: users\n", 0x07);
    return 0;
}

int require_bti_only() {
    if (current_user == USER_BTI) return 1;
    print_string("Access denied.\n", 0x07);
    return 0;
}

void print_prompt() {
    print_string(user_names[current_user], 0x07);
    if (current_user >= USER_SYSTEM && fs_cwd != 0) {
        char p[FS_MAX * NAME_MAX + 2];
        fs_path(fs_cwd, p);
        print_string(":", 0x07);
        print_string(p, 0x07);
    }
    print_string("> ", 0x07);
}

int strcmp_custom(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

int str_starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++; prefix++;
    }
    return 1;
}

void handle_help() {
    print_string("Available commands:\n", 0x07);
    newline();
    print_string("  help               - Show this message\n", 0x07);
    print_string("  whoami             - Display current user\n", 0x07);
    print_string("  users              - Show user list\n", 0x07);
    print_string("  users switch NAME [-s] - Switch user; -s skips SYSTEM warnings\n", 0x07);
    print_string("  echo TEXT          - Print text (BTI only)\n", 0x07);
    print_string("  clear              - Clear screen (BTI only)\n", 0x07);
    print_string("  allowcommands      - Check account permissions\n", 0x07);
    print_string("SYSTEM filesystem commands (ls/cd also in temporary sessions):\n", 0x07);
    print_string("  ls [path]          - List files\n", 0x07);
    print_string("  cd [path]          - Change directory\n", 0x07);
    print_string("  pwd                - Print current directory\n", 0x07);
    print_string("  mkdir NAME         - Make a directory\n", 0x07);
    print_string("  rmdir NAME         - Remove an empty directory\n", 0x07);
    print_string("  rm [-r] NAME       - Remove a file (or directory with -r)\n", 0x07);
    print_string("  touch NAME         - Create an empty file\n", 0x07);
    print_string("  cat NAME           - Show a file\n", 0x07);
    print_string("  write NAME TEXT    - Write text into a file\n", 0x07);
    print_string("  mv SRC DST         - Move or rename\n", 0x07);
    newline();
}

void handle_whoami() {
    print_string(user_names[current_user], 0x07);
    newline();
}

void system_warnings() {
    const char *warnings[] = {
        "WARNING: this could hurt your PC!!!!!",
        "WARNING: SYSTEM privileges can destroy important data.",
        "WARNING: Incorrect commands can make an operating system unbootable.",
        "WARNING: Deleting system files can prevent startup.",
        "WARNING: Unrestricted changes can compromise account security.",
        "WARNING: Recovery may require reinstalling your operating system.",
        "WARNING: Verify your backups before making privileged changes.",
        "WARNING: Do not run commands you do not understand.",
        "WARNING: Privileged operations may cause irreversible data loss.",
        "WARNING: You accept responsibility for every SYSTEM command."
    };
    for (int i = 0; i < 10; i++) {
        clear_screen();
        print_string(warnings[i], 0x4f);
        print_string("\nPress Enter to acknowledge and continue.\n", 0x07);
        while (get_key() != 0x1c) ;
    }
    clear_screen();
}

void handle_users() {
    print_string("User\n", 0x07);
    print_string("BetterTrustedInstaller\n", 0x07);
    print_string("SYSTEM\n", 0x07);
    newline();
    print_string("To switch users, do users switch (name), for example: users switch SYSTEM\n", 0x07);
}

void handle_users_switch(const char *target) {
    char name[64];
    str_copy(name, target, sizeof(name));
    int skip = 0;
    int len = strlen(name);
    if (len >= 3 && strcmp_custom(name + len - 3, " -s") == 0) {
        name[len - 3] = 0;
        skip = 1;
    }
    target = name;
    int target_user = -1;
    if (strcmp_custom(target, "User") == 0) target_user = USER_USER;
    else if (strcmp_custom(target, "BetterTrustedInstaller") == 0 || strcmp_custom(target, "BTI") == 0) target_user = USER_BTI;
    else if (strcmp_custom(target, "SYSTEM") == 0) target_user = USER_SYSTEM;

    if (target_user == -1) {
        print_string("User not found\n", 0x07);
        return;
    }

    if (target_user == current_user) {
        print_string("Error: Cannot switch to current user\n", 0x07);
        return;
    }

    if (target_user == USER_BTI || target_user == USER_SYSTEM) {
        print_string("Enter Password for ", 0x07);
        print_string(user_names[target_user], 0x07);
        print_string(": ", 0x07);

        char pass[256];
        int pass_pos = 0;
        pass[0] = 0;

        int attempts = 0;
        while (attempts < 3) {
            attempts++;
            pass_pos = 0;
            pass[0] = 0;

            while (1) {
                uint16_t key = get_key();
                if (key == 0x1c) {
                    print_char('\n', 0x07);
                    break;
                }
                if (key == 0x0e) {
                    if (pass_pos > 0) {
                        pass_pos--;
                        print_char('\b', 0x07); print_char(' ', 0x07); print_char('\b', 0x07);
                    }
                    pass[pass_pos] = 0;
                    continue;
                }
                if (key >= 32 && key < 127 && pass_pos < 250) {
                    pass[pass_pos++] = key;
                    pass[pass_pos] = 0;
                    print_char('*', 0x07);
                }
            }

            if (strcmp_custom(pass, user_passwords[target_user]) == 0) {
                if (target_user == USER_SYSTEM) {
                    if (!skip) system_warnings();
                    current_user = USER_TEMP_SYSTEM;
                    ceasefire_installed = 0;
                    sds_enabled = 0;
                    print_string("TEMP_SYSTEM session started. Only ls/cd and the Ceasefire setup flow are available.\n", 0x07);
                    print_string("Run: Ceasefire install system jailbreaker 5000\n", 0x07);
                } else {
                    current_user = target_user;
                    print_string("Login successful\n", 0x07);
                }
                return;
            } else {
                print_string("Password incorrect. Try again: ", 0x07);
            }
        }
        clear_screen();
        print_string("Access denied.\n", 0x07);
        return;
    }

    current_user = target_user;
}

void handle_allowcommands() {
    if (current_user == USER_USER) {
        print_string("Requires BetterTrustedInstaller permissions.\n", 0x07);
    } else if (current_user == USER_BTI) {
        print_string("Error: Cannot access system! Need privileges of SYSTEM\n", 0x07);
    } else {
        print_string("SYSTEM commands are enabled.\n", 0x07);
    }
}

static inline uint8_t inb(uint16_t port);
static inline void outb(uint16_t port, uint8_t v);

/* PIT channel 2 supplies real elapsed time even while interrupts are disabled. */
void delay_second() {
    uint8_t gate = inb(0x61);
    for (int i = 0; i < 19; i++) {
        outb(0x61, gate & ~3);
        outb(0x43, 0xb0);
        outb(0x42, 0xff);
        outb(0x42, 0xff);
        outb(0x61, (gate & ~2) | 1);
        while (!(inb(0x61) & 0x20)) ;
    }
    outb(0x61, gate);
}

void jailbreak_screen() {
    const char *steps[] = {
        "Inspecting session...", "Locating privilege boundary...",
        "Bypassing measures...", "Checking account policy...",
        "Inspecting system protections...", "Preparing temporary credentials...",
        "Checking access token...", "Negotiating SYSTEM access...",
        "Validating session state...", "Applying temporary session...",
        "Checking security override...", "Finalizing request..."
    };
    clear_screen();
    print_string("JAILBREAKING\n", 0x0f);
    for (int i = 0; i < 12; i++) {
        print_string(steps[i], 0x07);
        newline();
        delay_second();
    }
}

void handle_ceasefire(const char *arg) {
    if (strcmp_custom(arg, "install system jailbreaker 5000") == 0) {
        if (current_user != USER_TEMP_SYSTEM) {
            print_string("Installation requires TEMP_SYSTEM.\n", 0x07);
            return;
        }
        ceasefire_installed = 1;
        print_string("System jailbreaker 5000 installed. Run ceasefire.\n", 0x07);
        return;
    }
    if (!ceasefire_installed) {
        print_string("Ceasefire is not installed.\n", 0x07);
        return;
    }
    if (strcmp_custom(arg, "switchuser SYSTEM") == 0) {
        if (current_user != USER_TEMP_JB_SYSTEM) {
            print_string("Requires TEMP-JB-SYSTEM.\n", 0x07);
            return;
        }
        current_user = USER_SYSTEM;
        print_string("SYSTEM session started.\n", 0x07);
        return;
    }
    if (arg[0]) {
        print_string("Unknown Ceasefire command.\n", 0x07);
        return;
    }
    print_string("CEASEFIRE\n1) Exit\n2) Jailbreak BTI account\n3) Jailbreak SYSTEM account\nSelect: ", 0x07);
    uint16_t selection = get_key();
    print_char((char)selection, 0x07);
    while (get_key() != 0x1c) ;
    newline();
    if (selection == '1') return;
    if (selection != '2' && selection != '3') {
        print_string("Invalid selection.\n", 0x07);
        return;
    }
    jailbreak_screen();
    if (selection == '2') {
        current_user = USER_BTI;
        print_string("BetterTrustedInstaller session started.\n", 0x07);
    } else if (!sds_enabled) {
        print_string("ERR: NOT ALLOWED, DO \"sds\" to toggle on or off\n", 0x07);
    } else {
        current_user = USER_TEMP_JB_SYSTEM;
        print_string("TEMP-JB-SYSTEM session started.\nRun: ceasefire switchuser SYSTEM\n", 0x07);
    }
}

void handle_ls(const char *arg) {
    if (current_user < USER_SYSTEM && !require_system()) return;
    int t = fs_cwd;
    if (arg && arg[0]) {
        t = fs_resolve(arg);
        if (t < 0) { print_string("ls: no such file or directory\n", 0x07); return; }
    }
    if (!fs[t].is_dir) { print_string(fs[t].name, 0x07); newline(); return; }
    for (int i = 1; i < FS_MAX; i++) {
        if (fs[i].used && fs[i].parent == t) {
            print_string(fs[i].name, 0x07);
            if (fs[i].is_dir) print_string("/", 0x07);
            newline();
        }
    }
}

void handle_cd(const char *arg) {
    if (current_user < USER_SYSTEM && !require_system()) return;
    int t = 0;
    if (arg && arg[0]) t = fs_resolve(arg);
    if (t < 0) { print_string("cd: no such directory\n", 0x07); return; }
    if (!fs[t].is_dir) { print_string("cd: not a directory\n", 0x07); return; }
    fs_cwd = t;
}

void handle_pwd() {
    if (!require_system()) return;
    char p[FS_MAX * NAME_MAX + 2];
    fs_path(fs_cwd, p);
    print_string(p, 0x07);
    newline();
}

void handle_mkdir(const char *arg) {
    if (!require_system()) return;
    char name[NAME_MAX];
    if (!arg[0]) { print_string("Usage: mkdir NAME\n", 0x07); return; }
    int p = fs_split(arg, name);
    if (p < 0) { print_string("mkdir: invalid path\n", 0x07); return; }
    if (fs_find_child(p, name) >= 0) { print_string("mkdir: already exists\n", 0x07); return; }
    if (fs_alloc(p, name, 1) < 0) print_string("mkdir: filesystem full\n", 0x07);
}

void handle_touch(const char *arg) {
    if (!require_system()) return;
    char name[NAME_MAX];
    if (!arg[0]) { print_string("Usage: touch NAME\n", 0x07); return; }
    int p = fs_split(arg, name);
    if (p < 0) { print_string("touch: invalid path\n", 0x07); return; }
    if (fs_find_child(p, name) >= 0) return;
    if (fs_alloc(p, name, 0) < 0) print_string("touch: filesystem full\n", 0x07);
}

void handle_rm(const char *arg) {
    if (!require_system()) return;
    int recursive = 0;
    if (str_starts_with(arg, "-r ")) {
        recursive = 1;
        arg += 3;
        while (*arg == ' ') arg++;
    }
    if (!arg[0]) { print_string("Usage: rm [-r] NAME\n", 0x07); return; }
    int t = fs_resolve(arg);
    if (t < 0) { print_string("rm: no such file or directory\n", 0x07); return; }
    if (t == 0) { print_string("rm: cannot remove the root directory\n", 0x07); return; }
    if (fs[t].is_dir && !recursive) { print_string("rm: is a directory (use rm -r)\n", 0x07); return; }
    if (fs_is_within(fs_cwd, t)) fs_cwd = fs[t].parent;
    fs_free(t);
}

void handle_rmdir(const char *arg) {
    if (!require_system()) return;
    if (!arg[0]) { print_string("Usage: rmdir NAME\n", 0x07); return; }
    int t = fs_resolve(arg);
    if (t < 0) { print_string("rmdir: no such directory\n", 0x07); return; }
    if (t == 0) { print_string("rmdir: cannot remove the root directory\n", 0x07); return; }
    if (!fs[t].is_dir) { print_string("rmdir: not a directory\n", 0x07); return; }
    for (int i = 1; i < FS_MAX; i++)
        if (fs[i].used && fs[i].parent == t) { print_string("rmdir: directory not empty\n", 0x07); return; }
    if (fs_is_within(fs_cwd, t)) fs_cwd = fs[t].parent;
    fs[t].used = 0;
}

void handle_cat(const char *arg) {
    if (!require_system()) return;
    if (!arg[0]) { print_string("Usage: cat NAME\n", 0x07); return; }
    int t = fs_resolve(arg);
    if (t < 0) { print_string("cat: no such file\n", 0x07); return; }
    if (fs[t].is_dir) { print_string("cat: is a directory\n", 0x07); return; }
    print_string(fs[t].data, 0x07);
    newline();
}

void handle_write(const char *arg) {
    if (!require_system()) return;
    char file[64];
    int n = 0;
    while (arg[n] && arg[n] != ' ' && n < 63) { file[n] = arg[n]; n++; }
    file[n] = 0;
    if (n == 0) { print_string("Usage: write NAME TEXT\n", 0x07); return; }
    const char *text = arg + n;
    while (*text == ' ') text++;
    int t = fs_resolve(file);
    if (t < 0) {
        char name[NAME_MAX];
        int p = fs_split(file, name);
        if (p < 0) { print_string("write: invalid path\n", 0x07); return; }
        t = fs_alloc(p, name, 0);
        if (t < 0) { print_string("write: filesystem full\n", 0x07); return; }
    }
    if (fs[t].is_dir) { print_string("write: is a directory\n", 0x07); return; }
    str_copy(fs[t].data, text, DATA_MAX);
}

void handle_mv(const char *arg) {
    if (!require_system()) return;
    char src[64];
    int n = 0;
    while (arg[n] && arg[n] != ' ' && n < 63) { src[n] = arg[n]; n++; }
    src[n] = 0;
    const char *dst = arg + n;
    while (*dst == ' ') dst++;
    if (n == 0 || !dst[0]) { print_string("Usage: mv SRC DST\n", 0x07); return; }
    int s = fs_resolve(src);
    if (s < 0) { print_string("mv: no such file or directory\n", 0x07); return; }
    if (s == 0) { print_string("mv: cannot move the root directory\n", 0x07); return; }
    char name[NAME_MAX];
    int p;
    int d = fs_resolve(dst);
    if (d >= 0 && fs[d].is_dir) { p = d; str_copy(name, fs[s].name, NAME_MAX); }
    else if (d >= 0) { print_string("mv: destination already exists\n", 0x07); return; }
    else { p = fs_split(dst, name); if (p < 0) { print_string("mv: invalid destination\n", 0x07); return; } }
    if (fs_is_within(p, s)) { print_string("mv: cannot move a directory into itself\n", 0x07); return; }
    int ex = fs_find_child(p, name);
    if (ex >= 0 && ex != s) { print_string("mv: destination already exists\n", 0x07); return; }
    fs[s].parent = p;
    str_copy(fs[s].name, name, NAME_MAX);
}

void handle_echo(const char *text) {
    if (!require_bti_only()) return;
    if (text) print_string(text, 0x07);
    newline();
}

void handle_clear() {
    if (!require_bti_only()) return;
    clear_screen();
}

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    asm volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(uint16_t port, uint8_t v) {
    asm volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}

/* Move the hardware (blinking underscore) cursor to where the text goes */
void update_cursor() {
    uint16_t pos = cursor_y * SCREEN_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* Protected mode: BIOS int 0x16 is gone, so poll the PS/2 controller (scancode set 1).
   Returns 0x1c for Enter, 0x0e for Backspace, ASCII for printable keys. */
static const char sc_normal[58] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
    'q','w','e','r','t','y','u','i','o','p','[',']',0,0,'a','s',
    'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
    'b','n','m',',','.','/',0,'*',0,' '
};
static const char sc_shift[58] = {
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,
    'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,'A','S',
    'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
    'B','N','M','<','>','?',0,'*',0,' '
};
static int shift_down = 0;

uint16_t get_key() {
    update_cursor();
    while (1) {
        while (!(inb(0x64) & 1)) ;
        uint8_t sc = inb(0x60);
        if (sc == 0xE0 || sc == 0xE1) continue;
        if (sc == 0x2A || sc == 0x36) { shift_down = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift_down = 0; continue; }
        if (sc & 0x80) continue;
        if (sc == 0x1c) return 0x1c;
        if (sc == 0x0e) return 0x0e;
        if (sc < 58) {
            char c = shift_down ? sc_shift[sc] : sc_normal[sc];
            if (c) return (uint8_t)c;
        }
    }
}

const char *cmd_arg(const char *cmd, const char *name) {
    while (*name) {
        if (*cmd != *name) return NULL;
        cmd++; name++;
    }
    if (*cmd == 0) return cmd;
    if (*cmd != ' ') return NULL;
    while (*cmd == ' ') cmd++;
    return cmd;
}

void parse_and_execute(const char *cmd) {
    const char *a;
    if ((a = cmd_arg(cmd, "ceasefire")) != NULL ||
        (a = cmd_arg(cmd, "Ceasefire")) != NULL) {
        handle_ceasefire(a);
        return;
    }
    if (strcmp_custom(cmd, "sds") == 0) {
        if (current_user != USER_TEMP_SYSTEM || !ceasefire_installed) {
            print_string("sds requires TEMP_SYSTEM and Ceasefire installation.\n", 0x07);
        } else {
            sds_enabled = !sds_enabled;
            print_string(sds_enabled ? "sds ON. Run ceasefire and select 3.\n" : "sds OFF.\n", 0x07);
        }
        return;
    }
    if (current_user == USER_TEMP_SYSTEM || current_user == USER_TEMP_JB_SYSTEM) {
        if ((a = cmd_arg(cmd, "ls")) != NULL || (a = cmd_arg(cmd, "LS")) != NULL) handle_ls(a);
        else if ((a = cmd_arg(cmd, "cd")) != NULL || (a = cmd_arg(cmd, "CD")) != NULL) handle_cd(a);
        else print_string("Temporary session: only ls, cd and the Ceasefire flow are permitted.\n", 0x07);
        return;
    }
    if (strcmp_custom(cmd, "help") == 0) {
        handle_help();
    } else if (strcmp_custom(cmd, "whoami") == 0) {
        handle_whoami();
    } else if (strcmp_custom(cmd, "users") == 0) {
        handle_users();
    } else if (strcmp_custom(cmd, "users switch") == 0) {
        print_string("Usage: users switch (name), for example: users switch SYSTEM\n", 0x07);
    } else if (str_starts_with(cmd, "users switch ")) {
        handle_users_switch(cmd + 13);
    } else if (strcmp_custom(cmd, "allowcommands") == 0) {
        handle_allowcommands();
    } else if ((a = cmd_arg(cmd, "ls")) != NULL) {
        handle_ls(a);
    } else if ((a = cmd_arg(cmd, "cd")) != NULL) {
        handle_cd(a);
    } else if ((a = cmd_arg(cmd, "pwd")) != NULL) {
        handle_pwd();
    } else if ((a = cmd_arg(cmd, "mkdir")) != NULL) {
        handle_mkdir(a);
    } else if ((a = cmd_arg(cmd, "rmdir")) != NULL) {
        handle_rmdir(a);
    } else if ((a = cmd_arg(cmd, "rm")) != NULL) {
        handle_rm(a);
    } else if ((a = cmd_arg(cmd, "touch")) != NULL) {
        handle_touch(a);
    } else if ((a = cmd_arg(cmd, "cat")) != NULL) {
        handle_cat(a);
    } else if ((a = cmd_arg(cmd, "write")) != NULL) {
        handle_write(a);
    } else if ((a = cmd_arg(cmd, "mv")) != NULL) {
        handle_mv(a);
    } else if (str_starts_with(cmd, "echo ")) {
        handle_echo(cmd + 5);
    } else if (strcmp_custom(cmd, "clear") == 0) {
        handle_clear();
    } else {
        print_string("Permission denied. Try: allowcommands\n", 0x07);
    }
}

__attribute__((section(".text.main"))) void main() {
    clear_screen();
    fs_init();
    print_string("CookieOS 1.0\n", 0x0F);
    newline();

    while (1) {
        print_prompt();
        input_pos = 0;
        input_buffer[0] = 0;

        while (1) {
            uint16_t key = get_key();
            if (key == 0x1c) {
                print_char('\n', 0x07);
                break;
            }
            if (key == 0x0e && input_pos > 0) {
                input_pos--;
                input_buffer[input_pos] = 0;
                print_char('\b', 0x07);
                print_char(' ', 0x07);
                print_char('\b', 0x07);
                continue;
            }
            if (key >= 32 && key < 127 && input_pos < 250) {
                input_buffer[input_pos++] = key;
                input_buffer[input_pos] = 0;
                print_char(key, 0x07);
            }
        }

        if (input_pos > 0) {
            parse_and_execute(input_buffer);
        }
        newline();
    }
}
