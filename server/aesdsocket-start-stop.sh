#!/bin/sh

case "$1" in
    start)
        echo "Starting aesdsocket"
        start-stop-daemon -S -n aesdsocket -a /home/parallels/assignment-1-cristicsordas/server/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -N -n aesdsocket
        ;;
    *)
    echo "Usage: $0 {start|stop}"
    exit 1
    ;;
esac

exit 0