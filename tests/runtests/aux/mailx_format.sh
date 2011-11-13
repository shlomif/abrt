
pushd $OUTPUT_ROOT

touch 'mail.summary'
if grep -q 'FAIL' 'results'; then
    echo 'Failed tests:' >> 'mail.summary'
    for ft in $( find . -type f -name 'failed' ); do
        dir=$( dirname $ft )
        ts=$( basename $dir )

        echo " - $ts:" >> 'mail.summary'
        grep -i FAIL "$dir/protocol.log" >> 'mail.summary'
        echo "" >> 'mail.summary'
    done
else
    echo 'All tests passed' >> 'mail.summary'
fi

popd

tar czvf $OUTPUT_ROOT.tar.gz $OUTPUT_ROOT
