#!/bin/sh

pid=$1

if ! [[ $pid ]]; then
    {
        echo "Usage: $0 <pid>"
        echo
        echo "Run systemd-coredumpctl to see a list of <pid>s."
    }
    exit 1
fi

. /etc/os-release

mkdir -p /tmp/abrt-adhoc-$pid
cd /tmp/abrt-adhoc-$pid

systemd-coredumpctl dump $pid > coredump
echo -n "CCpp" > analyzer

eval $(journalctl -o verbose COREDUMP_PID=$pid | egrep 'COREDUMP_EXE|COREDUMP_TIMESTAMP|COREDUMP_PID|COREDUMP_UID')

echo $COREDUMP_EXE > executable
echo $(($COREDUMP_TIMESTAMP / 1000000 )) > time
echo $COREDUMP_PID > pid
echo $COREDUMP_UID > uid
echo $PRETTY_NAME > os_release
arch > architecture
echo "$COREDUMP_EXE dumped core" > reason
rpm -qf $COREDUMP_EXE --qf '%{arch}' > pkg_arch

epoch=$(rpm -qf $COREDUMP_EXE --qf '%{epoch}')
if [[ $epoch = *none* ]]; then
    echo 0 > pkg_epoch
else
    echo $epoch > pkg_epoch
fi

rpm -qf $COREDUMP_EXE --qf '%{name}' > pkg_name
rpm -qf $COREDUMP_EXE --qf '%{release}' > pkg_release
rpm -qf $COREDUMP_EXE --qf '%{version}' > pkg_version
rpm -qf $COREDUMP_EXE > package

>environ
>maps
>dso_list

chmod -R 770 /tmp/abrt-adhoc-$pid
chgrp -R abrt /tmp/abrt-adhoc-$pid
chown -R $COREDUMP_UID /tmp/abrt-adhoc-$pid
systemctl start abrtd
DDIR="/var/tmp/abrt/ccpp-$(date -d $(echo -n "@"; echo $(($COREDUMP_TIMESTAMP / 1000000 ))) +"%F-%T")-$pid"
rm -fr $DDIR
mv /tmp/abrt-adhoc-$pid $DDIR
