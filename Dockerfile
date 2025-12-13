FROM debian:stable-slim

WORKDIR /app

COPY server .
COPY www ./www

RUN chmod +x server

CMD ["./server"]