// Сборка корпуса companyLamp под VL0079.  PART = "assembly"|"exploded"|"base"|"cover"|"latch_ring"|"e14"|"pipe"|"led_ring"|"sled"
include <../lib/BOSL2/std.scad>
include <params.scad>
PART = "assembly";
EXPLODE = (PART == "exploded") ? 35 : 0;

DECK = BAY_H + FLOOR;   // верх палубы основания = дно паза

// Мелкий паз: стакан (наружная стенка) + внутренний бортик, край плафона
// заходит на SEAT_DEPTH. На стакане снаружи буртик под зубья кольца.
module seat() {
    tube(h=SEAT_DEPTH, od=COLLAR_OD, id=GROOVE_OD, anchor=BOT);
    difference() {
        tube(h=LIP_H, od=GROOVE_ID, wall=1.6, anchor=BOT);
        // вырез в бортике под провода ленты
        zrot(WIRE_ANG) right(GROOVE_ID/2) cuboid([8, WIRE_NOTCH, 3*LIP_H]);
    }
    // буртик с фаской сверху (зубья набегают), снизу полка 90°
    up(BEAD_Z) cyl(h=BEAD*1.2, d1=COLLAR_OD+2*BEAD, d2=COLLAR_OD, anchor=BOT);
}

// Прижимное кольцо (по CaseCap.scad, резьба заменена N защёлками):
// плоское кольцо с конусным отверстием по плафону, давит на край сверху;
// N лапок вниз, зуб цепляется под буртик стакана.
module latch_ring() {
    z0 = SEAT_DEPTH;
    difference() {
        up(z0) cyl(h=RING_T, d=RING_OD, rounding2=1.2, anchor=BOT);
        up(z0-0.01) cyl(h=RING_T+0.02, d1=shade_d(z0)+2*FIT, d2=shade_d(z0+RING_T)+2*FIT, anchor=BOT);
    }
    zrot_copies(n=N_SNAPS) {
        // лапка: от низа кольца до низа буртика
        right(ARM_R) up(BEAD_Z-0.1-HOOK_H) cuboid([SNAP_T, SNAP_W, z0-BEAD_Z+0.1+HOOK_H+0.6], rounding=0.4, edges="Z", anchor=BOT+LEFT);
        // зуб внутрь под буртик, фаска захода снизу
        right(ARM_R+0.01) up(BEAD_Z-0.1) prismoid(size1=[0.01, SNAP_W], size2=[SNAP_HOOK, SNAP_W], h=HOOK_H, shift=[-SNAP_HOOK/2,0], anchor=TOP+RIGHT);
    }
}

module base() {
    difference() {
        union() {
            tube(h=DECK, od1=BASE_BOT_D, od2=BODY_TOP_D, wall=WALL, anchor=BOT);
            up(BAY_H) cyl(h=FLOOR, d=BODY_TOP_D-1, anchor=BOT);
            up(DECK) seat();
            up(DECK) e14_decor();
            deck_mounts();
        }
        down(1) cyl(h=DECK+E14_H+5, d=CABLE_D, anchor=BOT);
        // проход проводов ленты между кольцом 86 и бортиком
        zrot(WIRE_ANG) right((LED_RING_OD/2 + GROOVE_ID/2-1.6)/2) up(BAY_H-1) cyl(h=FLOOR+3, d=WIRE_D, anchor=BOT);
        pipe_path() cyl(h=60, d=PIPE_D+0.3, anchor=CENTER);
        up(BAY_H/2) fwd(BASE_BOT_D/2) xrot(90) cyl(h=20, d=BTN_D, anchor=CENTER);
        up(USBC_Z) right(POWER_C.x+USBC_X) back(BASE_BOT_D/2) cuboid([USBC.x, 30, USBC.y], rounding=1.2, edges="Y");
        for (i=[0:VENT_SLOTS-1]) zrot(-30 + i*10) up(BAY_H*0.3) back(BASE_BOT_D/2)
            cuboid([2, 20, BAY_H*0.35], rounding=1, edges="Y");
        zrot_copies(n=3) right(BASE_BOT_D/2-WALL) up(FLOOR+2) cuboid([WALL*2, 10, 2]);
    }
}

module e14_decor() difference() {
    union() {
        cyl(h=E14_H, d=E14_OD, rounding2=2, anchor=BOT);
        // рёбра «резьбы» E14 — декор, не крепёж
        for (z=[4:4:E14_H-4]) up(z) torus(d_maj=E14_OD, d_min=1.2);
    }
    up(2) cyl(h=E14_H, d=E14_OD-2*WALL, anchor=BOT);
}

module pipe_path() // от передне-правого бока к leds[0] (угол 0°)
    up(BAY_H*0.75) right(LED_RING_OD/2-3) yrot(-55) children();

module light_pipe() color("white", 0.6) pipe_path() cyl(h=50, d=PIPE_D, anchor=CENTER);

module led_ring() color("lightyellow") difference() {
    tube(h=LED_PCB_T+1.5, od=LED_RING_OD, id=LED_RING_ID, anchor=BOT);
}
module led_pixels() zrot_copies(n=LED_N, r=(LED_RING_OD+LED_RING_ID)/4) color("white") cuboid([5,5,1.6], anchor=BOT);

// Крышка дна: салазки Lolin, площадка питания, отсек 18650, ножки.
// --- Узлы электроники закреплены НА ПАЛУБЕ основания (висят вниз, компоненты вниз) ---
PWR_Z  = BAY_H - POWER_FRAME_H - POWER_PCB.z;      // низ платы питания
LOL_Z  = BAY_H - STANDOFF_H - LOLIN.z;              // низ Lolin
CELL_Z = BAY_H - 1 - BATT_CELL[0];                  // низ ячейки 18650
module deck_mounts() down(0.01) {
    // Lolin: 4 стойки M2 вниз от палубы
    move(LOLIN_C) grid_copies(spacing=LOLIN_HOLES, n=[2,2]) up(BAY_H) tube(h=STANDOFF_H, od=5, id=1.8, anchor=TOP);
    // рамка платы питания вниз от палубы
    move(POWER_C) up(BAY_H) rect_tube(size=[POWER_PCB.x+2.4, POWER_PCB.y+2.4], wall=1.2, h=POWER_FRAME_H, anchor=TOP);
    // отсек 18650 вниз от палубы
    if (WITH_BATTERY) move(BATT_C) up(BAY_H)
        rect_tube(size=[BATT_BAY.y+2.4, BATT_BAY.x+2.4], wall=1.2, h=BATT_BAY.z*0.6, anchor=TOP);
}

// Крышка дна: только прижимные рёбра снизу, без посадочных мест.
CL = 0.3;   // зазор рёбер до платы/ячейки
module cover() {
    d = BASE_BOT_D - 2*WALL - 0.4;
    difference() {
        cyl(h=FLOOR, d=BASE_BOT_D, rounding1=1, anchor=BOT);
        back(25) xcopies(n=5, spacing=5) cuboid([2, 18, 10], rounding=1, edges="Z");
    }
    up(FLOOR) {
        difference() {
            tube(h=6, od=d, wall=1.6, anchor=BOT);
            back(d/2) cuboid([POWER_PCB.x+6, 12, 20]);
        }
        zrot_copies(n=3) right(d/2) up(4) prismoid([0.01,8],[1.2,8], h=2, shift=[0.6,0], anchor=BOT+LEFT);
        // рёбра под длинные края платы питания (в обход разъёма USB-C сзади)
        move(POWER_C) fwd(5) xcopies(spacing=POWER_PCB.x-3, n=2)
            cuboid([1.6, POWER_PCB.y-14, PWR_Z-CL-FLOOR], anchor=BOT);
        // рёбра под края Lolin (поперёк, у коротких сторон)
        move(LOLIN_C) xcopies(spacing=LOLIN.x-3, n=2)
            cuboid([1.6, LOLIN.y-6, LOL_Z-CL-FLOOR], anchor=BOT);
        // две ложементы под 18650
        if (WITH_BATTERY) move(BATT_C) xcopies(spacing=BATT_CELL[1]*0.6, n=2) difference() {
            cuboid([4, 14, CELL_Z-FLOOR+4], anchor=BOT);
            up(CELL_Z-FLOOR+BATT_CELL[0]/2-CL) xcyl(d=BATT_CELL[0], h=10);
        }
        back(40) left(30) difference() { cuboid([6,8,6], anchor=BOT); up(1.5) cuboid([7,4,2.2], anchor=BOT); }
    }
}

module lolin_dummy() color("dodgerblue") up(LOL_Z) move(LOLIN_C) cuboid(LOLIN, anchor=BOT);
module power_dummy() color("darkred") up(PWR_Z) move(POWER_C) { cuboid(POWER_PCB, anchor=BOT); color("silver") back(POWER_PCB.y/2-3.5) right(USBC_X) cuboid([8.9,7.5,USBC_BODY_H], rounding=1.4, edges="Y", anchor=TOP); }
module batt_dummy() if (WITH_BATTERY) color("green") up(CELL_Z) move(BATT_C) xcyl(h=BATT_CELL[1], d=BATT_CELL[0], anchor=BOT);

module shade() color("white", 0.35) difference() {
    cyl(h=SHADE_H, d1=SHADE_LOWER_D, d2=SHADE_TOP_D, rounding2=8, anchor=BOT);
    down(0.01) cyl(h=SHADE_H-SHADE_WALL, d1=SHADE_LOWER_D-2*SHADE_WALL, d2=SHADE_TOP_D-2*SHADE_WALL, anchor=BOT);
    cyl(h=SHADE_H*3, d=SHADE_TOP_HOLE);
}

module assembly(e) {
    color("ivory") base();
    lolin_dummy(); power_dummy(); batt_dummy();
    down(e*1.5) color("gainsboro") cover();
    up(DECK+0.5+e*0.6) { led_ring(); up(LED_PCB_T) led_pixels(); }
    left(e) light_pipe();
    up(DECK+e*2.0) shade();
    up(DECK+e*4.2) color("orange") latch_ring();
}

if (PART=="assembly" || PART=="exploded") assembly(EXPLODE);
else if (PART=="base") base();
else if (PART=="cover") cover();
else if (PART=="latch_ring") latch_ring();
else if (PART=="pipe") light_pipe();
else if (PART=="led_ring") led_ring();
