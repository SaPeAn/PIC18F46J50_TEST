#ifndef USB_H
#define	USB_H

#include "system.h"

/*
 * USB HID-клавиатура для PIC18F46J50.
 *
 * Модуль поднимает USB-устройство класса HID (Boot Interface, Keyboard).
 * При подключении к ПК контроллер определяется как обычная USB-клавиатура,
 * а прошивка может "печатать" произвольные ASCII-символы в активное окно.
 *
 * Требования к тактированию (уже выполнены в config.h):
 *   OSC = HSPLL, PLLDIV = 1 (кварц 4 МГц), CPUDIV = OSC1
 *   -> 96 МГц PLL / 2 = 48 МГц: это и системная частота, и такт USB.
 *
 * Требования к схеме:
 *   D-/D+ на RC4/RC5, вывод VUSB подключён (для VDD = 3.3 В — на VDD
 *   через керамику 0.1...0.47 мкФ), подтяжка линии — встроенная (UPUEN).
 *
 * Порядок инициализации (см. main.c):
 *   sys_init();   // должен быть вызван до USB_init() — нужен таймер и GIE/PEIE
 *   USB_init();
 */

#define      USB_KBD_BUFLEN           64    // очередь символов на "печать"

// Модификаторы HID-клавиатуры (байт 0 отчёта)
#define      USB_KBD_MOD_LCTRL        0x01
#define      USB_KBD_MOD_LSHIFT       0x02
#define      USB_KBD_MOD_LALT         0x04
#define      USB_KBD_MOD_LGUI         0x08

// Светодиоды клавиатуры (приходят от хоста через SET_REPORT)
#define      USB_KBD_LED_NUMLOCK      0x01
#define      USB_KBD_LED_CAPSLOCK     0x02
#define      USB_KBD_LED_SCROLLLOCK   0x04

/** Инициализация USB-модуля и запуск подключения к шине. */
void USB_init(void);

/** Останов USB-модуля, отключение от шины. */
void USB_deinit(void);

/**
 * @return 1, если устройство сконфигурировано хостом и готово "печатать",
 *         0 — если не подключено, не перечислено или переведено в suspend.
 */
uint8_t USB_is_configured(void);

/**
 * Поставить один ASCII-символ в очередь на "печать".
 * @return 1 — символ поставлен в очередь, 0 — очередь переполнена / не готово.
 */
int16_t USB_kbd_putchar(char c);

/**
 * Поставить в очередь строку (до первого '\0' либо до Nmax символов).
 * @return число реально поставленных в очередь символов, -1 — не готово.
 */
int16_t USB_kbd_putstring(const char* str, uint16_t Nmax);

/**
 * Поставить в очередь одиночное нажатие по HID Usage ID (US-раскладка)
 * с заданными модификаторами. Позволяет отправлять то, чего нет в ASCII:
 * стрелки, F1..F12, Enter, Ctrl+Alt+Del и т.п.
 * Пример: USB_kbd_putkey(0x4F, 0) — стрелка вправо.
 */
int16_t USB_kbd_putkey(uint8_t usage, uint8_t modifiers);

/** Текущее состояние светодиодов клавиатуры, присланное хостом. */
uint8_t USB_kbd_getleds(void);

#endif	/* USB_H */
