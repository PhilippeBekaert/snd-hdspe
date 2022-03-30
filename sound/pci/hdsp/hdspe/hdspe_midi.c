// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * hdspe_midi.c
 * @brief RME HDSPe MIDI driver.
 *
 * 20210728 - Philippe.Bekaert@uhasselt.be
 *
 * Refactored work of the other MODULE_AUTHORs.
 */

#include "hdspe.h"
#include "hdspe_core.h"

#include <sound/rawmidi.h>

static inline bool hdspe_midi_is_readwrite(struct hdspe_midi *m)
{
	return m->dataOut > 0;
}

static inline unsigned char snd_hdspe_midi_read_byte (struct hdspe *hdspe,
						      int id)
{
	/* the hardware already does the relevant bit-mask with 0xff */
	return hdspe_read(hdspe, hdspe->midi[id].dataIn);
}

static inline void snd_hdspe_midi_write_byte (struct hdspe *hdspe, int id,
					      int val)
{
	/* the hardware already does the relevant bit-mask with 0xff */
	return hdspe_write(hdspe, hdspe->midi[id].dataOut, val);
}

static inline int snd_hdspe_midi_input_available (struct hdspe *hdspe, int id)
{
	return hdspe_read(hdspe, hdspe->midi[id].statusIn) & 0xFF;
}

static inline int snd_hdspe_midi_output_possible (struct hdspe *hdspe, int id)
{
	int fifo_bytes_used;

	fifo_bytes_used = hdspe_read(hdspe, hdspe->midi[id].statusOut) & 0xFF;

	if (fifo_bytes_used < 128)
		return  128 - fifo_bytes_used;
	else
		return 0;
}

static void snd_hdspe_flush_midi_input(struct hdspe *hdspe, int id)
{
	while (snd_hdspe_midi_input_available (hdspe, id))
		snd_hdspe_midi_read_byte (hdspe, id);
}

static int snd_hdspe_midi_output_write (struct hdspe_midi *hmidi)
{
	unsigned long flags;
	int n_pending;
	int to_write;
	int i;
	unsigned char buf[128];

	/* Output is not interrupt driven */

	spin_lock_irqsave (&hmidi->lock, flags);
	if (hmidi->output &&
	    !snd_rawmidi_transmit_empty (hmidi->output)) {
		n_pending = snd_hdspe_midi_output_possible (hmidi->hdspe,
							    hmidi->id);
		if (n_pending > 0) {
			if (n_pending > (int)sizeof (buf))
				n_pending = sizeof (buf);

			to_write = snd_rawmidi_transmit (hmidi->output, buf,
							 n_pending);
			if (to_write > 0) {
				for (i = 0; i < to_write; ++i)
					snd_hdspe_midi_write_byte (hmidi->hdspe,
								   hmidi->id,
								   buf[i]);
			}
		}
	}
	spin_unlock_irqrestore (&hmidi->lock, flags);
	return 0;
}

static int snd_hdspe_midi_input_read (struct hdspe_midi *hmidi)
{
	unsigned char buf[128]; /* this buffer is designed to match the MIDI
				 * input FIFO size
				 */
	unsigned long flags;
	int n_pending;
	int i;

	spin_lock_irqsave (&hmidi->lock, flags);
	n_pending = snd_hdspe_midi_input_available (hmidi->hdspe, hmidi->id);
	while (n_pending > 0) {
		for (i = 0; i < n_pending && i < sizeof(buf); i++)
			buf[i] = snd_hdspe_midi_read_byte (hmidi->hdspe,
							   hmidi->id);

		/* all hdspe MIDI ports are read-write, except for the TCO MTC 
		 * port. MTC messages are either 2 or 10 bytes, so always fit
		 * in the buffer. */
		if (i>0 && !hdspe_midi_is_readwrite(hmidi))
			hdspe_tco_mtc(hmidi->hdspe, buf, i);

		if (hmidi->input)
			snd_rawmidi_receive (hmidi->input, buf, i);
		
		n_pending -= i;
	}
	hmidi->pending = 0;
	spin_unlock_irqrestore(&hmidi->lock, flags);

	/* re-enable MIDI interrupt (was disabled in snd_hdspe_interrupt()) */
	spin_lock_irqsave(&hmidi->hdspe->lock, flags);
	hmidi->hdspe->reg.control.raw |= hmidi->ie;
	hdspe_write_control(hmidi->hdspe);
	spin_unlock_irqrestore(&hmidi->hdspe->lock, flags);

	return snd_hdspe_midi_output_write (hmidi);
}

static void
snd_hdspe_midi_input_trigger(struct snd_rawmidi_substream *substream, int up)
{
	struct hdspe_midi *hmidi = substream->rmidi->private_data;
	struct hdspe *hdspe = hmidi->hdspe;
	bool changed = false;
	unsigned long flags;

	if (!hdspe_midi_is_readwrite(hmidi)) {  /* MTC port is always up. */
		return;
	}

	spin_lock_irqsave (&hdspe->lock, flags);
	if (up) {
		if (!(hdspe->reg.control.raw & hmidi->ie)) {
			snd_hdspe_flush_midi_input (hdspe, hmidi->id);
			hdspe->reg.control.raw |= hmidi->ie;
			hdspe_write_control(hdspe);
			changed = true;
		}
	} else {
		if ((hdspe->reg.control.raw & hmidi->ie) != 0) {
			hdspe->reg.control.raw &= ~hmidi->ie;
			hdspe_write_control(hdspe);
			changed = true;
		}
	}
	spin_unlock_irqrestore (&hdspe->lock, flags);
	return;

	if (changed)
		dev_dbg(hdspe->card->dev,
			"%s: MIDI port %d input %s. IE=0x%08x.\n",
			__func__, hmidi->id, up ? "UP" : "DOWN", hmidi->ie);
}

static void snd_hdspe_midi_output_timer(struct timer_list *t)
{
	struct hdspe_midi *hmidi = from_timer(hmidi, t, timer);
	unsigned long flags;

	snd_hdspe_midi_output_write(hmidi);
	spin_lock_irqsave (&hmidi->lock, flags);

	/* this does not bump hmidi->istimer, because the
	   kernel automatically removed the timer when it
	   expired, and we are now adding it back, thus
	   leaving istimer wherever it was set before.
	*/

	if (hmidi->istimer)
		mod_timer(&hmidi->timer, 1 + jiffies);

	spin_unlock_irqrestore (&hmidi->lock, flags);
}

static void
snd_hdspe_midi_output_trigger(struct snd_rawmidi_substream *substream, int up)
{
	struct hdspe_midi *hmidi;
	unsigned long flags;

	hmidi = substream->rmidi->private_data;
	spin_lock_irqsave (&hmidi->lock, flags);
	if (up) {
		if (!hmidi->istimer) {
			timer_setup(&hmidi->timer,
				    snd_hdspe_midi_output_timer, 0);
			mod_timer(&hmidi->timer, 1 + jiffies);
			hmidi->istimer++;
		}
	} else {
		if (hmidi->istimer && --hmidi->istimer <= 0)
			del_timer (&hmidi->timer);
	}
	spin_unlock_irqrestore (&hmidi->lock, flags);
	if (up)
		snd_hdspe_midi_output_write(hmidi);
	
	dev_dbg(hmidi->hdspe->card->dev,
		"%s: MIDI port %d output %s.\n",
		__func__, hmidi->id, up ? "UP" : "DOWN");
}

static int snd_hdspe_midi_input_open(struct snd_rawmidi_substream *substream)
{
	struct hdspe_midi *hmidi;

	hmidi = substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	snd_hdspe_flush_midi_input (hmidi->hdspe, hmidi->id);
	hmidi->input = substream;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdspe_midi_output_open(struct snd_rawmidi_substream *substream)
{
	struct hdspe_midi *hmidi;

	hmidi = substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->output = substream;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdspe_midi_input_close(struct snd_rawmidi_substream *substream)
{
	struct hdspe_midi *hmidi;

	snd_hdspe_midi_input_trigger (substream, 0);

	hmidi = substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->input = NULL;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static int snd_hdspe_midi_output_close(struct snd_rawmidi_substream *substream)
{
	struct hdspe_midi *hmidi;

	snd_hdspe_midi_output_trigger (substream, 0);

	hmidi = substream->rmidi->private_data;
	spin_lock_irq (&hmidi->lock);
	hmidi->output = NULL;
	spin_unlock_irq (&hmidi->lock);

	return 0;
}

static const struct snd_rawmidi_ops snd_hdspe_midi_output =
{
	.open =		snd_hdspe_midi_output_open,
	.close =	snd_hdspe_midi_output_close,
	.trigger =	snd_hdspe_midi_output_trigger,
};

static const struct snd_rawmidi_ops snd_hdspe_midi_input =
{
	.open =		snd_hdspe_midi_input_open,
	.close =	snd_hdspe_midi_input_close,
	.trigger =	snd_hdspe_midi_input_trigger,
};

void hdspe_init_midi(struct hdspe* hdspe, int count, struct hdspe_midi *list)
{
	int i;
	
	hdspe->midiPorts = count;
	hdspe->midiInterruptEnableMask = 0;
	hdspe->midiIRQPendingMask = 0;

	for (i = 0; i < count; i ++) {
		hdspe->midi[i]           = list[i];
		hdspe->midi[i].hdspe     = hdspe;
		hdspe->midi[i].id        = i;
		hdspe->midiInterruptEnableMask |= hdspe->midi[i].ie;
		hdspe->midiIRQPendingMask      |= hdspe->midi[i].irq;
	}
}

int snd_hdspe_create_midi(struct snd_card* card,
			  struct hdspe *hdspe, int id)
{
	struct hdspe_midi* m = &hdspe->midi[id];
	bool rw = hdspe_midi_is_readwrite(m);
	int err;
	char buf[64];

	spin_lock_init (&m->lock);

	snprintf(buf, sizeof(buf), "%s %s", card->shortname, m->portname);
	err = snd_rawmidi_new(card, buf, id, 1, 1, &m->rmidi);
	if (err < 0)
		return err;
	  
	m->rmidi->private_data = &hdspe->midi[id];

	if (rw) {  // read-write port
		snprintf(m->rmidi->name, sizeof(m->rmidi->name),
			 "%s MIDI %d", card->id, id+1);

		snd_rawmidi_set_ops(m->rmidi,
				    SNDRV_RAWMIDI_STREAM_OUTPUT,
				    &snd_hdspe_midi_output);
		snd_rawmidi_set_ops(m->rmidi,
				    SNDRV_RAWMIDI_STREAM_INPUT,
				    &snd_hdspe_midi_input);
		
		m->rmidi->info_flags |=
			SNDRV_RAWMIDI_INFO_OUTPUT |
			SNDRV_RAWMIDI_INFO_INPUT |
			SNDRV_RAWMIDI_INFO_DUPLEX;
	} else {   // read-only port (MTC from TCO module)
		snprintf(m->rmidi->name, sizeof(m->rmidi->name),
			 "%s MTC", card->id);

		snd_rawmidi_set_ops(m->rmidi,
				    SNDRV_RAWMIDI_STREAM_INPUT,
				    &snd_hdspe_midi_input);
		
		m->rmidi->info_flags |= SNDRV_RAWMIDI_INFO_INPUT;
	}
	dev_dbg(hdspe->card->dev, "%s: %s rawmidi port %d %s created.\n",
		__func__, rw ? "read-write" : "read-only", id,
		m->rmidi->name);

	snd_hdspe_flush_midi_input(hdspe, id);

	return 0;
}

void hdspe_midi_work(struct work_struct *work)
{
	struct hdspe *hdspe = container_of(work, struct hdspe, midi_work);
	int i;

	for (i = 0; i < hdspe->midiPorts; i++) {
		if (hdspe->midi[i].pending)
			snd_hdspe_midi_input_read(&hdspe->midi[i]);
	}
}
