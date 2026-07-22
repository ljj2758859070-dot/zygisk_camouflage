#!/system/bin/sh
MODDIR=${0%/*}
chmod 0700 "$MODDIR"
chcon u:object_r:system_file:s0 "$MODDIR"
rm -rf "$MODDIR/."* 2>/dev/null
