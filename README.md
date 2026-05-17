# imap-mail-transfer

Transfers, validates, and manages emails between two IMAP mailboxes using libcurl. Supports an interactive TUI and a non-interactive CLI mode.

## Files

| File | Description |
|------|-------------|
| `main.c` | Entry point, TUI/CLI dispatch |
| `src/imap.c` | libcurl IMAP operations (fetch, append, delete) |
| `src/imap_parser.c` | IMAP ENVELOPE parser |
| `src/init.c` | Argument parsing and authentication |
| `src/ncurses_helper.c` | ncurses UI helpers (menus, lists, input) |
| `src/actions.c` | Transfer, validate, and clear mailbox actions |
| `CMakeLists.txt` | Build configuration |
| `docker/docker-compose.yml` | Two GreenMail IMAP servers for testing |
| `scripts/seed_source.sh` | Seeds the source server with test emails |

## Usage

```bash
# Interactive TUI
./build/imap_mail_transfer user@src-host:port user@dst-host:port INBOX

# CLI — run action directly
./build/imap_mail_transfer -o transfer user@src-host:port user@dst-host:port INBOX
./build/imap_mail_transfer -o validate user@src-host:port user@dst-host:port INBOX

# With passwords as flags (otherwise prompted interactively)
./build/imap_mail_transfer user@src-host:port -p src_pass user@dst-host:port -P dst_pass INBOX -o transfer

# With SSL (use -s for source, -S for destination)
./build/imap_mail_transfer -s -S user@src-host:993 user@dst-host:993 INBOX -o transfer

# Full email as username (e.g. when server requires user@domain format)
./build/imap_mail_transfer user@domain@src-host:993 user@domain@dst-host:993 INBOX
```

## Requirements

- C compiler (gcc or clang)
- CMake 3.28+
- libcurl with IMAP support
- ncurses

### Install on Debian/Ubuntu

```bash
sudo apt install cmake gcc libcurl4-openssl-dev libncurses-dev
```

## Build

```bash
cmake -B build
cmake --build build
```

## Testing with Docker

Start two local IMAP servers and a Roundcube webmail:

```bash
cd docker && docker compose up -d
```

Seed the source server with test emails:

```bash
bash scripts/seed_source.sh
```

Open Roundcube at `http://localhost:8080` — log in as `test` / `password`. To browse the destination server, change the IMAP host to `dest-imap` port `3143` in account settings.
