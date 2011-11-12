date="$(date +%F)"

if [ -x /bin/systemctl ]; then
    /bin/systemctl start sendmail.service
else
    /usr/sbin/service sendmail start
fi

echo -n "Sending report to <$MAILTO>: "
if cat $OUTPUT_ROOT/mail.summary | mail -v -s "[$date] [$RESULT] ABRT testsuite report" -r $MAILFROM -a $OUTPUT_ROOT.tar.gz $MAILTO; then
    echo "OK"
else
    echo "FAILED"
fi
