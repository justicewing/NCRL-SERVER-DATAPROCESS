#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xc6c01fa, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xfcb42cdb, __VMLINUX_SYMBOL_STR(up_read) },
	{ 0xbd100793, __VMLINUX_SYMBOL_STR(cpu_online_mask) },
	{ 0x79aa04a2, __VMLINUX_SYMBOL_STR(get_random_bytes) },
	{ 0x1846825f, __VMLINUX_SYMBOL_STR(netif_carrier_on) },
	{ 0xb7eb0c3d, __VMLINUX_SYMBOL_STR(netif_carrier_off) },
	{ 0x44b1d426, __VMLINUX_SYMBOL_STR(__dynamic_pr_debug) },
	{ 0x7d9cc03b, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0xa779d03a, __VMLINUX_SYMBOL_STR(__put_net) },
	{ 0x75e82745, __VMLINUX_SYMBOL_STR(kthread_create_on_node) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x1b1e54ad, __VMLINUX_SYMBOL_STR(down_read) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0x42a6a019, __VMLINUX_SYMBOL_STR(kthread_bind) },
	{ 0xcd262071, __VMLINUX_SYMBOL_STR(__netdev_alloc_skb) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0x8d657ab9, __VMLINUX_SYMBOL_STR(param_ops_charp) },
	{ 0x11517817, __VMLINUX_SYMBOL_STR(misc_register) },
	{ 0xfb578fc5, __VMLINUX_SYMBOL_STR(memset) },
	{ 0x496cff4d, __VMLINUX_SYMBOL_STR(netif_rx_ni) },
	{ 0x7f36101a, __VMLINUX_SYMBOL_STR(unregister_pernet_subsys) },
	{ 0x4399f900, __VMLINUX_SYMBOL_STR(netif_tx_wake_queue) },
	{ 0x391afe42, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0xaf69df35, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xbfec97cb, __VMLINUX_SYMBOL_STR(kthread_stop) },
	{ 0x11ec5e3, __VMLINUX_SYMBOL_STR(free_netdev) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0x9166fada, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0x62e4100, __VMLINUX_SYMBOL_STR(register_netdev) },
	{ 0x5a921311, __VMLINUX_SYMBOL_STR(strncmp) },
	{ 0xb7964e0, __VMLINUX_SYMBOL_STR(skb_push) },
	{ 0xbf97e500, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x3dacdf, __VMLINUX_SYMBOL_STR(up_write) },
	{ 0xb9bcd2ae, __VMLINUX_SYMBOL_STR(down_write) },
	{ 0xa916b694, __VMLINUX_SYMBOL_STR(strnlen) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xd62c833f, __VMLINUX_SYMBOL_STR(schedule_timeout) },
	{ 0x8c961247, __VMLINUX_SYMBOL_STR(alloc_netdev_mqs) },
	{ 0x17cf49e, __VMLINUX_SYMBOL_STR(eth_type_trans) },
	{ 0x7f24de73, __VMLINUX_SYMBOL_STR(jiffies_to_usecs) },
	{ 0x179ea56, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0xac9b2391, __VMLINUX_SYMBOL_STR(register_pernet_subsys) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0x90339ca9, __VMLINUX_SYMBOL_STR(ether_setup) },
	{ 0xa6bbd805, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(kthread_should_stop) },
	{ 0x2207a57f, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0x9c55cec, __VMLINUX_SYMBOL_STR(schedule_timeout_interruptible) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0xb83399a3, __VMLINUX_SYMBOL_STR(dev_trans_start) },
	{ 0xf08242c2, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0x7cbab064, __VMLINUX_SYMBOL_STR(unregister_netdev) },
	{ 0x420ffece, __VMLINUX_SYMBOL_STR(consume_skb) },
	{ 0x64a71dc1, __VMLINUX_SYMBOL_STR(skb_put) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0x712c3460, __VMLINUX_SYMBOL_STR(misc_deregister) },
	{ 0x5af3a944, __VMLINUX_SYMBOL_STR(__init_rwsem) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "665D0979B9E6E71058B1DE9");
