# imap-mail-transfer

Copies all emails from one IMAP mailbox to another using libcurl.

## Files

| File | Description |
|------|-------------|
| `main.c` | All program logic |
| `CMakeLists.txt` | Build configuration |
| `docker/docker-compose.yml` | Two GreenMail IMAP servers for testing |
| `scripts/seed_source.sh` | Seeds the source server with test emails |

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
