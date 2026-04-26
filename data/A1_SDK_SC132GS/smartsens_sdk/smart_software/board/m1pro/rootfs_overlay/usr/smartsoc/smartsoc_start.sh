#!/bin/sh

kernel_ver="`uname -r`"
ko_dir="/lib/modules/${kernel_ver}/extra"

# insmod KOs
insmod ${ko_dir}/ddr_mmap.ko
insmod ${ko_dir}/ocm.ko
insmod ${ko_dir}/emb.ko
insmod ${ko_dir}/preoffline.ko
insmod ${ko_dir}/preonpipe.ko
insmod ${ko_dir}/lnpu.ko
insmod ${ko_dir}/osd_kmod.ko
insmod ${ko_dir}/isp_debug.ko
insmod ${ko_dir}/axi_dma.ko
insmod ${ko_dir}/aec.ko
insmod ${ko_dir}/uart_kmod.ko

cd app_demo
./scripts/run.sh
