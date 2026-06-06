#!/bin/bash
# Seeds the source IMAP server with test emails.
set -e

HOST="${1:-localhost}"
PORT="${2:-1143}"
COUNT="${3:-3}"
USER="test"
PASS="password"
TMP=$(mktemp)

send_mail() {
    local subject="$1"
    local body="$2"
    local mid="<$(date +%s%N).$$@example.com>"
    printf "From: sender@example.com\r\nTo: test@example.com\r\nSubject: %s\r\nDate: %s\r\nMessage-ID: %s\r\n\r\n%s\r\n" \
        "$subject" "$(date -R)" "$mid" "$body" > "$TMP"
    curl -s \
        --url "imap://$HOST:$PORT/INBOX" \
        --user "$USER:$PASS" \
        --upload-file "$TMP" \
        --append
    echo "Sent: $subject"
}

trap 'rm -f "$TMP"' EXIT

for i in $(seq 1 "$COUNT"); do
    send_mail "Test email $i" "This is test email number $i."
done
