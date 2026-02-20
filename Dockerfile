# Use Debian Bookworm (same base as Raspberry Pi OS) for SDL2/Wayland support
FROM debian:bookworm-slim AS build

RUN apt-get update && \
    apt-get install --yes build-essential gcc pkg-config libsdl2-dev

COPY . /app
WORKDIR /app
RUN make build

FROM scratch AS export
COPY --from=build /app/bin/flying-toasters /flying-toasters
