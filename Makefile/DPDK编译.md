```
# DPDK 路径配置
DPDK_LIB_PATH ?= /usr/local/lib/

# 核心库（必需）
DPDK_CORE_LIBS := \
    eal mbuf mempool ethdev pci ring \
    net hash timer kvargs
# 网络驱动库
DPDK_NET_LIBS := \
    net_e1000 net_ixgbe net_i40e \
    net_bnxt net_mlx5 net_virtio

# 功能库
DPDK_FEATURE_LIBS := \
    acl bbdev bitratestats cryptodev \
    distributor eventdev flow_classify \
    ip_frag pipeline security

# 自动发现可用库
DPDK_AVAILABLE_LIBS := $(notdir $(wildcard $(DPDK_LIB_PATH)librte_*.a))
DPDK_AVAILABLE_LIBS := $(patsubst librte_%.a,%,$(DPDK_AVAILABLE_LIBS))

# 生成链接标志
define add_dpdk_lib
    $(if $(filter $(1),$(DPDK_AVAILABLE_LIBS)),$(DPDK_LIB_PATH)librte_$(1).a)
endef

DPDK_LIB_FILES := \
    $(foreach lib,$(DPDK_CORE_LIBS) $(DPDK_NET_LIBS) $(DPDK_FEATURE_LIBS),\
    $(call add_dpdk_lib,$(lib)))

# 系统库
SYS_LIBS := \
    pthread m dl numa jansson z \
    pcap pfring pcre2-8 yaml net rt

# 最终的链接标志
LDFLAGS := -Wl,--no-undefined \
           -Wl,--start-group \
           -Wl,--whole-archive \
           $(DPDK_LIB_FILES) \
           -Wl,--no-whole-archive \
           $(addprefix -l,$(SYS_LIBS)) \
           -Wl,--end-group \
           $(addprefix -Wl,-rpath ,$(dir $(shell find /usr/local/lib -name "*.so*")))

# 检查必要库
check_required_libs:
	@missing_libs='$(filter-out $(DPDK_AVAILABLE_LIBS),$(DPDK_CORE_LIBS))'; \
	if [ ! -z "$$missing_libs" ]; then \
		echo "Error: Missing required DPDK libraries: $$missing_libs"; \
		exit 1; \
	fi

.PHONY: check_required_libs
```