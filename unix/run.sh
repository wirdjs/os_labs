#!/bin/sh
set -eu

IMAGE="concurrent-worker"
VOLUME="shared_files"

stop_workers() {
    docker ps -q --filter "name=^worker-" | xargs -r docker stop >/dev/null
}

case "${1:-}" in
    stop)
        stop_workers
        echo "Все воркеры остановлены."
        ;;
    watch)
        docker run --rm -v "$VOLUME:/data" alpine sh -c '
            for step in 1 2 3 4 5; do
                echo "=== Проверка $step ==="
                for f in /data/[0-9]*; do
                    [ -f "$f" ] && printf "%s -> %s\n" "${f##*/}" "$(cat "$f" 2>/dev/null || echo "<удалён>")"
                done
                sleep 1
            done
        '
        ;;
    ""|*[!0-9]*)
        echo "Использование: $0 <количество|watch|stop>"
        exit 1
        ;;
    *)
        count="$1"
        stop_workers
        docker volume create "$VOLUME" >/dev/null
        docker build -t "$IMAGE" .

        i=1
        while [ "$i" -le "$count" ]; do
            docker run -d --rm --name "worker-$i" -v "$VOLUME:/data" "$IMAGE" >/dev/null
            i=$((i + 1))
        done
        echo "Запущено контейнеров: $count"
        ;;
esac