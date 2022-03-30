#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x5c4cd626, "module_layout" },
	{ 0x2d3385d3, "system_wq" },
	{ 0xc5bca62a, "kmalloc_caches" },
	{ 0xc4f0da12, "ktime_get_with_offset" },
	{ 0xb5b54b34, "_raw_spin_unlock" },
	{ 0xe8b88f8c, "param_ops_int" },
	{ 0x2b68bd2f, "del_timer" },
	{ 0x754d539c, "strlen" },
	{ 0xfd0e606e, "snd_pcm_period_elapsed" },
	{ 0x263ed23b, "__x86_indirect_thunk_r12" },
	{ 0xf2ee5fac, "dma_set_mask" },
	{ 0xb48cc5df, "snd_pcm_hw_constraint_msbits" },
	{ 0xfe903b2a, "pci_disable_device" },
	{ 0xfff5afc, "time64_to_tm" },
	{ 0x3e556aff, "seq_printf" },
	{ 0x56470118, "__warn_printk" },
	{ 0x3c12dfe, "cancel_work_sync" },
	{ 0x39bf9301, "_snd_pcm_hw_param_setempty" },
	{ 0x1f60b17e, "pci_release_regions" },
	{ 0x8e59a22d, "param_ops_bool" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x816cfd6a, "snd_rawmidi_set_ops" },
	{ 0xdd64e639, "strscpy" },
	{ 0x775677fc, "dma_set_coherent_mask" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x340f1fe2, "snd_rawmidi_new" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x999d613a, "param_ops_charp" },
	{ 0x62d5dc27, "pci_set_master" },
	{ 0xbe836042, "_dev_warn" },
	{ 0xfb578fc5, "memset" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0x54ce3499, "current_task" },
	{ 0x55f7d5f8, "snd_pcm_hw_constraint_list" },
	{ 0x9ec6ca96, "ktime_get_real_ts64" },
	{ 0xde80cd09, "ioremap" },
	{ 0x5c993fd7, "snd_pcm_set_ops" },
	{ 0xab8a51b2, "snd_ctl_notify" },
	{ 0xb91e4297, "snd_pcm_hw_constraint_pow2" },
	{ 0x4b750f53, "_raw_spin_unlock_irq" },
	{ 0x62985987, "pci_read_config_word" },
	{ 0xaa0b33da, "snd_hwdep_new" },
	{ 0xebbff79e, "snd_pcm_lib_free_pages" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0xce8b1878, "__x86_indirect_thunk_r14" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0x207529da, "snd_pcm_lib_ioctl" },
	{ 0xdeb29e6e, "_dev_err" },
	{ 0x94098ff8, "snd_interval_list" },
	{ 0x5c86e297, "snd_card_set_id" },
	{ 0x440d1aa9, "snd_ctl_boolean_mono_info" },
	{ 0x5e3e45c3, "snd_pcm_lib_malloc_pages" },
	{ 0x7cb72933, "snd_card_new" },
	{ 0xcc6a729f, "snd_ctl_enum_info" },
	{ 0x6f51809c, "_dev_info" },
	{ 0xa6b2bd8, "snd_pcm_hw_rule_add" },
	{ 0xb601be4c, "__x86_indirect_thunk_rdx" },
	{ 0xa916b694, "strnlen" },
	{ 0xc959d152, "__stack_chk_fail" },
	{ 0xb8b9f817, "kmalloc_order_trace" },
	{ 0x8427cc7b, "_raw_spin_lock_irq" },
	{ 0x395a0453, "snd_ctl_new1" },
	{ 0x2ea2c95c, "__x86_indirect_thunk_rax" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0xeb0c30f0, "pci_unregister_driver" },
	{ 0x90b9d0d7, "snd_pcm_set_sync" },
	{ 0xea34ab69, "kmem_cache_alloc_trace" },
	{ 0xba8fbd64, "_raw_spin_lock" },
	{ 0xb76580fb, "__dynamic_dev_dbg" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0xcd91b127, "system_highpri_wq" },
	{ 0x2414890c, "snd_card_rw_proc_new" },
	{ 0x4cda566, "snd_interval_refine" },
	{ 0x37a0cba, "kfree" },
	{ 0x69acdf38, "memcpy" },
	{ 0x171e64f9, "pci_request_regions" },
	{ 0xe46e126b, "param_array_ops" },
	{ 0x5eaece58, "snd_pcm_hw_constraint_minmax" },
	{ 0xedc03953, "iounmap" },
	{ 0xe223133a, "__pci_register_driver" },
	{ 0x401c9a08, "snd_pcm_lib_preallocate_pages_for_all" },
	{ 0x16078625, "snd_card_free" },
	{ 0x67058b4e, "snd_card_register" },
	{ 0xc5b6f236, "queue_work_on" },
	{ 0x656e4a6e, "snprintf" },
	{ 0xfd25d016, "snd_pcm_new" },
	{ 0x7081a567, "snd_ctl_add" },
	{ 0x7c980197, "snd_rawmidi_transmit" },
	{ 0xa818538b, "pci_enable_device" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x18177e12, "snd_rawmidi_transmit_empty" },
	{ 0xd91e8c4, "snd_rawmidi_receive" },
	{ 0xc1514a3b, "free_irq" },
};

MODULE_INFO(depends, "snd-pcm,snd-rawmidi,snd,snd-hwdep");

MODULE_ALIAS("pci:v000010EEd00003FC6sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00001D18d00003FC6sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "A99F46C6665D4B693DD2455");
