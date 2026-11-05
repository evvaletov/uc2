FROM python:3.12.11-slim-bookworm
RUN apt-get update && apt-get install -y --no-install-recommends wine64 \
    && rm -rf /var/lib/apt/lists/*
ENV WINEDEBUG=-all WINEPREFIX=/tmp/uc2-wine LC_ALL=C.UTF-8
ENTRYPOINT ["/usr/lib/wine/wine64"]
