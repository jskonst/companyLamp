uint32_t maxFlashMs = 0;

void buttonTick() {
  btn.tick();

  // клики
  uint8_t clicks = btn.getClicks();
  if (clicks) {
    if (clicks == 1) {   // вкл выкл
      data.power = !data.power;
      sendPacket();
      memory.update();
    } else if (clicks == 2) {   // сменить цвет
      data.color += 32;
      sendPacket();
      memory.update();
    } else if (clicks == 3) {   // подмигнуть
      winkFlag = 1;
      sendPacket();
    }
  }

  // импульсное удержание
  static int8_t dir = 10;
  if (btn.step()) {
    data.bright = constrain(data.bright + dir, 0, 255);
    if (data.bright == 255) maxFlashMs = millis();  // мигнуть на максимуме без delay — ведёт animation()
  }
  if (btn.releaseStep()) {
    dir = -dir;
    memory.update();
  }
}
