#!/usr/bin/env bash
# Установка Mosquitto (Docker) на чистый Ubuntu/Debian VPS.
# Идемпотентно: повторный запуск безопасен.
#
# Опциональные переменные окружения:
#   LAMP_USER, LAMP_GROUP, LAMP_PASSWORD  — первый пользователь лампы
#   ADMIN_USER (по умолчанию admin), ADMIN_PASSWORD, SKIP_ADMIN=1
#   DOMAIN, EMAIL                         — включить TLS 8883 (Let's Encrypt)
#   SKIP_UFW=1, SKIP_DOCKER=1
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

MOSQUITTO_UID=1883
ADMIN_USER="${ADMIN_USER:-admin}"
SKIP_ADMIN="${SKIP_ADMIN:-0}"
SKIP_UFW="${SKIP_UFW:-0}"
SKIP_DOCKER="${SKIP_DOCKER:-0}"
DOMAIN="${DOMAIN:-}"
EMAIL="${EMAIL:-}"

log() { echo "==> $*"; }
die() { echo "Ошибка: $*" >&2; exit 1; }

need_root() {
  if [[ "$(id -u)" -ne 0 ]]; then
    die "запустите от root: sudo $0"
  fi
}

compose() {
  if docker compose version >/dev/null 2>&1; then
    docker compose "$@"
  elif command -v docker-compose >/dev/null 2>&1; then
    docker-compose "$@"
  else
    die "не найден docker compose. Установите Docker и плагин compose."
  fi
}

detect_os() {
  if [[ -f /etc/os-release ]]; then
    # shellcheck source=/dev/null
    . /etc/os-release
    case "${ID:-}" in
      ubuntu|debian) return 0 ;;
    esac
    case "${ID_LIKE:-}" in
      *debian*|*ubuntu*) return 0 ;;
    esac
  fi
  die "поддерживаются Ubuntu и Debian. Обнаружено: ${ID:-unknown}"
}

install_docker() {
  if [[ "${SKIP_DOCKER}" == "1" ]]; then
    command -v docker >/dev/null || die "SKIP_DOCKER=1, но docker не найден"
    return 0
  fi
  if command -v docker >/dev/null && docker info >/dev/null 2>&1; then
    log "Docker уже установлен"
  else
    log "Ставлю Docker (официальный скрипт get.docker.com)"
    apt-get update -qq
    apt-get install -y -qq ca-certificates curl
    curl -fsSL https://get.docker.com | sh
  fi
  if command -v systemctl >/dev/null && systemctl list-unit-files docker.service >/dev/null 2>&1; then
    systemctl enable --now docker
  fi
  if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
    usermod -aG docker "${SUDO_USER}" || true
  fi
  command -v docker >/dev/null || die "docker не появился после установки"
  if ! docker compose version >/dev/null 2>&1 && ! command -v docker-compose >/dev/null; then
    die "нет docker compose. Проверьте установку Docker."
  fi
}

prepare_dirs() {
  log "Готовлю каталоги config, data, log, certs"
  mkdir -p "${SCRIPT_DIR}/config" "${SCRIPT_DIR}/data" "${SCRIPT_DIR}/log" "${SCRIPT_DIR}/certs" "${SCRIPT_DIR}/conf.d"
  if [[ ! -f "${SCRIPT_DIR}/config/passwd" ]]; then
    : > "${SCRIPT_DIR}/config/passwd"
  fi
  if [[ ! -f "${SCRIPT_DIR}/config/acl" ]]; then
    printf '%s\n' '# ACL, managed by add-user.sh' > "${SCRIPT_DIR}/config/acl"
  fi
  chown -R "${MOSQUITTO_UID}:${MOSQUITTO_UID}" \
    "${SCRIPT_DIR}/config" "${SCRIPT_DIR}/data" "${SCRIPT_DIR}/log" "${SCRIPT_DIR}/certs"
  chmod 700 "${SCRIPT_DIR}/config" "${SCRIPT_DIR}/data"
  chmod 0700 "${SCRIPT_DIR}/config/passwd"
  chmod 0700 "${SCRIPT_DIR}/config/acl"
}

configure_ufw() {
  if [[ "${SKIP_UFW}" == "1" ]]; then
    log "Пропускаю ufw (SKIP_UFW=1)"
    return 0
  fi
  if ! command -v ufw >/dev/null; then
    log "Ставлю ufw"
    apt-get update -qq
    apt-get install -y -qq ufw
  fi
  log "Открываю SSH и MQTT в ufw, остальные входящие по умолчанию закрыты"
  ufw allow OpenSSH >/dev/null || true
  ufw allow 22/tcp >/dev/null || true
  ufw allow 1883/tcp comment 'MQTT plaintext' >/dev/null || ufw allow 1883/tcp >/dev/null
  if [[ -n "${DOMAIN}" ]]; then
    ufw allow 80/tcp comment 'HTTP for certbot' >/dev/null || ufw allow 80/tcp >/dev/null
    ufw allow 8883/tcp comment 'MQTT TLS' >/dev/null || ufw allow 8883/tcp >/dev/null
  fi
  if ufw status | grep -q 'Status: inactive'; then
    # SSH уже разрешён выше — иначе можно потерять доступ.
    ufw --force enable
  fi
  ufw status | head -n 20
}

install_tls() {
  if [[ -z "${DOMAIN}" ]]; then
    return 0
  fi
  [[ -n "${EMAIL}" ]] || die "задан DOMAIN, но не задан EMAIL для Let's Encrypt"
  log "Выпускаю сертификат Let's Encrypt для ${DOMAIN} (certbot standalone, порт 80)"
  if ! command -v certbot >/dev/null; then
    apt-get update -qq
    apt-get install -y -qq certbot
  fi
  certbot certonly --standalone --non-interactive --agree-tos \
    --keep-until-expiring --email "${EMAIL}" -d "${DOMAIN}"

  local live="/etc/letsencrypt/live/${DOMAIN}"
  [[ -f "${live}/fullchain.pem" && -f "${live}/privkey.pem" ]] || die "certbot не положил сертификаты в ${live}"

  mkdir -p "${SCRIPT_DIR}/certs"
  install -m 640 -o "${MOSQUITTO_UID}" -g "${MOSQUITTO_UID}" \
    "${live}/fullchain.pem" "${SCRIPT_DIR}/certs/fullchain.pem"
  install -m 640 -o "${MOSQUITTO_UID}" -g "${MOSQUITTO_UID}" \
    "${live}/privkey.pem" "${SCRIPT_DIR}/certs/privkey.pem"

  cp "${SCRIPT_DIR}/conf.d/tls.conf.example" "${SCRIPT_DIR}/conf.d/tls.conf"
  log "Включён слушатель 8883 (conf.d/tls.conf)"

  local hook_src="${SCRIPT_DIR}/hooks/renew-mosquitto.sh"
  local hook_dst="/etc/letsencrypt/renewal-hooks/deploy/companylamp-mosquitto.sh"
  mkdir -p "$(dirname "${hook_dst}")"
  sed \
    -e "s|__BROKER_DIR__|${SCRIPT_DIR}|g" \
    -e "s|__DOMAIN__|${DOMAIN}|g" \
    "${hook_src}" > "${hook_dst}"
  chmod 755 "${hook_dst}"
  log "Поставлен hook обновления сертификата: ${hook_dst}"
}

start_broker() {
  log "Запускаю брокер"
  compose up -d
  local i=0
  while [[ "${i}" -lt 10 ]]; do
    i=$((i + 1))
    if compose exec -T mosquitto pgrep -x mosquitto >/dev/null 2>&1; then
      log "Mosquitto запущен"
      return 0
    fi
    sleep 1
  done
  compose logs --tail 50 || true
  die "брокер не поднялся, смотрите логи: docker compose logs"
}

create_users() {
  local add="${SCRIPT_DIR}/add-user.sh"
  chmod +x "${add}"

  if [[ "${SKIP_ADMIN}" != "1" ]]; then
    log "Создаю администратора ${ADMIN_USER}"
    if [[ -n "${ADMIN_PASSWORD:-}" ]]; then
      "${add}" --admin "${ADMIN_USER}" "${ADMIN_PASSWORD}"
    else
      "${add}" --admin "${ADMIN_USER}"
    fi
  fi

  if [[ -n "${LAMP_USER:-}" && -n "${LAMP_GROUP:-}" ]]; then
    log "Создаю пользователя лампы ${LAMP_USER} (группа ${LAMP_GROUP})"
    if [[ -n "${LAMP_PASSWORD:-}" ]]; then
      "${add}" "${LAMP_USER}" "${LAMP_GROUP}" "${LAMP_PASSWORD}"
    else
      "${add}" "${LAMP_USER}" "${LAMP_GROUP}"
    fi
  else
    log "Пользователь лампы не задан. Добавьте позже:"
    echo "    sudo ${add} <user> <group>"
  fi
}

need_root
detect_os
install_docker
prepare_dirs
configure_ufw
install_tls
start_broker
create_users

log "Готово. Дальше см. README.md: проверка mosquitto_sub/pub, настройка ламп."
if [[ -n "${DOMAIN}" ]]; then
  echo "    MQTT: ${DOMAIN}:1883 (открытый текст) и ${DOMAIN}:8883 (TLS)"
else
  echo "    MQTT: порт 1883 на этом сервере (открытый текст)"
fi
