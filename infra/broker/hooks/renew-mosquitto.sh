#!/usr/bin/env bash
# Deploy-hook certbot: копирует новые сертификаты и перезапускает брокер.
# install.sh ставит копию в /etc/letsencrypt/renewal-hooks/deploy/ с подставленными путями.
set -euo pipefail

BROKER_DIR="${BROKER_DIR:-__BROKER_DIR__}"
DOMAIN="${DOMAIN:-__DOMAIN__}"

if [[ "${BROKER_DIR}" == __BROKER_DIR__ ]]; then
  # Запуск из репозитория: каталог на уровень выше hooks/.
  BROKER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
fi

src="${RENEWED_LINEAGE:-/etc/letsencrypt/live/${DOMAIN}}"
if [[ ! -f "${src}/fullchain.pem" || ! -f "${src}/privkey.pem" ]]; then
  echo "renew-mosquitto: нет сертификатов в ${src}" >&2
  exit 1
fi

mkdir -p "${BROKER_DIR}/certs"
install -m 640 -o 1883 -g 1883 "${src}/fullchain.pem" "${BROKER_DIR}/certs/fullchain.pem"
install -m 640 -o 1883 -g 1883 "${src}/privkey.pem" "${BROKER_DIR}/certs/privkey.pem"

cd "${BROKER_DIR}"
if docker compose version >/dev/null 2>&1; then
  docker compose restart mosquitto
elif command -v docker-compose >/dev/null 2>&1; then
  docker-compose restart mosquitto
else
  echo "renew-mosquitto: не найден docker compose" >&2
  exit 1
fi
