#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# ifeq ($(RTE_SDK),)
# $(error "Please define RTE_SDK environment variable")
# endif

RTE_SDK ?= $(HOME)/dpdk

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-icc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = dataProcess

# all source are stored in SRCS-y
SRCS-y := main.c crc.c bit.c cbsegm.c debug.c turbocoder.c vector.c tc_interl_umts.c tc_interl_lte.c rm_turbo.c MQAMMod.c phy_common.c sequence.c turbodecoder.c turbodecoder_gen.c turbodecoder_sse.c QAM_NDemod.c CalPilotSymb.c CalSymb_mmse.c ChannelEstimator.c RxAdptLink.c ChannelEstimator_LS.c R_Estimator.c vector_simd.c crc_cbsegm.c crc_mod.c packing.c chest_calsym.c derm_crc.c crc_check.c TaskScheduler.c thread_pool.c 

LIBS += -lm -lpthread -lmkl_rt -msse4.1 -fopenmp

CFLAGS += -g -Wall -I . -msse4.1 -O3 -mavx 

CFLAGS += -I$(SRCDIR)
CFLAGS += -O3 $(USER_FLAGS)
CFLAGS += $(WERROR_FLAGS)

include $(RTE_SDK)/mk/rte.extapp.mk
