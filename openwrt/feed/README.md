# Signed opkg feed

GitHub Actions publishes separate package indexes for OpenWrt 24.10.2 on
Raspberry Pi 4 (`aarch64_cortex-a72`) and Raspberry Pi 5
(`aarch64_cortex-a76`).

The generated Pages site contains:

```text
opkg.pub
install.sh
24.10.2/stable/aarch64_cortex-a72/
24.10.2/stable/aarch64_cortex-a76/
```

`Packages` is signed with `usign`. Store the private key only in the GitHub
Actions secret `OPKG_SIGNING_KEY`. Store the matching public key in
`OPKG_SIGNING_PUBLIC_KEY`; the workflow publishes it as `opkg.pub`.

Each architecture index includes the `les-chatd` daemon IPK and the
architecture-independent `luci-app-les-chat` UI IPK.

The installer checks the OpenWrt release and package architecture before it
changes `/etc/opkg/customfeeds.conf`.
