#!/bin/sh
set -eu

SHARED_DIR="${SHARED_DIR:-/data}"
LOCK_FILE="$SHARED_DIR/.lock"

mkdir -p -- "$SHARED_DIR"

# Режим наблюдателя: docker run --rm -v shared_files:/data concurrent-worker watch [число_проверок]
if [ "${1:-}" = "watch" ]; then
    limit="${2:-5}"
    i=1
    while [ "$i" -le "$limit" ]; do
        echo "--- $i ---"
        ls -la -- "$SHARED_DIR"
        for f in "$SHARED_DIR"/[0-9]*; do
            if [ -f "$f" ]; then
                printf '%s -> %s\n' "${f##*/}" "$(cat "$f" 2>/dev/null || echo '<удалён>')"
            fi
        done
        if [ "$i" -lt "$limit" ]; then
            sleep 1
        fi
        i=$((i + 1))
    done
    exit 0
fi

# Обычный режим воркера
if [ -r /proc/sys/kernel/random/uuid ]; then
    IFS= read -r CONTAINER_ID < /proc/sys/kernel/random/uuid
else
    CONTAINER_ID=$(dd if=/dev/urandom bs=8 count=1 2>/dev/null | od -An -tx1 | tr -d ' \n')
fi

running=1
trap 'running=0' INT TERM

seq_num=0

while [ "$running" -eq 1 ]; do
    seq_num=$((seq_num + 1))
    filepath=""

    {
        flock -x 9

        i=1
        while :; do
            if [ "$i" -lt 10 ]; then
                name="00$i"
            elif [ "$i" -lt 100 ]; then
                name="0$i"
            else
                name="$i"
            fi

            candidate="$SHARED_DIR/$name"
            if [ ! -e "$candidate" ]; then
                printf '%s %d\n' "$CONTAINER_ID" "$seq_num" > "$candidate"
                filepath="$candidate"
                break
            fi
            i=$((i + 1))
        done
 
        flock -u 9
    } 9>>"$LOCK_FILE"

    sleep 1 || true

    if [ -n "$filepath" ]; then
        rm -f -- "$filepath"
    fi

    if [ "$running" -eq 1 ]; then
        sleep 1 || true
    fi
done