FROM alpine:3.21

COPY unix/worker.sh /usr/local/bin/worker.sh
RUN chmod +x /usr/local/bin/worker.sh

ENV SHARED_DIR=/data
VOLUME ["/data"]

ENTRYPOINT ["/usr/local/bin/worker.sh"]