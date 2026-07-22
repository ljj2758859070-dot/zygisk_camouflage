#!/system/bin/sh
MODDIR=${0%/*}
PKG=com.tencent.tmgp.dfm
GAME_DATA=/data/data/$PKG
ACTIVE=0

while true; do
    if pidof $PKG >/dev/null 2>&1; then
        if [ $ACTIVE -eq 0 ];then
            ACTIVE=1
            rm -rf $GAME_DATA/files/hawk_data $GAME_DATA/cache/ano_tmp 2>/dev/null
        fi
    else
        if [ $ACTIVE -eq 1 ];then
            ACTIVE=0
        fi
    fi
    sleep 3
done &
