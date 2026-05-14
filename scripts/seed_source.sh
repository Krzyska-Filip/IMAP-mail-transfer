#!/bin/bash
# Seeds the source IMAP server with test emails.
set -e

HOST="${1:-localhost}"
PORT="${2:-1143}"
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

send_mail "Test email 1" "This is the first test email."
send_mail "Test email 2" "This is the second test email."
send_mail "Test email 3" "This is the third test email."
