# Свой MQTT-брокер для ламп

Mosquitto 2.x в Docker вместо публичного `broker.mqttdashboard.com`. Рассчитан на группу из 15–20 ламп, несколько сообщений в секунду, retained-состояния и Last Will (LWT). Протокол топиков описан в [`docs/protocol.md`](../../docs/protocol.md).

Анонимный доступ выключен. Пользователь лампы может читать и писать только `lamp/<своя-группа>/#`.

## Где взять VPS

Для такой нагрузки хватит самого скромного тарифа: 1 vCPU, 1 ГБ RAM, 10 ГБ диска, Ubuntu 22.04 или 24.04 (подойдёт и Debian 12).

Варианты (без конкретных цен — они часто меняются, смотрите на сайте провайдера):

- [Hetzner Cloud](https://www.hetzner.com/cloud) — обычно один из самых дешёвых в Европе, удобно из России через карту или реселлера;
- [Timeweb Cloud](https://timeweb.cloud/), [Selectel](https://selectel.ru/), [VDSina](https://vdsina.com/) — привычные российские VPS, оплата в рублях;
- [Oracle Cloud Always Free](https://www.oracle.com/cloud/free/) — есть всегда бесплатные машины, если устраивает регистрация и лимиты.

На машине должен быть публичный IPv4 (или домен, который на него указывает). Для ламп достаточно привязать DNS-имя; в текущей прошивке поле хоста ограничено 31 символом, так что имя лучше короткое (`mqtt.example.ru`), либо укажите IP.

## Установка

Скопируйте каталог `infra/broker/` на сервер (или клонируйте репозиторий) и выполните от root:

```bash
cd infra/broker
sudo ./install.sh
```

Скрипт идемпотентный: ставит Docker (официальный `get.docker.com`, если его ещё нет), создаёт каталоги от пользователя Mosquitto (uid 1883), открывает в `ufw` порты 1883 и SSH (22), поднимает контейнер и создаёт администратора `admin`.

Первого пользователя лампы удобно задать сразу:

```bash
sudo LAMP_USER=lamp-home LAMP_GROUP=home ./install.sh
```

Пароль, если его не передать в `LAMP_PASSWORD` / `ADMIN_PASSWORD`, будет сгенерирован и напечатан один раз — сохраните его.

Повторный запуск не ломает уже созданных пользователей и данные. Если файрвол настраиваете сами: `SKIP_UFW=1`. Если Docker уже стоит: `SKIP_DOCKER=1`.

## Добавить лампу или пользователя

```bash
cd infra/broker
sudo ./add-user.sh <логин> <группа> [пароль]
```

Пример: `sudo ./add-user.sh lamp-home home`. В ACL появится `topic readwrite lamp/home/#`. Повторный вызов с тем же логином без пароля только обновит ACL, пароль не сменит.

Администратор (чтение и запись любых топиков, удобно для отладки):

```bash
sudo ./add-user.sh --admin admin
```

Секреты (`config/passwd`, `config/acl`) в git не попадают. После добавления пользователя брокер перечитывает ACL сигналом HUP, без разрыва сессий.

На группу из 20 ламп достаточно одного логина на всю группу (все лампы группы знают один пароль) или отдельного логина на лампу — оба варианта укладываются в ACL `lamp/<группа>/#`.

## Как направить прошивку на брокер

Целевой протокол — `docs/protocol.md`:

- клиентский id: `lamp-<id>` (`id` — `ESP.getChipId()` в hex, 6 символов);
- keepalive 15 с, clean session;
- топики: `lamp/<группа>/<id>/status`, `.../state`, `.../wink`;
- `status` и `state` — retained, QoS 1;
- LWT: топик `status`, payload `offline`, retained, QoS 1;
- логин и пароль, выданные `add-user.sh`.

В портале лампы укажите хост VPS (или короткое DNS-имя), порт **1883**, логин, пароль и имя группы.

Прошивка публикует состояние в `lamp/<группа>/<id>/status`, `lamp/<группа>/<id>/state` и `lamp/<группа>/<id>/wink` и подключается только с логином и паролем. Не включайте `allow_anonymous`: тогда любой в интернете сможет писать в ваши топики.

## Проверка: mosquitto_sub / mosquitto_pub

На своём компьютере: `sudo apt install mosquitto-clients` (или `brew install mosquitto`). Ниже `HOST` — адрес брокера, `USER`/`PASS` — логин группы `home`, id лампы для примера — `a1b2c3`.

Подписка на группу, как это делает лампа:

```bash
mosquitto_sub -h HOST -p 1883 -u USER -P PASS \
  -t 'lamp/home/+/status' -t 'lamp/home/+/state' -t 'lamp/home/+/wink' -v
```

Retained-состояние (как после включения лампы):

```bash
mosquitto_pub -h HOST -p 1883 -u USER -P PASS -q 1 -r \
  -t 'lamp/home/a1b2c3/status' -m 'online'
mosquitto_pub -h HOST -p 1883 -u USER -P PASS -q 1 -r \
  -t 'lamp/home/a1b2c3/state' -m '{"m":75,"n":"Женя"}'
mosquitto_pub -h HOST -p 1883 -u USER -P PASS -q 0 \
  -t 'lamp/home/a1b2c3/wink' -m '{"h":96}'
```

Новая подписка сразу получит `status=online` и `state` — брокер хранит retained в `data/` и переживает рестарт:

```bash
cd infra/broker
sudo docker compose restart
mosquitto_sub -h HOST -p 1883 -u USER -P PASS -C 2 -W 5 -v \
  -t 'lamp/home/a1b2c3/status' -t 'lamp/home/a1b2c3/state'
```

Проверка LWT (как если лампу выдернули из розетки). В одном терминале подписка, в другом клиент с завещанием и keepalive 15 с; затем убейте второй процесс (`kill -9`), не давая ему отключиться штатно. Примерно через 22 с (1,5 keepalive) подписчик получит `offline`:

```bash
# терминал 1
mosquitto_sub -h HOST -p 1883 -u USER -P PASS -v -t 'lamp/home/a1b2c3/status'

# терминал 2
mosquitto_sub -h HOST -p 1883 -u USER -P PASS \
  --id lamp-a1b2c3 --keepalive 15 \
  --will-topic 'lamp/home/a1b2c3/status' \
  --will-payload 'offline' --will-retain --will-qos 1 \
  -t 'lamp/home/dummy'
# затем: kill -9 <pid этого процесса>
```

Чужую группу пользователь писать не должен. В MQTT 3.1.1 брокер молча отбрасывает такое сообщение, а `mosquitto_pub` при этом может завершиться с кодом 0. Надёжные проверки — MQTT 5 (будет «Not authorized») или подписка администратором:

```bash
# MQTT 5: ожидаем «Publish failed: Not authorized»
mosquitto_pub -V mqttv5 -q 1 -h HOST -p 1883 -u USER -P PASS \
  -t 'lamp/other/a1b2c3/status' -m 'online'

# Параллельно от admin: это сообщение появиться не должно
mosquitto_sub -h HOST -p 1883 -u admin -P ADMINPASS -v -t 'lamp/other/#'
```

Анонимное подключение тоже отклоняется:

```bash
mosquitto_sub -h HOST -p 1883 -t 'lamp/home/+/status'
```

## TLS (порт 8883), опционально

Основной порт — **1883**. На ESP8266 TLS заметно нагружает чип и требует аккуратной работы с сертификатами, поэтому шифрование не обязательно для первого запуска.

Если есть домен, указывающий на VPS, можно включить Let's Encrypt:

```bash
sudo DOMAIN=mqtt.example.ru EMAIL=you@example.ru ./install.sh
```

Скрипт выпустит сертификат через certbot в режиме standalone (нужен свободный порт 80), положит `fullchain.pem` и `privkey.pem` в `certs/`, включит слушатель 8883 и поставит hook обновления: при `certbot renew` сертификаты копируются заново и контейнер перезапускается (`hooks/renew-mosquitto.sh`). Проверка с компьютера:

```bash
mosquitto_sub -h mqtt.example.ru -p 8883 --cafile /etc/ssl/certs/ca-certificates.crt \
  -u USER -P PASS -t 'lamp/home/+/status' -v
```

Проверка обновления: `sudo certbot renew --dry-run`.

## Важно: 1883 — открытый текст

На порту 1883 логин, пароль и все сообщения идут без шифрования. Любой, кто видит ваш трафик (чужой Wi-Fi, провайдер), может перехватить пароль. Имеет смысл:

- не открывать 1883 всему интернету, если можете ограничить ufw сетями, откуда ходят лампы;
- либо пользоваться 8883 с компьютера и смириться с 1883 для ESP8266, понимая риск;
- не переиспользовать этот пароль нигде больше.

Не коммитьте `config/passwd`, `config/acl` и сертификаты — они в `.gitignore`.

## Резервные копии

Сохраняйте пароли, ACL и базу retained (после `docker compose stop` копия будет согласованной):

```bash
cd infra/broker
sudo docker compose stop
sudo tar czf ~/lamp-broker-$(date +%F).tgz config data mosquitto.conf conf.d
sudo docker compose start
```

Восстановление: распаковать в `infra/broker/` и `sudo docker compose up -d`. Каталог `log/` копировать не обязательно.

## Обновление брокера

Образ зафиксирован как `eclipse-mosquitto:2.0.22` в `docker-compose.yml` (линейка 2.x). Чтобы обновить, смените тег на новый 2.x, затем:

```bash
cd infra/broker
sudo docker compose pull
sudo docker compose up -d
```

Данные в `data/` и пользователи в `config/` при этом сохраняются. После обновления проверьте retained и вход одним из пользователей.

## Полезные команды

```bash
cd infra/broker
sudo docker compose ps
sudo docker compose logs -f --tail 100
sudo docker compose restart
```

Логи также пишутся в `log/mosquitto.log`.
