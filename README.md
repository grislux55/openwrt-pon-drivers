# NNRRWrt Kernel Drivers (`nnrrwrt-drivers`)

Out-of-tree Linux kernel driver feed for [NNRRWrt](https://github.com/grislux55/nnrrwrt), providing Airoha AN7581/AN7583 xPON MAC control, optical frontend SerDes/BOSA drivers, and optical transceiver firmware.

[NNRRWrt](https://github.com/grislux55/nnrrwrt) 的内核驱动软件包源，提供 Airoha AN7581/AN7583 平台的 xPON MAC 控制器、光前端 SerDes/BOSA 驱动及配套光前端微码固件（基于上游 [pbs05/openwrt-pon-drivers](https://github.com/pbs05/openwrt-pon-drivers) 持续维护）。

## Packages / 内核模块包

| Package / 软件包 | Source / 目录 | Purpose / 职责说明 |
| --- | --- | --- |
| `kmod-airoha-xpon` | `airoha-xpon/` | AN7581/AN7583 xPON MAC controller over `airoha_eth` QDMA1, Generic Netlink PLOAM/OMCI/OAM & diagnostics interfaces, and `pon0-*` LED triggers with state replay / xPON MAC 控制器、Netlink 控制与诊断接口及支持状态重放的 `pon0-*` LED 触发器 |
| `kmod-airoha-pon-frontend` | `airoha-pon-frontends/` | Common optical frontend abstraction core (`airoha-pon-frontend-core.ko`) / 光前端公共抽象核心 |
| `kmod-airoha-en7572` | `airoha-pon-frontends/` | Airoha EN7572 optical frontend driver and `A60993` PM/DM firmware (used on Nokia XG-040G-MD) / EN7572 光前端驱动与 `A60993` 固件（Nokia XG-040G-MD 板载方案） |
| `kmod-airoha-paged-bosa` | `airoha-pon-frontends/` | Paged I2C BOSA driver for Semtech GN28L95 and MaxLinear UX3363 / 面向 GN28L95 与 UX3363 的分页 I2C BOSA 驱动 |

## Optical frontends / 光前端

| Device / 器件 | Driver / 驱动 |
| --- | --- |
| EN7572 | `airoha-en7572` |
| GN28L95 | `airoha-paged-bosa` |
| UX3363 | `airoha-paged-bosa` |

## Protocols / 协议

| Mode / 模式 | Status / 状态 |
| --- | --- |
| GPON | ❌ |
| XG-PON | ✅️ |
| XGS-PON | ❓ |
| EPON 1G/1G | ❌ |
| 10G-EPON 10G/1G | ✅️ |
| 10G-EPON 10G/10G | ❓ |

- ✅ tested / 已测试
- ❓ untested / 未测试
- ❌ not yet implemented due to the lack of compatible devices / 我没有找到兼容设备，暂未实现

## OpenWrt Feed Integration / Feed 接入方式

```text
src-git nnrrwrt_drivers https://github.com/grislux55/nnrrwrt-drivers.git
```

## License & Attribution / 许可与致谢

Licensed under `GPL-2.0-only`. Upstream PON MAC and optical frontend drivers by [pbs05](https://github.com/pbs05/openwrt-pon-drivers).
