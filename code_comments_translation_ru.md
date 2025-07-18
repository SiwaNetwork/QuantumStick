# Перевод комментариев из кода драйвера TimeStick

## Основные комментарии и их перевод

### Из файла ax_main.h:

```c
/* SPDX-License-Identifier: GPL-2.0 */
/* SPDX-Идентификатор лицензии: GPL-2.0 */

/*******************************************************************************
 *     Copyright (c) 2022    ASIX Electronic Corporation    All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 ******************************************************************************/
/*******************************************************************************
 *     Авторские права (c) 2022    ASIX Electronic Corporation    Все права защищены.
 *
 * Эта программа является свободным программным обеспечением: вы можете распространять
 * и/или изменять её в соответствии с условиями Стандартной общественной лицензии GNU,
 * опубликованной Фондом свободного программного обеспечения, версии 2 Лицензии или
 * (по вашему выбору) любой более поздней версии.
 *
 * Эта программа распространяется в надежде, что она будет полезной, но БЕЗ КАКИХ-ЛИБО
 * ГАРАНТИЙ; даже без подразумеваемой гарантии КОММЕРЧЕСКОЙ ЦЕННОСТИ или ПРИГОДНОСТИ
 * ДЛЯ ОПРЕДЕЛЕННОЙ ЦЕЛИ. Подробности см. в Стандартной общественной лицензии GNU.
 *
 * Вы должны были получить копию Стандартной общественной лицензии GNU вместе с
 * этой программой. Если нет, см. <https://www.gnu.org/licenses/>.
 ******************************************************************************/
```

### Из файла ax88179a_772d.c - настройки скорости:

```c
struct _ax_buikin_setting AX88179A_BULKIN_SIZE[] = {
	{5, 0x7B, 0x00,	0x18, 0x0F},	//1G, SS       // 1 Гбит/с, SuperSpeed USB
	{5, 0xC0, 0x02,	0x06, 0x0F},	//1G, HS       // 1 Гбит/с, HighSpeed USB
	{7, 0xF0, 0x00,	0x0C, 0x0F},	//100M, Full, SS  // 100 Мбит/с, полный дуплекс, SuperSpeed
	{6, 0x00, 0x00,	0x06, 0x0F},	//100M, Half, SS  // 100 Мбит/с, полудуплекс, SuperSpeed
	{5, 0xC0, 0x04,	0x06, 0x0F},	//100M, Full, HS  // 100 Мбит/с, полный дуплекс, HighSpeed
	{7, 0xC0, 0x04,	0x06, 0x0F},	//100M, Half, HS  // 100 Мбит/с, полудуплекс, HighSpeed
	{7, 0x00, 0,	0x03, 0x3F},	//FS           // FullSpeed USB
};

struct _ax_buikin_setting AX88279_BULKIN_SIZE[] = {
	{5, 0x10, 0x01,	0x11, 0x0F},	//2.5G         // 2.5 Гбит/с
	{7, 0xB3, 0x01,	0x11, 0x0F},	//1G, SS       // 1 Гбит/с, SuperSpeed USB
	// ... остальные настройки аналогично
};
```

### Из файла ax_ptp.h - константы PTP:

```c
#define AX_PTP_CMD		0x09
#define AX_PTP_OP		0x0E
	#define AX_SET_LOCAL_CLOCK	0x01    // Установить локальные часы
	#define AX_SET_LOCAL_CLOCK_SIZE		0x0A    // Размер данных для установки часов
	#define AX_GET_LOCAL_CLOCK	0x02    // Получить время локальных часов
	#define AX_GET_LOCAL_CLOCK_SIZE		0x0A    // Размер данных для получения времени
	#define AX_SET_ADDEND		0x03    // Установить коэффициент коррекции частоты
	#define AX_SET_ADDEND_SIZE		0x04    // Размер данных коэффициента
	#define AX_SET_ACTIVE_TIME	0x06    // Установить время активации
	#define AX_SET_ACTIVE_TIME_SIZE	0x04    // Размер данных времени активации
	#define AX_SET_TX_PHY_DELAY	0x07    // Установить задержку передачи PHY
	#define AX_SET_TX_PHY_DELAY_SIZE	0x05    // Размер данных задержки TX
	#define AX_SET_RX_PHY_DELAY	0x08    // Установить задержку приема PHY

// Ethernet: 14B, IPv4: 20B, UDP: 8B
#define AX_TX_PTPHDR_OFFSET_L3_IP	42    // Смещение PTP заголовка для IPv4
// Ethernet: 14B, IPv6: 40B, UDP: 8B
#define AX_TX_PTPHDR_OFFSET_L3_IPV6	62    // Смещение PTP заголовка для IPv6
// Ethernet: 14B
#define AX_TX_PTPHDR_OFFSET_L2	14    // Смещение PTP заголовка для уровня L2

#define AX_PPS_ACTIVE_DEFAULT_TIME	0x1DCD6500    // Время активации по умолчанию для PPS
#define AX_BASE_ADDEND			0xCCCCCCCC    // Базовый коэффициент частоты
```

### Из файла ax_main.c - параметры модуля:

```c
static int autodetach = -1;
module_param(autodetach, int, 0);
MODULE_PARM_DESC(autodetach, "Autodetach configuration");
// Параметр автоотключения - конфигурация автоматического отключения устройства

static int bctrl = -1;
module_param(bctrl, int, 0);
MODULE_PARM_DESC(bctrl, "RX Bulk Control");
// Управление массовым приемом - настройка режима массового приема данных

static int blwt = -1;
module_param(blwt, int, 0);
MODULE_PARM_DESC(blwt, "RX Bulk Timer Low");
// Нижний таймер массового приема - минимальное время ожидания пакетов

static int bhit = -1;
module_param(bhit, int, 0);
MODULE_PARM_DESC(bhit, "RX Bulk Timer High");
// Верхний таймер массового приема - максимальное время ожидания пакетов

static int bsize = -1;
module_param(bsize, int, 0);
MODULE_PARM_DESC(bsize, "RX Bulk Queue Size");
// Размер очереди массового приема - количество пакетов в очереди

static int bifg = -1;
module_param(bifg, int, 0);
MODULE_PARM_DESC(bifg, "RX Bulk Inter Frame Gap");
// Межкадровый интервал массового приема - промежуток между кадрами
```

### Из файла ax88179_178a.c - работа со светодиодами:

```c
/* loaded the old eFuse LED Mode */
/* загружен старый режим LED из eFuse */

} else { /* loaded the old EEprom LED Mode */
} else { /* загружен старый режим LED из EEPROM */

/* LED full duplex setting */
/* Настройка LED для полного дуплекса */

if (ledvalue & LED2_FD) /* LED2_FD */
// Если LED2 настроен на индикацию полного дуплекса

else if ((ledvalue & LED2_USB3_MASK) == 0) /* LED2_USB3 */
// Иначе если LED2 настроен на индикацию USB3

/* Disable */
/* Отключить */
```

### Из файла ax_ptp.c - функции PTP:

```c
static void ax_reset_ptp_queue(struct ax_device *axdev)
{
	// Сброс очереди PTP временных меток
	struct ax_ptp_cfg *ptp_cfg = axdev->ptp_cfg;

	if (!ptp_cfg)
		return;

	ptp_cfg->ptp_head = 0;    // Сброс головы очереди
	ptp_cfg->ptp_tail = 0;    // Сброс хвоста очереди
	ptp_cfg->num_items = 0;   // Обнуление количества элементов
	ptp_cfg->get_timestamp_retry = 0;    // Сброс счетчика попыток получения временной метки

	memset(ptp_cfg->tx_ptp_info, 0, AX_PTP_INFO_SIZE * AX_PTP_QUEUE_SIZE);
}

int ax88179a_ptp_pps_ctrl(struct ax_device *axdev, u8 enable)
{
	// Управление сигналом PPS (Pulse Per Second - импульс в секунду)
	u32 reg32 = 0;
	int ret;
	
	// Чтение регистра управления PPS
	ret = ax_read_cmd(axdev, AX88179A_PBUS_REG, 0x1894, 0x000F, 4, &reg32, 1);
	if (ret < 0)
		return ret;

	reg32 &= ~0x01000000;    // Очистка бита включения PPS

	if (enable) 
		reg32 |= 0x01000000;    // Установка бита включения PPS

	// Запись обновленного значения обратно в регистр
	ret = ax_write_cmd(axdev, AX88179A_PBUS_REG, 0x1894, 0x000F, 4, &reg32);
	if (ret < 0)
		return ret;
	
	return 0;
}
```

## Описание ключевых функций

### Функции управления временем:

- **ax88179a_ptp_adjtime** - корректировка времени PTP часов
- **ax88179a_ptp_adjfine/adjfreq** - точная подстройка частоты часов
- **ax88179a_ptp_gettime64** - получение текущего времени в формате 64 бит
- **ax88179a_ptp_settime64** - установка времени в формате 64 бит

### Функции управления 1PPS:

- **ax88179a_ptp_pps_ctrl** - включение/выключение генерации сигнала 1PPS для чипа AX88179A
- **ax88279_ptp_pps_ctrl** - включение/выключение генерации сигнала 1PPS для чипа AX88279

### Сетевые функции:

- **ax_start_xmit** - начало передачи пакета
- **ax_start_rx** - запуск приема пакетов
- **ax_stop_rx** - остановка приема пакетов
- **ax_tx_timeout** - обработка таймаута передачи

## Важные константы:

- **AX_PPS_ACTIVE_DEFAULT_TIME** (0x1DCD6500) - время активации PPS по умолчанию (500 миллисекунд)
- **AX_BASE_ADDEND** (0xCCCCCCCC) - базовый коэффициент для корректировки частоты
- **NSEC_PER_SEC** - количество наносекунд в секунде (1,000,000,000)
- **AX_PTP_QUEUE_SIZE** - размер очереди PTP временных меток