# imap-mail-transfer

Copies all emails from one IMAP mailbox to another using libcurl.

## Files

| File | Description |
|------|-------------|
| `main.c` | TUI, CLI, and transfer logic |
| `src/imap.c` | libcurl IMAP operations |
| `src/init.c` | Argument parsing and authentication |
| `src/ncurses_helper.c` | ncurses menu helpers |
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

Start two local IMAP servers:

```bash
cd docker && docker compose up -d
```

Seed the source server with test emails:

```bash
bash scripts/seed_source.sh
```
