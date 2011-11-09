echo "Mailx disabled"
exit 0

date="$(date +%F)"

tar czf $OUTPUT_ROOT.tar.gz $OUTPUT_ROOT
echo -n "Sending report to <$MAILTO>: "
if cat $OUTPUT_ROOT/abrt-test-output.summary | mail -v -s "[$date] [$RESULT] ABRT testsuite report" -r $MAILFROM -a $OUTPUT_ROOT.tar.gz $MAILTO; then
    echo "OK"
else
    echo "FAILED"
fi
