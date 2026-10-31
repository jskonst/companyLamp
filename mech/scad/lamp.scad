// Сборка корпуса companyLamp под VL0079.  PART = "assembly"|"exploded"|"base"|"cover"|"e14"|"pipe"|"led_ring"|"sled"
include <../lib/BOSL2/std.scad>
include <params.scad>
PART = "assembly";
EXPLODE = (PART == "exploded") ? 35 : 0;

// --- радиальная защёлка-лапка: BOSL2 cuboid + клиновой зуб (prismoid) ---
module snap_arm() {
    up(BAY_H + FLOOR) {
        cuboid([SNAP_T, SNAP_W, SNAP_L], anchor=BOT+RIGHT, rounding=0.5, edges="Z");
        // зуб наружу с фаской захода сверху: плафон надевается, лапка пружинит
        up(SNAP_L - 3) right(0)
            prismoid(size1=[SNAP_HOOK, SNAP_W], size2=[0.01, SNAP_W], h=3, shift=[-SNAP_HOOK/2,0], anchor=BOT+LEFT);
    }
}

module base() {
    difference() {
        union() {
            // корпус отсека: конус от BASE_BOT_D к уступу под плафоном
            tube(h=BAY_H+FLOOR, od1=BASE_BOT_D, od2=BODY_TOP_D, wall=WALL, anchor=BOT);
            // палуба под LED-кольцом
            up(BAY_H) cyl(h=FLOOR, d=BODY_TOP_D-1, anchor=BOT);
            // юбка: на неё садится край плафона
            up(BAY_H+FLOOR) tube(h=SKIRT_H, od=SKIRT_OD, wall=WALL, anchor=BOT);
            // E14 декор + кабельный ввод в центре
            up(BAY_H+FLOOR) e14_decor();
        }
        // прорези вокруг лапок
        zrot_copies(n=N_SNAPS) right(SKIRT_OD/2 - WALL/2)
            up(BAY_H+FLOOR+0.01) cuboid([WALL*3, SNAP_W+2*SNAP_GAP, SNAP_L+1], anchor=BOT);
        // проход провода
        down(1) cyl(h=BAY_H+FLOOR+E14_H+5, d=CABLE_D, anchor=BOT);
        // световод к leds[0] (ось X+): наклонный канал от бока до пикселя
        pipe_path() cyl(h=60, d=PIPE_D+0.3, anchor=CENTER);
        // кнопка на передней (-Y) стенке
        up(BAY_H/2) fwd(BASE_BOT_D/2) xrot(90) cyl(h=20, d=BTN_D, anchor=CENTER);
        // USB-C на задней (+Y) стенке
        up(USBC_Z) right(POWER_C.x+USBC_X) back(BASE_BOT_D/2) cuboid([USBC.x, 30, USBC.y], rounding=1.2, edges="Y");
        // вентиляция над зарядкой (задняя стенка)
        for (i=[0:VENT_SLOTS-1]) zrot(-30 + i*10) up(BAY_H*0.6) back(BASE_BOT_D/2)
            cuboid([2, 20, BAY_H*0.5], rounding=1, edges="Y");
        // гнёзда защёлок крышки
        zrot_copies(n=3) right(BASE_BOT_D/2-WALL) up(FLOOR+2) cuboid([WALL*2, 10, 2]);
    }
    zrot_copies(n=N_SNAPS) right(SKIRT_OD/2 - WALL/2 + SNAP_T/2) snap_arm();
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
module cover() {
    d = BASE_BOT_D - 2*WALL - 0.4;
    difference() {
        cyl(h=FLOOR, d=BASE_BOT_D, rounding1=1, anchor=BOT);
        // вентиляция дна под зарядкой
        back(25) xcopies(n=5, spacing=5) cuboid([2, 18, 10], rounding=1, edges="Z");
    }
    up(FLOOR) {
        // центрирующее кольцо крышки с вырезом под рамку платы питания и USB-C
        difference() {
            tube(h=6, od=d, wall=1.6, anchor=BOT);
            back(d/2) cuboid([POWER_PCB.x+6, 12, 20]);
        }
        // защёлки крышки (3 шт.)
        zrot_copies(n=3) right(d/2) up(4) prismoid([0.01,8],[1.2,8], h=2, shift=[0.6,0], anchor=BOT+LEFT);
        // Lolin: 4 стойки M2 h=STANDOFF_H
        move(LOLIN_C) grid_copies(spacing=LOLIN_HOLES, n=[2,2]) tube(h=STANDOFF_H, od=5, id=1.8, anchor=BOT);
        // площадка питания (TP4056+boost): рамка с упорами
        move(POWER_C) rect_tube(size=[POWER_PCB.x+2.4, POWER_PCB.y+2.4], wall=1.2, h=POWER_FRAME_H, anchor=BOT);
        // стяжка: петля для фиксации входного провода (strain relief)
        back(40) left(30) difference() { cuboid([6,8,6], anchor=BOT); up(1.5) cuboid([7,4,2.2], anchor=BOT); }
        if (WITH_BATTERY) move(BATT_C)
            rect_tube(size=[BATT_BAY.y+2.4, BATT_BAY.x+2.4], wall=1.2, h=BATT_BAY.z*0.6, anchor=BOT);
    }
}

module lolin_dummy() color("dodgerblue") up(FLOOR+STANDOFF_H) move(LOLIN_C) cuboid(LOLIN, anchor=BOT);
module power_dummy() color("darkred") up(FLOOR+POWER_FRAME_H) move(POWER_C) { cuboid(POWER_PCB, anchor=BOT); color("silver") up(POWER_PCB.z) back(POWER_PCB.y/2-3.5) right(USBC_X) cuboid([8.9,7.5,USBC_BODY_H], rounding=1.4, edges="Y", anchor=BOT); }
module batt_dummy() if (WITH_BATTERY) color("green") up(FLOOR+1) move(BATT_C) xcyl(h=BATT_CELL[1], d=BATT_CELL[0], anchor=BOT);

module shade() color("white", 0.35) up(BAY_H+FLOOR) difference() {
    cyl(h=SHADE_H, d1=SHADE_LOWER_D, d2=SHADE_TOP_D, rounding2=8, anchor=BOT);
    down(0.01) cyl(h=SHADE_H-SHADE_WALL, d1=SHADE_LOWER_D-2*SHADE_WALL, d2=SHADE_TOP_D-2*SHADE_WALL, anchor=BOT);
    cyl(h=SHADE_H*3, d=SHADE_TOP_HOLE);
}

module assembly(e) {
    color("ivory") base();
    down(e*1.5) { color("gainsboro") cover(); lolin_dummy(); power_dummy(); batt_dummy(); }
    up(BAY_H+FLOOR+0.5+e*0.6) { led_ring(); up(LED_PCB_T) led_pixels(); }
    left(e) light_pipe();
    up(e*2.5) shade();
}

if (PART=="assembly" || PART=="exploded") assembly(EXPLODE);
else if (PART=="base") base();
else if (PART=="cover") cover();
else if (PART=="pipe") light_pipe();
else if (PART=="led_ring") led_ring();
