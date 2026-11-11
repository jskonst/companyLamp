#!/usr/bin/env bash
# Добавляет пользователя Mosquitto и ACL на группу ламп (или админа).
# Использование:
#   ./add-user.sh <user> <group> [password]
#   ./add-user.sh --admin <user> [password]
# Если пароль не задан, генерируется случайный и печатается один раз.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

MOSQUITTO_IMAGE="${MOSQUITTO_IMAGE:-eclipse-mosquitto:2.0.22}"
AUTH_DIR="${SCRIPT_DIR}/config"
PASSWD_FILE="${AUTH_DIR}/passwd"
ACL_FILE="${AUTH_DIR}/acl"
CONTAINER_PASSWD="/mosquitto/auth/passwd"
CONTAINER_ACL="/mosquitto/auth/acl"

usage() {
  echo "Использование: $0 <user> <group> [password]" >&2
  echo "       $0 --admin <user> [password]" >&2
  echo "  user и group: латиница, цифры, точка, подчёркивание, дефис; группа до 20 символов." >&2
  exit 2
}

compose() {
  if docker compose version >/dev/null 2>&1; then
    docker compose "$@"
  elif command -v docker-compose >/dev/null 2>&1; then
    docker-compose "$@"
  else
    return 1
  fi
}

broker_running() {
  compose ps --status running --services 2>/dev/null | grep -qx 'mosquitto'
}

valid_name() {
  local name="$1"
  local max="$2"
  [[ ${#name} -ge 1 && ${#name} -le ${max} && "${name}" =~ ^[A-Za-z0-9._-]+$ ]]
}

upsert_acl() {
  local user="$1"
  local topic_line="$2"
  local tmp
  tmp="$(mktemp)"
  if [[ ! -s "${ACL_FILE}" ]]; then
    printf '%s\n' '# ACL, managed by add-user.sh' > "${ACL_FILE}"
  fi
  awk -v user="${user}" -v topic="${topic_line}" '
    BEGIN { inblock = 0 }
    /^user / {
      if ($2 == user) { inblock = 1; next }
      inblock = 0
    }
    inblock { next }
    { print }
    END {
      print "user " user
      print topic
    }
  ' "${ACL_FILE}" > "${tmp}"
  mv "${tmp}" "${ACL_FILE}"
}

ensure_files() {
  mkdir -p "${AUTH_DIR}" "${SCRIPT_DIR}/data" "${SCRIPT_DIR}/log" "${SCRIPT_DIR}/certs" "${SCRIPT_DIR}/conf.d"
  if [[ ! -f "${PASSWD_FILE}" ]]; then
    : > "${PASSWD_FILE}"
  fi
  if [[ ! -f "${ACL_FILE}" ]]; then
    printf '%s\n' '# ACL, managed by add-user.sh' > "${ACL_FILE}"
  fi
}

passwd_is_empty() {
  if broker_running; then
    compose exec -u root -T mosquitto sh -c "test ! -s '${CONTAINER_PASSWD}'"
  else
    [[ ! -s "${PASSWD_FILE}" ]]
  fi
}

user_in_passwd() {
  local user="$1"
  if broker_running; then
    compose exec -u root -T mosquitto grep -q "^${user}:" "${CONTAINER_PASSWD}"
  else
    [[ -s "${PASSWD_FILE}" ]] && grep -q "^${user}:" "${PASSWD_FILE}"
  fi
}

sync_passwd_from_container() {
  local tmp
  tmp="$(mktemp)"
  compose exec -u root -T mosquitto cat "${CONTAINER_PASSWD}" > "${tmp}"
  mv "${tmp}" "${PASSWD_FILE}"
  if [[ "$(id -u)" -eq 0 ]]; then
    chown 1883:1883 "${PASSWD_FILE}" || true
  fi
  chmod 0700 "${PASSWD_FILE}" || true
}

run_passwd() {
  local user="$1"
  local password="$2"
  local create_flag=()
  if passwd_is_empty; then
    create_flag=(-c)
  fi
  if broker_running; then
    compose exec -u root -T mosquitto mosquitto_passwd "${create_flag[@]}" -b "${CONTAINER_PASSWD}" "${user}" "${password}"
    # В образе процесс идёт от uid 1883, ему нужно читать файл. 0700 убирает предупреждение о «world readable».
    compose exec -u root -T mosquitto chown 1883:1883 "${CONTAINER_PASSWD}"
    compose exec -u root -T mosquitto chmod 0700 "${CONTAINER_PASSWD}"
    sync_passwd_from_container
  else
    docker run --rm \
      -u root \
      -v "${AUTH_DIR}:/mosquitto/auth" \
      --entrypoint mosquitto_passwd \
      "${MOSQUITTO_IMAGE}" \
      "${create_flag[@]}" -b "${CONTAINER_PASSWD}" "${user}" "${password}"
    if [[ "$(id -u)" -eq 0 ]]; then
      chown 1883:1883 "${PASSWD_FILE}" "${ACL_FILE}"
    fi
    chmod 0700 "${PASSWD_FILE}" || true
  fi
}

reload_broker() {
  if broker_running; then
    # SIGHUP перечитывает пароли и ACL, не рвая соединения.
    compose exec -u root -T mosquitto kill -HUP 1
  fi
}

admin=0
if [[ "${1:-}" == "--admin" ]]; then
  admin=1
  shift
fi

if [[ ${admin} -eq 1 ]]; then
  [[ $# -ge 1 && $# -le 2 ]] || usage
  user="$1"
  password="${2:-}"
  group=""
else
  [[ $# -ge 2 && $# -le 3 ]] || usage
  user="$1"
  group="$2"
  password="${3:-}"
fi

if ! valid_name "${user}" 64; then
  echo "Некорректное имя пользователя: ${user}" >&2
  exit 2
fi
if [[ ${admin} -eq 0 ]]; then
  if ! valid_name "${group}" 20 || [[ "${group}" =~ \. ]]; then
    # Протокол: латиница, цифры, дефис, до 20 символов. Подчёркивание разрешаем.
    echo "Некорректное имя группы: ${group}" >&2
    exit 2
  fi
fi

generated=0
if [[ -z "${password}" ]]; then
  if user_in_passwd "${user}"; then
    password=""
  else
    if command -v openssl >/dev/null 2>&1; then
      password="$(openssl rand -base64 24 | tr -d '\n/+=')"
    else
      password="$(head -c 24 /dev/urandom | base64 | tr -d '\n/+=')"
    fi
    password="${password:0:20}"
    generated=1
  fi
fi

ensure_files

user_exists=0
if user_in_passwd "${user}"; then
  user_exists=1
fi

if [[ -n "${password}" ]]; then
  run_passwd "${user}" "${password}"
elif [[ ${user_exists} -eq 0 ]]; then
  echo "Внутренняя ошибка: нет пароля для нового пользователя" >&2
  exit 1
fi

if [[ ${admin} -eq 1 ]]; then
  upsert_acl "${user}" "topic readwrite #"
else
  upsert_acl "${user}" "topic readwrite lamp/${group}/#"
fi

if [[ "$(id -u)" -eq 0 ]]; then
  chown 1883:1883 "${ACL_FILE}" "${PASSWD_FILE}" 2>/dev/null || true
  chmod 0700 "${ACL_FILE}" || true
fi

# Если брокер запущен, проталкиваем ACL в контейнер (на VPS это тот же bind-mount файл).
if broker_running; then
  compose exec -u root -T mosquitto sh -c "cat > '${CONTAINER_ACL}'" < "${ACL_FILE}"
  compose exec -u root -T mosquitto chown 1883:1883 "${CONTAINER_ACL}"
  compose exec -u root -T mosquitto chmod 0700 "${CONTAINER_ACL}"
fi

reload_broker

echo "Пользователь: ${user}"
if [[ ${admin} -eq 1 ]]; then
  echo "Роль: admin (readwrite #)"
else
  echo "Группа: ${group}"
  echo "ACL: readwrite lamp/${group}/#"
fi
if [[ ${generated} -eq 1 ]]; then
  echo "Пароль (сохраните, больше не будет показан): ${password}"
elif [[ -n "${password}" && ${user_exists} -eq 1 ]]; then
  echo "Пароль обновлён."
elif [[ -n "${password}" ]]; then
  echo "Пароль задан из аргумента (в журнал не печатаем)."
else
  echo "Пароль не менялся. ACL обновлён."
fi
