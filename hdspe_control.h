// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file hdspe-control.h
 * @brief RME HDSPe sound card driver status and control interface helpers.
 *
 * 20210728,0907,08,09,10 - Philippe.Bekaert@uhasselt.be
 *
 * Based on earlier work of the other MODULE_AUTHORs,
 * information kindly made available by RME (www.rme-audio.com),
 * and hardware kindly made available by Amptec Belgium (www.amptec.be).
 */

#ifndef _HDSPE_CONTROL_H_
#define _HDSPE_CONTROL_H_

// TODO: when to use locking / use_is_eclusive() ?
// TODO: put() argument range checking needed?
// TODO: put() return value? always 0, or 1 when changed?

#include <sound/control.h>

/**
 * ENUMERATE_CTL_INFO - enumerated snd_kcontrol_new struct info method
 * helper.
 */
#define ENUMERATED_CTL_INFO(info, texts) \
	snd_ctl_enum_info(info, 1, ARRAY_SIZE(texts), texts)

/**
 * HDSPE_RO_KCTL - generate a snd_kcontrol_new struct for a read-only 
 * non-volatile property.
 * @xface: MIXER, CARD, etc...
 * @xname: display name for the property.
 * @prop: source code name for the property. 
 */
#define HDSPE_RO_KCTL(xface, xname, prop)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_##xface,	\
	.name = xname,				\
	.access = SNDRV_CTL_ELEM_ACCESS_READ,	\
	.info = snd_hdspe_info_##prop,		\
	.get = snd_hdspe_get_##prop		\
}

/**
 * HDSPE_RV_KCTL - generate a snd_kcontrol_new struct for a read-only 
 * volatile property.
 * @xface: MIXER, CARD, etc...
 * @xname: display name for the property.
 * @prop: source code name for the property. 
 */
#define HDSPE_RV_KCTL(xface, xname, prop)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_##xface,	\
	.name = xname,				\
	.access = SNDRV_CTL_ELEM_ACCESS_READ |	\
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,	\
	.info = snd_hdspe_info_##prop,		\
	.get = snd_hdspe_get_##prop		\
}

/**
 * HDSPE_RW_KCTL - generate a snd_kcontrol_new struct for a read-write property.
 * @xface: MIXER, CARD, etc...
 * @xname: display name for the property.
 * @prop: source code name for the property. 
 */
#define HDSPE_RW_KCTL(xface, xname, prop)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_##xface,	\
	.name = xname,				\
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,	\
	.info = snd_hdspe_info_##prop,		\
	.get = snd_hdspe_get_##prop,		\
	.put = snd_hdspe_put_##prop		\
}

/**
 * HDSPE_WO_KCTL - generate a snd_kcontrol_new struct for a write-only property.
 * @xface: MIXER, CARD, etc...
 * @xname: display name for the property.
 * @prop: source code name for the property. 
 */
#define HDSPE_WO_KCTL(xface, xname, prop)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_##xface,	\
	.name = xname,				\
	.access = SNDRV_CTL_ELEM_ACCESS_WRITE,	\
	.info = snd_hdspe_info_##prop,		\
	.put = snd_hdspe_put_##prop		\
}

/**
 * HDSPE_RO_BOOL_KCTL - generate a snd_kcontrol_new struct for a 
 * boolean read-only non-volatile property.
 * @xface: MIXER, CARD, etc...
 * @xname: display name for the property.
 * @prop: source code name for the property. 
 */
#define HDSPE_RO_BOOL_KCTL(xface, xname, prop)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_##xface,	\
	.name = xname,				\
	.access = SNDRV_CTL_ELEM_ACCESS_READ,	\
	.info = snd_ctl_boolean_mono_info,	\
	.get = snd_hdspe_get_##prop		\
}

/**
 * HDSPE_RV_BOOL_KCTL - generate a snd_kcontrol_new struct for a 
 * boolean read-only volatile property.
 * @xface: MIXER, CARD, etc...
 * @xname: display name for the property.
 * @prop: source code name for the property. 
 */
#define HDSPE_RV_BOOL_KCTL(xface, xname, prop)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_##xface,	\
	.name = xname,				\
	.access = SNDRV_CTL_ELEM_ACCESS_READ |	\
		SNDRV_CTL_ELEM_ACCESS_VOLATILE,	\
	.info = snd_ctl_boolean_mono_info,	\
	.get = snd_hdspe_get_##prop		\
}

/**
 * HDSPE_RW_BOOL_KCTL - generate a snd_kcontrol_new struct for a 
 * boolean read-write property.
 * @xface: MIXER, CARD, etc...
 * @xname: display name for the property.
 * @prop: source code name for the property. 
 */
#define HDSPE_RW_BOOL_KCTL(xface, xname, prop)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_##xface,	\
	.name = xname,				\
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,	\
	.info = snd_ctl_boolean_mono_info,	\
	.get = snd_hdspe_get_##prop,		\
	.put = snd_hdspe_put_##prop		\
}

/**
 * Get the current value for a property 
 * @kcontrol: control element.
 * @get: getter function - does the real work.
 * @lock_req: protect get() by spinlock, if true.
 * @propname: name of the property, for debugging 
 * Returns what get() returns.
 */
extern int hdspe_control_get(struct snd_kcontrol *kcontrol,
			     int (*get)(struct hdspe*),
			     bool lock_req, const char* propname);

/**
 * Set a property to a new value.
 * @kcontrol: control element.
 * @val: value to set.
 * @get: getter function - gets current value, for change detection.
 * If NULL, change detection is left to put().
 * @put: putter function - does the real work of setting the new value.
 * @lock_req: protect get() and put() by spinlock, if true.
 * @excl_req: return -EBUSY if we have no exclusive access, if true.
 * @propname: name of the property, for debugging 
 * Returns the return code of put() if nonzero. If put() returns zero,
 * returns 1 if the property changed and 0 otherwise.
 */
extern int hdspe_control_put(struct snd_kcontrol *kcontrol, int val,
			     int (*get)(struct hdspe*),
			     int (*put)(struct hdspe*, int val),
			     bool lock_req, bool excl_req, const char* propnam);


/**
 * HDSPE_GETTER: create code for a hdspe_get_<prop>(struct hdspe* hdspe)
 * getter function, simply returns hdspe-><prop>. 
 * @prop: property name, same as struct hdspe int member name.
 */
#define HDSPE_GETTER(prop)				\
static int hdspe_get_##prop(struct hdspe* hdspe)	\
{							\
	return hdspe->prop;				\
}

/**
 * HDSPE_PUTTER: create code for a 
 * hdspe_put_<prop>(struct hdspe* hdspe, int val)
 * putter function, simply setting hdspe-><prop> and returning 1 if
 * its value changed and 0 if not.
 * @prop: property name, same as struct hdspe int member name.
 */
#define HDSPE_PUTTER(prop)					\
static int hdspe_put_##prop(struct hdspe* hdspe, int val)	\
{								\
	int changed = val != hdspe->prop;			\
	hdspe->prop = val;					\
	return changed;						\
}

/**
 * HDSPE_REG_GETTER helpers.
 */
static inline __attribute__((always_inline))
union hdspe_control_reg hdspe_read_control(struct hdspe* hdspe)
{
	return hdspe->reg.control;
}

static inline __attribute__((always_inline))
union hdspe_settings_reg hdspe_read_settings(struct hdspe* hdspe)
{
	return hdspe->reg.settings;
}

/**
 * HDSPE_REG_GETTER: creates code for a hdspe_get_<prop>(struct hdspe* hdspe)
 * getter function, simply reading hdspe->reg.<regname>.<model>.<field>.
 * @prop: property name (defines generated function name).
 * @regname: control, settings, or status0: a field of hdspe->reg.
 * @model: madi, aes or raio: a field in hdspe->reg.regname.
 * @field: a bitfield in hdspe->reg.regname.model.
 * @do_read: if true, read the register from hardware (for status0 register).
 */
#define HDSPE_REG_GETTER(prop, regname, model, field, do_read) \
static int hdspe_get_##prop(struct hdspe* hdspe)			\
{									\
	if (do_read) hdspe_read_##regname(hdspe);			\
	return hdspe->reg.regname.model.field;				\
}

/**
 * HDSPE_REG_PUTTER: creates code for a 
 * hdspe_put_<prop>(struct hdspe* hdspe,int val) putter
 * function, writing hdspe->reg.<regname>.<model>.<field>. if
 * it is not changing.
 * @prop: property name (defines generated function name).
 * @regname: control or settings: a field of hdspe->reg. (status registers
 * do not need putters).
 * @model: madi, aes or raio: a field in hdspe->reg.regname.
 * @field: a bitfield in hdspe->reg.regname.model.
 * Returns 1 if the value has changed and 0 if not.
 */
#define HDSPE_REG_PUTTER(prop, regname, model, field)			\
static int hdspe_put_##prop(struct hdspe* hdspe, int val)		\
{									\
        int oldval = hdspe->reg.regname.model.field;			\
	if (val != oldval) {						\
		hdspe->reg.regname.model.field = val;			\
		hdspe_write_##regname(hdspe);				\
		return 1;						\
	}								\
	return 0;							\
}

/**
 * HDSPE_ENUM_GET_REG - generate code for a snd_kcontrol_new .get method
 * named snd_hdspe_get_<prop>, essentially just reading a bitfield from a 
 * register.
 * @prop: property name (defines generated function name).
 * @regname: control, settings, or status0: a field of hdspe->reg.
 * @model: madi, aes or raio: a field in hdspe->reg.regname.
 * @field: a bitfield in hdspe->reg.regname.model.
 * @lock_req: if nonzero, protect reading by spin_lock_irq(&hdspe->lock).
 * @do_read: if true, read the register from hardware (for status0 register).
 * The generated snd_hdspe_get_<prop> method simply gets the indicated
 * bitfield from the register. 
 */
#define HDSPE_ENUM_GET_REG(prop, regname, model, field, lock_req, do_read) \
	HDSPE_REG_GETTER(prop, regname, model, field, do_read)		\
static int snd_hdspe_get_##prop(struct snd_kcontrol *kcontrol,	        \
				struct snd_ctl_elem_value *ucontrol)	\
{									\
        ucontrol->value.enumerated.item[0] =				\
	hdspe_control_get(kcontrol, hdspe_get_##prop, lock_req, #prop);	\
	return 0;							\
}

/**
 * Same, but using a generic int (*getter)(struct hdspe* hdspe).
 */
#define HDSPE_ENUM_GET(prop, getter, lock_req)				\
static int snd_hdspe_get_##prop(struct snd_kcontrol *kcontrol,	        \
				struct snd_ctl_elem_value *ucontrol)	\
{									\
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);		\
	ucontrol->value.enumerated.item[0] = getter(hdspe);		\
	return 0;							\
}

/**
 * HDSPE_ENUM_PUT_REG - generate code for a snd_kcontrol_new .put method
 * names snd_hdspe_put_<prop>, essentially just writing a bitfield in a 
 * register.
 * @regname: control, settings or status0, one of the fields of 
 * hdspe->reg.
 * @model: madi, aes or raio, a field in hdspe->reg.regname.
 * @field: a bitfield in hdspe->reg.regname.card. 
 * @lock_req: if nonzero, protect writing by spin_lock_irq(&hdspe->lock).
 * @excl_req: if nonzero, return -EBUSY immediately if we have no
 * exclusive control over the card.
 * The generated snd_hdspe_put_<prop> method sets the indicated
 * bitfield in the register and writes the register to hardware if its
 * value has changed.
 * The method returns 1 if the value is changed and 0 if not. 
 */
#define HDSPE_ENUM_PUT_REG(prop, regname, model, field, lock_req, excl_req) \
	HDSPE_REG_PUTTER(prop, regname, model, field)			\
static int snd_hdspe_put_##prop(struct snd_kcontrol *kcontrol,		\
				struct snd_ctl_elem_value *ucontrol)	\
{									\
	return hdspe_control_put(					\
		kcontrol, ucontrol->value.enumerated.item[0],		\
		NULL, hdspe_put_##prop, lock_req, excl_req, #prop);	\
}

/**
 * Same, but using a generic int (*getter)(struct hdspe* hdspe) and
 * void (*putter)(struct hdspe* hdspe, int val).
 */
#define HDSPE_ENUM_PUT(prop, getter, putter, lock_req, excl_req)	\
static int snd_hdspe_put_##prop(struct snd_kcontrol *kcontrol,	        \
				struct snd_ctl_elem_value *ucontrol)	\
{									\
	return hdspe_control_put(					\
		kcontrol, ucontrol->value.enumerated.item[0],		\
		getter, putter, lock_req, excl_req, #prop);		\
}

/**
 * HDSPE_RW_ENUM_REG_METHODS - generates a snd_kcontrol_new .get and .put 
 * method for read-write control elements one-to-one corresponding to a bit 
 * field in the control or settings register.
 */
#define HDSPE_RW_ENUM_REG_METHODS(prop, regname, card, field, excl_req)		\
	HDSPE_ENUM_GET_REG(prop, regname, card, field, true, false) \
	HDSPE_ENUM_PUT_REG(prop, regname, card, field, true, excl_req)

/**
 * Same, using a generic getter and putter.
 */
#define HDSPE_RW_ENUM_METHODS(prop, getter, putter, excl_req) \
	HDSPE_ENUM_GET(prop, getter, true)				\
	HDSPE_ENUM_PUT(prop, getter, putter, true, excl_req)

/**
 * HDSPE_RO_ENUM_REG_METHODS - generates a snd_kcontrol_new .get method 
 * for read-only control elements one-to-one corresponding to a bit 
 * field in a status register.
 */
#define HDSPE_RO_ENUM_REG_METHODS(prop, regname, card, field) \
	HDSPE_ENUM_GET_REG(prop, regname, card, field, false, true)

/**
 * Same, using a generic getter.
 */
#define HDSPE_RO_ENUM_METHODS(prop, getter)			\
	HDSPE_ENUM_GET(prop, getter, false)

/**
 * Same, for single channel integer properties instead of enum.
 */
#define HDSPE_INT1_INFO(prop, minval, maxval, stepval)	  \
static int snd_hdspe_info_##prop(struct snd_kcontrol *kcontrol,	  \
				 struct snd_ctl_elem_info *uinfo) \
{								  \
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;		  \
	uinfo->count = 1;					  \
	uinfo->value.integer.min = minval;			  \
	uinfo->value.integer.max = maxval;			  \
	uinfo->value.integer.step = stepval;			  \
	return 0;						  \
}

#define HDSPE_INT1_GET(prop, getter, lock_req)				\
static int snd_hdspe_get_##prop(struct snd_kcontrol *kcontrol,	        \
				struct snd_ctl_elem_value *ucontrol)	\
{									\
	struct hdspe *hdspe = snd_kcontrol_chip(kcontrol);		\
	ucontrol->value.integer.value[0] = getter(hdspe);		\
	return 0;							\
}

#define HDSPE_INT1_PUT(prop, getter, putter, lock_req, excl_req)	\
static int snd_hdspe_put_##prop(struct snd_kcontrol *kcontrol,	        \
				struct snd_ctl_elem_value *ucontrol)	\
{									\
	return hdspe_control_put(					\
		kcontrol, ucontrol->value.integer.value[0],		\
		getter, putter, lock_req, excl_req, #prop);		\
}

#define HDSPE_RO_INT1_METHODS(prop, min, max, step, getter)	\
	HDSPE_INT1_INFO(prop, min, max, step)			\
	HDSPE_INT1_GET(prop, getter, false)

#define HDSPE_RW_INT1_METHODS(prop, min, max, step, getter, putter, excl_req) \
	HDSPE_INT1_INFO(prop, min, max, step)			\
	HDSPE_INT1_GET(prop, getter, true)		        \
	HDSPE_INT1_PUT(prop, getter, putter, true, excl_req)

/** 
 * HDSPE_RW_INT1_HDSPE_METHODS: create methods for reading/writing
 * the integer struct hdspe member <prop>. 
 */
#define HDSPE_RW_INT1_HDSPE_METHODS(prop, min, max, step)	\
	HDSPE_GETTER(prop)					\
	HDSPE_PUTTER(prop)					\
	HDSPE_RW_INT1_METHODS(prop, min, max, step,		\
			      hdspe_get_##prop, hdspe_put_##prop, false)

#define HDSPE_RO_INT1_HDSPE_METHODS(prop, min, max, step)	\
	HDSPE_GETTER(prop)					\
	HDSPE_RO_INT1_METHODS(prop, min, max, step, hdspe_get_##prop)

#define HDSPE_RO_ENUM_HDSPE_METHODS(prop)			\
	HDSPE_GETTER(prop)					\
	HDSPE_RO_ENUM_METHODS(prop, hdspe_get_##prop)


/**
 * HDSPE_ADD_CONTROL_ID: generates code for adding a control element,
 * storing the created snd_ctl_elem_id* in hdspe->cid.<prop>, for 
 * sending notifications later on.
 * @nctldecl: struct snd_kcontol_new initializer (e.g. HDSPE_RO_KCTL(...))
 * @prop: name of the property, at the same time name of a member
 * of hdspe->cid.
 */
#define HDSPE_ADD_CONTROL_ID(nctldecl, prop)			\
{								\
	const struct snd_kcontrol_new nctl = nctldecl;		\
	int err = hdspe_add_control_id(hdspe, &nctl, &hdspe->cid.prop); \
	if (err < 0)						\
		return err;					\
}

#define HDSPE_ADD_RO_CONTROL_ID(iface, name, prop)		\
	HDSPE_ADD_CONTROL_ID(HDSPE_RO_KCTL(iface, name, prop), prop)

#define HDSPE_ADD_RV_CONTROL_ID(iface, name, prop)		\
	HDSPE_ADD_CONTROL_ID(HDSPE_RV_KCTL(iface, name, prop), prop)

#define HDSPE_ADD_RO_BOOL_CONTROL_ID(iface, name, prop)		\
	HDSPE_ADD_CONTROL_ID(HDSPE_RO_BOOL_KCTL(iface, name, prop), prop)

#define HDSPE_ADD_RV_BOOL_CONTROL_ID(iface, name, prop)		\
	HDSPE_ADD_CONTROL_ID(HDSPE_RV_BOOL_KCTL(iface, name, prop), prop)

#define HDSPE_ADD_RW_CONTROL_ID(iface, name, prop)		\
	HDSPE_ADD_CONTROL_ID(HDSPE_RW_KCTL(iface, name, prop), prop)	

#define HDSPE_ADD_RW_BOOL_CONTROL_ID(iface, name, prop)		\
	HDSPE_ADD_CONTROL_ID(HDSPE_RW_BOOL_KCTL(iface, name, prop), prop)	

	
#endif /* _HDSPE_CONTROL_H_ */
