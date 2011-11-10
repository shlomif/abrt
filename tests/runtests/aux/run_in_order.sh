#!/bin/bash

testlist=$(cat $TEST_LIST | grep '^[^#]\+$')

CTESTFAIL=0
for test_dir in $testlist; do
    test="$test_dir/runtest.sh"
    testname="$(grep 'TEST=\".*\"' $test | awk -F '=' '{ print $2 }' | sed 's/"//g')"
    short_testname=${testname:0:55}

    syslog $short_testname
    mkdir -p "$OUTPUT_ROOT/test/$testname"
    logfile="$OUTPUT_ROOT/test/$testname/full.log"
    TESTNAME=$testname LOGFILE=$logfile $RUNNER_SCRIPT $test > $logfile

    # check test result
    test_result="FAIL"
    if [ -e $OUTPUT_ROOT/TESTOUT-${testname}.log ]; then
        if ! grep -q FAIL $OUTPUT_ROOT/TESTOUT-${testname}.log; then
            test_result="PASS"
        fi
    fi

    # text reporting
    if [ "$test_result" == "FAIL" ]; then
        echo_failure
    else
        echo_success
    fi
    echo " | $short_testname"

    if [ "$test_result" == "FAIL" ]; then
        for ctest in $TEST_CRITICAL; do
            if [ "$test" == "$ctest/runtest.sh" ]; then
                echo_failure
                echo -n " | Critical test failed"
                if [ "$TEST_CONTINUE" = "0" ]; then
                    echo ", stopping further testing"
                    CTESTFAIL=1
                    break
                else
                    echo ", TEST_CONTINUE in effect, resuming testing"
                fi
            fi
        done
        if [ $CTESTFAIL -eq 1 ]; then
            break
        fi
    fi

done

if grep -q FAIL $OUTPUT_ROOT/results; then
    RESULT="FAIL"
else
    RESULT="PASS"
fi

if [ $CTESTFAIL -eq 1 ]; then
    RESULT="FAIL"
fi

export RESULT
