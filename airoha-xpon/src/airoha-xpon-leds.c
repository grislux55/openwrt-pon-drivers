// SPDX-License-Identifier: GPL-2.0-only

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/slab.h>

#include "airoha-xpon-epon.h"
#include "airoha-xpon-private.h"

#ifdef CONFIG_LEDS_TRIGGERS

#define AIROHA_XPON_LED_FAST_BLINK_MS 200
#define AIROHA_XPON_LED_SLOW_BLINK_MS 500

enum airoha_xpon_led_state {
	AIROHA_XPON_LED_UNKNOWN = -1,
	AIROHA_XPON_LED_OFF,
	AIROHA_XPON_LED_FAST_BLINK,
	AIROHA_XPON_LED_SLOW_BLINK,
	AIROHA_XPON_LED_ON,
};

struct airoha_xpon_led_trig {
	struct led_trigger trig;
	bool registered;
	enum airoha_xpon_led_state state;
	char name[IFNAMSIZ + sizeof("-registration")];
};

struct airoha_xpon_leds {
	struct device *dev;
	struct mutex lock;
	struct airoha_xpon_led_trig registration;
	struct airoha_xpon_led_trig los;
	struct airoha_xpon_led_trig online;
};

static int airoha_xpon_led_trig_activate(struct led_classdev *led_cdev)
{
	struct airoha_xpon_led_trig *t =
		container_of(led_cdev->trigger, struct airoha_xpon_led_trig,
		             trig);
	enum airoha_xpon_led_state state = READ_ONCE(t->state);

	switch (state) {
	case AIROHA_XPON_LED_FAST_BLINK:
		led_blink_set_nosleep(led_cdev, AIROHA_XPON_LED_FAST_BLINK_MS,
		                      AIROHA_XPON_LED_FAST_BLINK_MS);
		break;
	case AIROHA_XPON_LED_SLOW_BLINK:
		led_blink_set_nosleep(led_cdev, AIROHA_XPON_LED_SLOW_BLINK_MS,
		                      AIROHA_XPON_LED_SLOW_BLINK_MS);
		break;
	case AIROHA_XPON_LED_ON:
		led_blink_set_nosleep(led_cdev, 1, 0);
		break;
	case AIROHA_XPON_LED_OFF:
	case AIROHA_XPON_LED_UNKNOWN:
	default:
		led_set_brightness(led_cdev, LED_OFF);
		break;
	}

	return 0;
}

static void airoha_xpon_led_trig_init(struct airoha_xpon_led_trig *t,
                                      const char *pon_name,
                                      const char *suffix)
{
	t->state = AIROHA_XPON_LED_UNKNOWN;
	snprintf(t->name, sizeof(t->name), "%s-%s", pon_name, suffix);
	t->trig.name = t->name;
	t->trig.activate = airoha_xpon_led_trig_activate;
	if (!led_trigger_register(&t->trig))
		t->registered = true;
}

static void airoha_xpon_led_trig_cleanup(struct airoha_xpon_led_trig *t)
{
	if (!t->registered)
		return;

	led_trigger_unregister(&t->trig);
	t->registered = false;
}

static void airoha_xpon_led_apply(struct airoha_xpon_led_trig *t,
                                  enum airoha_xpon_led_state state)
{
	if (!t->registered)
		return;

	/* The LED core treats delay_off=0 as steady on and stops the blink timer. */
	switch (state) {
	case AIROHA_XPON_LED_FAST_BLINK:
		led_trigger_blink(&t->trig, AIROHA_XPON_LED_FAST_BLINK_MS,
		                  AIROHA_XPON_LED_FAST_BLINK_MS);
		break;
	case AIROHA_XPON_LED_SLOW_BLINK:
		led_trigger_blink(&t->trig, AIROHA_XPON_LED_SLOW_BLINK_MS,
		                  AIROHA_XPON_LED_SLOW_BLINK_MS);
		break;
	case AIROHA_XPON_LED_ON:
		led_trigger_blink(&t->trig, 1, 0);
		break;
	case AIROHA_XPON_LED_OFF:
	default:
		led_trigger_event(&t->trig, LED_OFF);
		break;
	}
}

struct airoha_xpon_leds *airoha_xpon_leds_create(struct device *dev,
                                                 const char *pon_name)
{
	struct airoha_xpon_leds *leds;

	leds = kzalloc(sizeof(*leds), GFP_KERNEL);
	if (!leds)
		return NULL;

	leds->dev = dev;
	mutex_init(&leds->lock);
	airoha_xpon_led_trig_init(&leds->registration, pon_name,
	                          "registration");
	airoha_xpon_led_trig_init(&leds->los, pon_name, "los");
	airoha_xpon_led_trig_init(&leds->online, pon_name, "online");

	if (!leds->registration.registered && !leds->los.registered &&
	    !leds->online.registered) {
		mutex_destroy(&leds->lock);
		kfree(leds);
		return NULL;
	}

	dev_info(dev, "PON LED triggers registered: %s, %s, %s\n",
	         leds->registration.name, leds->los.name, leds->online.name);
	return leds;
}

void airoha_xpon_leds_destroy(void *data)
{
	struct airoha_xpon_leds *leds = data;

	if (!leds)
		return;

	/* Line teardown publishes OFF before unregistering trigger consumers. */
	airoha_xpon_led_trig_cleanup(&leds->online);
	airoha_xpon_led_trig_cleanup(&leds->los);
	airoha_xpon_led_trig_cleanup(&leds->registration);
	mutex_destroy(&leds->lock);
	kfree(leds);
}

void airoha_xpon_leds_update(struct airoha_xpon *xpon)
{
	struct airoha_xpon_leds *leds = xpon->leds;
	enum airoha_xpon_led_state registration, los, online;
	bool line_running, optical_signal;

	if (!leds)
		return;

	/* Optical loss uses a 500 ms blink period. */
	line_running = READ_ONCE(xpon->lifecycle) != AIROHA_XPON_STOPPED;
	optical_signal = READ_ONCE(xpon->optical_signal);
	los = line_running && !optical_signal ? AIROHA_XPON_LED_SLOW_BLINK :
	                                        AIROHA_XPON_LED_OFF;
	online = line_running && optical_signal &&
	                         READ_ONCE(xpon->service_ready) ?
	                 AIROHA_XPON_LED_ON :
	                 AIROHA_XPON_LED_OFF;

	if (!line_running || !optical_signal) {
		registration = AIROHA_XPON_LED_OFF;
	} else if (READ_ONCE(xpon->active_mode_valid) &&
	           airoha_xpon_mode_is_epon(READ_ONCE(xpon->active_mode))) {
		/* A valid LLID marks completed EPON MPCP registration. */
		registration = READ_ONCE(xpon->epon.mpcp_state) ==
		                               AIROHA_EPON_MPCP_REGISTERED ?
		                       AIROHA_XPON_LED_ON :
		                       AIROHA_XPON_LED_FAST_BLINK;
	} else {
		switch (READ_ONCE(xpon->onu_state)) {
		case AIROHA_XGPON_O5:
			registration = AIROHA_XPON_LED_ON;
			break;
		case AIROHA_XGPON_O4:
			registration = AIROHA_XPON_LED_SLOW_BLINK;
			break;
		case AIROHA_XGPON_O1:
		case AIROHA_XGPON_O2_3:
		default:
			registration = AIROHA_XPON_LED_FAST_BLINK;
			break;
		}
	}

	/* Keep the LED mutex outside the state_lock/ploam_lock hardware order. */
	mutex_lock(&leds->lock);
	if (READ_ONCE(leds->los.state) != los) {
		WRITE_ONCE(leds->los.state, los);
		airoha_xpon_led_apply(&leds->los, los);
	}
	if (READ_ONCE(leds->registration.state) != registration) {
		WRITE_ONCE(leds->registration.state, registration);
		airoha_xpon_led_apply(&leds->registration, registration);
	}
	if (READ_ONCE(leds->online.state) != online) {
		WRITE_ONCE(leds->online.state, online);
		airoha_xpon_led_apply(&leds->online, online);
	}
	mutex_unlock(&leds->lock);
}

#else /* !CONFIG_LEDS_TRIGGERS */

struct airoha_xpon_leds *airoha_xpon_leds_create(struct device *dev,
                                                 const char *pon_name)
{
	return NULL;
}

void airoha_xpon_leds_destroy(void *data)
{
}

void airoha_xpon_leds_update(struct airoha_xpon *xpon)
{
}

#endif /* CONFIG_LEDS_TRIGGERS */
